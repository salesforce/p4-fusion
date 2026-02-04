#include "credentials.h"
#include "lfscomm.h"
#include "lfs/lfs_client.h"
#include <curl/curl.h>
#include <memory>
#include <sstream>
#include <cstdlib>
#include <thread>
#include <chrono>
#include <functional>
#include <map>
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

namespace
{

// Retry configuration
const int MaxRetryAttempts = 3;
const int InitialRetryDelayMs = 1000;

const char* GetUserAgent()
{
	static std::string result = "curl/unknown";
	static std::once_flag init_flag;
	std::call_once(init_flag, []()
	    {
		    curl_version_info_data* version_info = curl_version_info(CURLVERSION_NOW);
		    if (version_info && version_info->version)
		    {
			    result = std::string("curl/") + version_info->version;
		    } });

	return result.c_str();
}

bool ShouldRetryRequest(CURLcode curl_result, long response_code)
{
	if (curl_result != CURLE_OK)
	{
		switch (curl_result)
		{
		case CURLE_COULDNT_CONNECT:
		case CURLE_COULDNT_RESOLVE_HOST:
		case CURLE_COULDNT_RESOLVE_PROXY:
		case CURLE_OPERATION_TIMEDOUT:
		case CURLE_RECV_ERROR:
		case CURLE_SEND_ERROR:
		case CURLE_GOT_NOTHING:
		case CURLE_PARTIAL_FILE:
			return true;
		default:
			return false;
		}
	}

	return (response_code < 200 || response_code >= 300);
}

struct RequestResult
{
	CURLcode curl_result;
	long response_code;
	std::string response_body;
};

// Helper function to perform HTTP requests with retry logic
RequestResult PerformRequestWithRetry(std::function<RequestResult()> requestFunc)
{
	RequestResult result;

	for (size_t attempt = 0; attempt < MaxRetryAttempts; ++attempt)
	{
		result = requestFunc();

		if (!ShouldRetryRequest(result.curl_result, result.response_code))
		{
			break;
		}

		if (result.curl_result != CURLE_OK)
		{
			WARN("LFS request failed with curl error " << result.curl_result << ", retrying (attempt " << (attempt + 1) << "/" << MaxRetryAttempts << ")");
		}
		else
		{
			WARN("LFS request failed with HTTP status " << result.response_code << " and body " << result.response_body << ", retrying (attempt " << (attempt + 1) << "/" << MaxRetryAttempts << ")");
		}

		if (attempt < MaxRetryAttempts - 1)
		{
			int delay_ms = InitialRetryDelayMs * (1 << attempt);
			std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
		}
	}

	return result;
}

class CURLHandle
{
public:
	CURLHandle()
	{
		curl = curl_easy_init();
		if (!curl)
		{
			throw std::runtime_error("Failed to initialize CURL");
		}
	}

	~CURLHandle()
	{
		if (curl)
		{
			curl_easy_cleanup(curl);
		}
	}

	CURL* get() const { return curl; }
	explicit operator bool() const { return curl != nullptr; }

private:
	CURL* curl;
};

// Callback function to write response data
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp)
{
	size_t totalSize = size * nmemb;
	userp->append(static_cast<char*>(contents), totalSize);
	return totalSize;
}

void SetupAuth(CURL* curl, const Credentials& creds)
{
	if (creds.GetType() == Credentials::Type::UserNameAndPass)
	{
		curl_easy_setopt(curl, CURLOPT_HTTPAUTH, (long)CURLAUTH_BASIC);
		curl_easy_setopt(curl, CURLOPT_USERNAME, creds.GetUsername().c_str());
		curl_easy_setopt(curl, CURLOPT_PASSWORD, creds.GetPassword().c_str());
	}
	else if (creds.GetType() == Credentials::Type::Token)
	{
		curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BEARER);
		curl_easy_setopt(curl, CURLOPT_XOAUTH2_BEARER, creds.GetToken().c_str());
	}
}

// Helper function to create curl headers from a map
struct curl_slist* CreateHeadersFromMap(const std::map<std::string, std::string>& headerMap, struct curl_slist* existingHeaders = nullptr)
{
	struct curl_slist* headers = existingHeaders;
	for (const auto& header : headerMap)
	{
		const std::string headerStr = header.first + ": " + header.second;
		headers = curl_slist_append(headers, headerStr.c_str());
	}
	return headers;
}

void SetupRequest(CURL* curl,
    const std::string& url,
    const void* data,
    size_t data_size,
    struct curl_slist* headers,
    RequestResult* pResult,
    const Credentials& creds = Credentials())
{
	SetupAuth(curl, creds);

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, data_size);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &pResult->response_body);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, GetUserAgent());
}

RequestResult PerformPostRequestWithRetry(CURL* curl,
    const std::string& url,
    const void* data,
    size_t data_size,
    struct curl_slist* headers,
    const Credentials& auth = Credentials())
{
	RequestResult result = {};
	SetupRequest(curl, url, data, data_size, headers, &result, auth);

	return PerformRequestWithRetry([&]() -> RequestResult
	    {
		    result.curl_result = curl_easy_perform(curl);

		    if (result.curl_result == CURLE_OK)
		    {
			    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &result.response_code);
		    }

		    return result; });
}

// Helper function to perform a PUT request with binary data
RequestResult PerformPutRequestWithRetry(CURL* curl, const std::string& url,
    const void* data, size_t data_size,
    struct curl_slist* headers,
    const Credentials& auth = Credentials())
{
	RequestResult result = {};
	SetupRequest(curl, url, data, data_size, headers, &result, auth);

	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");

	return PerformRequestWithRetry([&]() -> RequestResult
	    {
		    result.curl_result = curl_easy_perform(curl);

		    if (result.curl_result == CURLE_OK)
		    {
			    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &result.response_code);
		    }

		    return result; });
}

struct BatchResponse
{
	bool success = false;
	std::string uploadUrl;
	std::string verifyUrl;
	bool needsUpload = false;
	std::map<std::string, std::string> uploadHeaders;
	std::map<std::string, std::string> verifyHeaders;
};

std::string CreateBatchUploadPayload(const std::string& oid, size_t fileSize)
{
	rapidjson::Document doc;
	doc.SetObject();
	rapidjson::Document::AllocatorType& allocator = doc.GetAllocator();

	doc.AddMember("operation", "upload", allocator);

	rapidjson::Value transfers(rapidjson::kArrayType);
	transfers.PushBack("basic", allocator);
	doc.AddMember("transfers", transfers, allocator);

	rapidjson::Value objects(rapidjson::kArrayType);
	rapidjson::Value obj(rapidjson::kObjectType);
	obj.AddMember("oid", rapidjson::StringRef(oid.c_str()), allocator);
	obj.AddMember("size", static_cast<uint64_t>(fileSize), allocator);
	objects.PushBack(obj, allocator);
	doc.AddMember("objects", objects, allocator);

	rapidjson::StringBuffer buffer;
	rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
	doc.Accept(writer);
	return buffer.GetString();
}

BatchResponse PerformBatchUploadRequest(const std::string& serverUrl, const Credentials& auth, const std::string& oid, size_t fileSize)
{
	BatchResponse result;

	const std::string payload = CreateBatchUploadPayload(oid, fileSize);

	CURLHandle curl;
	if (!curl)
	{
		return result;
	}

	struct curl_slist* headers = nullptr;
	headers = curl_slist_append(headers, "Content-Type: application/vnd.git-lfs+json");
	headers = curl_slist_append(headers, "Accept: application/vnd.git-lfs+json");

	std::string batchUrl = serverUrl + "/objects/batch";
	auto batchResult = PerformPostRequestWithRetry(curl.get(), batchUrl, payload.data(), payload.size(), headers, auth);
	curl_slist_free_all(headers);

	if (batchResult.curl_result != CURLE_OK || batchResult.response_code != 200)
	{
		return result;
	}

	// Parse response using rapidjson
	rapidjson::Document responseDoc;
	if (responseDoc.Parse(batchResult.response_body.c_str()).HasParseError())
	{
		return result;
	}

	// Check if objects array exists and has our object
	if (!responseDoc.HasMember("objects") || !responseDoc["objects"].IsArray() || responseDoc["objects"].Size() == 0)
	{
		return result;
	}

	const rapidjson::Value& objectsArray = responseDoc["objects"];
	const rapidjson::Value& firstObject = objectsArray[0];

	// Check if upload action is needed
	if (!firstObject.HasMember("actions") || !firstObject["actions"].IsObject())
	{
		// No actions means file already exists
		result.success = true;
		result.needsUpload = false;
		return result;
	}

	const rapidjson::Value& actions = firstObject["actions"];
	if (!actions.HasMember("upload") || !actions["upload"].IsObject())
	{
		// No upload action means file already exists
		result.success = true;
		result.needsUpload = false;
		return result;
	}

	// Extract upload URL from response
	const rapidjson::Value& uploadAction = actions["upload"];
	if (!uploadAction.HasMember("href") || !uploadAction["href"].IsString())
	{
		return result;
	}

	result.uploadUrl = uploadAction["href"].GetString();
	result.needsUpload = true;

	// Extract headers from upload action if present
	if (uploadAction.HasMember("header") && uploadAction["header"].IsObject())
	{
		const rapidjson::Value& headers = uploadAction["header"];
		for (auto it = headers.MemberBegin(); it != headers.MemberEnd(); ++it)
		{
			if (it->value.IsString())
			{
				result.uploadHeaders[it->name.GetString()] = it->value.GetString();
			}
		}
	}

	// Check if verification is required
	if (actions.HasMember("verify") && actions["verify"].IsObject())
	{
		const rapidjson::Value& verifyAction = actions["verify"];
		if (verifyAction.HasMember("href") && verifyAction["href"].IsString())
		{
			result.verifyUrl = verifyAction["href"].GetString();
		}

		// Extract headers from verify action if present
		if (verifyAction.HasMember("header") && verifyAction["header"].IsObject())
		{
			const rapidjson::Value& headers = verifyAction["header"];
			for (auto it = headers.MemberBegin(); it != headers.MemberEnd(); ++it)
			{
				if (it->value.IsString())
				{
					result.verifyHeaders[it->name.GetString()] = it->value.GetString();
				}
			}
		}
	}

	result.success = true;
	return result;
}

Communicator::UploadResult PerformUpload(const std::string& uploadUrl, const std::vector<char>& fileContents, const std::map<std::string, std::string>& actionHeaders, const Credentials& auth = Credentials())
{
	// Initialize curl
	CURLHandle curl;
	if (!curl)
	{
		return Communicator::UploadResult::Error;
	}

	// Set up headers for file upload - start with default headers
	struct curl_slist* uploadHeaders = nullptr;
	// Only add default headers if they're not already present in actionHeaders
	if (actionHeaders.find("Content-Type") == actionHeaders.end())
	{
		uploadHeaders = curl_slist_append(uploadHeaders, "Content-Type: application/octet-stream");
	}
	if (actionHeaders.find("Accept") == actionHeaders.end())
	{
		uploadHeaders = curl_slist_append(uploadHeaders, "Accept: application/vnd.git-lfs");
	}

	// Add action-specific headers from the batch response
	uploadHeaders = CreateHeadersFromMap(actionHeaders, uploadHeaders);

	RequestResult uploadResult = {};
	SetupRequest(curl.get(), uploadUrl, fileContents.data(), fileContents.size(), uploadHeaders, &uploadResult, auth);
	curl_easy_setopt(curl.get(), CURLOPT_CUSTOMREQUEST, "PUT");

	uploadResult = PerformRequestWithRetry([&]() -> RequestResult
	    {
		    uploadResult.curl_result = curl_easy_perform(curl.get());
		    if (uploadResult.curl_result == CURLE_OK)
		    {
			    curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &uploadResult.response_code);
		    }
		    return uploadResult; });

	curl_slist_free_all(uploadHeaders);

	if (uploadResult.curl_result != CURLE_OK)
	{
		return Communicator::UploadResult::Error;
	}

	if (uploadResult.response_code >= 200 && uploadResult.response_code < 300)
	{
		return Communicator::UploadResult::Uploaded;
	}

	return Communicator::UploadResult::Error;
}

std::string CreateVerifyPayload(const std::string& oid, size_t fileSize)
{
	rapidjson::Document verifyDoc;
	verifyDoc.SetObject();
	rapidjson::Document::AllocatorType& verifyAllocator = verifyDoc.GetAllocator();
	verifyDoc.AddMember("oid", rapidjson::StringRef(oid.c_str()), verifyAllocator);
	verifyDoc.AddMember("size", static_cast<uint64_t>(fileSize), verifyAllocator);

	rapidjson::StringBuffer verifyBuffer;
	rapidjson::Writer<rapidjson::StringBuffer> verifyWriter(verifyBuffer);
	verifyDoc.Accept(verifyWriter);

	return verifyBuffer.GetString();
}

bool PerformVerify(const std::string& verifyUrl, const std::string& oid, size_t fileSize, const std::map<std::string, std::string>& actionHeaders, const Credentials& auth = Credentials())
{
	CURLHandle curl;
	if (!curl)
	{
		return false;
	}
	std::string verifyPayload = CreateVerifyPayload(oid, fileSize);
	struct curl_slist* verifyHeaders = nullptr;
	verifyHeaders = curl_slist_append(verifyHeaders, "Content-Type: application/vnd.git-lfs+json");
	verifyHeaders = curl_slist_append(verifyHeaders, "Accept: application/vnd.git-lfs+json");

	// Add action-specific headers from the batch response
	verifyHeaders = CreateHeadersFromMap(actionHeaders, verifyHeaders);

	// Perform request without authentication (headers from action should contain auth)
	RequestResult verifyResult = {};
	SetupRequest(curl.get(), verifyUrl, verifyPayload.data(), verifyPayload.size(), verifyHeaders, &verifyResult, auth);

	verifyResult = PerformRequestWithRetry([&]() -> RequestResult
	    {
		    verifyResult.curl_result = curl_easy_perform(curl.get());
		    if (verifyResult.curl_result == CURLE_OK)
		    {
			    curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &verifyResult.response_code);
		    }
		    return verifyResult; });

	curl_slist_free_all(verifyHeaders);

	return (verifyResult.curl_result == CURLE_OK && verifyResult.response_code == 200);
}

} // namespace

LFSComm::LFSComm(const std::string& serverURL, const Credentials& creds)
    : m_ServerURL(serverURL)
    , m_Creds(creds)
{
}

Communicator::UploadResult LFSComm::UploadFile(const std::vector<char>& fileContents) const
{
	std::string oid = LFSClient::CalcOID(fileContents);

	auto batchResponse = PerformBatchUploadRequest(m_ServerURL, m_Creds, oid, fileContents.size());
	if (!batchResponse.success)
	{
		return UploadResult::Error;
	}

	if (!batchResponse.needsUpload)
	{
		return UploadResult::AlreadyExists;
	}

	auto uploadResult = PerformUpload(batchResponse.uploadUrl, fileContents, batchResponse.uploadHeaders, m_Creds);
	if (uploadResult != UploadResult::Uploaded)
	{
		return uploadResult;
	}

	if (!batchResponse.verifyUrl.empty())
	{
		if (!PerformVerify(batchResponse.verifyUrl, oid, fileContents.size(), batchResponse.verifyHeaders, m_Creds))
		{
			return UploadResult::Error;
		}
	}

	return UploadResult::Uploaded;
}