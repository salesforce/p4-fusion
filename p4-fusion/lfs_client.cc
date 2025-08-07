/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include "lfs_client.h"

#include "openssl/sha.h"
#include <curl/curl.h>
#include <memory>
#include <sstream>
#include <cstdlib>
#include <thread>
#include <chrono>
#include <functional>
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

namespace
{

// Retry configuration
const int MaxRetryAttempts = 3;
const int InitialRetryDelayMs = 1000;

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
			WARN("LFS request failed with HTTP status " << result.response_code << ", retrying (attempt " << (attempt + 1) << "/" << MaxRetryAttempts << ")");
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

void SetupBasicAuth(CURL* curl, const std::string& username, const std::string& password)
{
	if (!username.empty() && !password.empty())
	{
		curl_easy_setopt(curl, CURLOPT_HTTPAUTH, (long)CURLAUTH_BASIC);
		curl_easy_setopt(curl, CURLOPT_USERNAME, username.c_str());
		curl_easy_setopt(curl, CURLOPT_PASSWORD, password.c_str());
	}
}

void SetupRequest(CURL* curl,
    const std::string& url,
    const void* data,
    size_t data_size,
    struct curl_slist* headers,
    RequestResult* pResult,
    const std::string& username = "",
    const std::string& password = "")
{
	SetupBasicAuth(curl, username, password);

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, data_size);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &pResult->response_body);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
}

RequestResult PerformPostRequestWithRetry(CURL* curl,
    const std::string& url,
    const void* data,
    size_t data_size,
    struct curl_slist* headers,
    const std::string& username = "",
    const std::string& password = "")
{
	RequestResult result = {};
	SetupRequest(curl, url, data, data_size, headers, &result, username, password);

	return PerformRequestWithRetry([&]() -> RequestResult
	    {        
        result.curl_result = curl_easy_perform(curl);
        
        if (result.curl_result == CURLE_OK) {
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &result.response_code);
        }
        
        return result; });
}

// Helper function to perform a PUT request with binary data
RequestResult PerformPutRequestWithRetry(CURL* curl, const std::string& url,
    const void* data, size_t data_size,
    struct curl_slist* headers,
    const std::string& username = "",
    const std::string& password = "")
{
	RequestResult result = {};
	SetupRequest(curl, url, data, data_size, headers, &result, username, password);

	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");

	return PerformRequestWithRetry([&]() -> RequestResult
	    {
        result.curl_result = curl_easy_perform(curl);
        
        if (result.curl_result == CURLE_OK) {
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

BatchResponse PerformBatchUploadRequest(const std::string& serverUrl, const std::string& username, const std::string& password, const std::string& oid, size_t fileSize)
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
	auto batchResult = PerformPostRequestWithRetry(curl.get(), batchUrl, payload.data(), payload.size(), headers, username, password);
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

	// Check if verification is required
	if (actions.HasMember("verify") && actions["verify"].IsObject())
	{
		const rapidjson::Value& verifyAction = actions["verify"];
		if (verifyAction.HasMember("href") && verifyAction["href"].IsString())
		{
			result.verifyUrl = verifyAction["href"].GetString();
		}
	}

	result.success = true;
	return result;
}

LFSClient::UploadResult PerformUpload(const std::string& username, const std::string& password, const std::string& uploadUrl, const std::vector<char>& fileContents)
{
	// Initialize curl
	CURLHandle curl;
	if (!curl)
	{
		return LFSClient::UploadResult::Error;
	}

	// Set up headers for file upload
	struct curl_slist* uploadHeaders = nullptr;
	uploadHeaders = curl_slist_append(uploadHeaders, "Content-Type: application/octet-stream");
	uploadHeaders = curl_slist_append(uploadHeaders, "Accept: application/vnd.git-lfs");

	auto uploadResult = PerformPutRequestWithRetry(curl.get(), uploadUrl, fileContents.data(),
	    fileContents.size(), uploadHeaders, username, password);
	curl_slist_free_all(uploadHeaders);

	if (uploadResult.curl_result != CURLE_OK)
	{
		return LFSClient::UploadResult::Error;
	}

	if (uploadResult.response_code >= 200 && uploadResult.response_code < 300)
	{
		return LFSClient::UploadResult::Uploaded;
	}

	return LFSClient::UploadResult::Error;
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

bool PerformVerify(const std::string& username, const std::string& password, const std::string& verifyUrl, const std::string& oid, size_t fileSize)
{
	CURLHandle curl;
	if (!curl)
	{
		return false;
	}

	rapidjson::Document verifyDoc;
	verifyDoc.SetObject();
	rapidjson::Document::AllocatorType& verifyAllocator = verifyDoc.GetAllocator();
	verifyDoc.AddMember("oid", rapidjson::StringRef(oid.c_str()), verifyAllocator);
	verifyDoc.AddMember("size", static_cast<uint64_t>(fileSize), verifyAllocator);

	rapidjson::StringBuffer verifyBuffer;
	rapidjson::Writer<rapidjson::StringBuffer> verifyWriter(verifyBuffer);
	verifyDoc.Accept(verifyWriter);
	std::string verifyPayload = verifyBuffer.GetString();

	struct curl_slist* verifyHeaders = nullptr;
	verifyHeaders = curl_slist_append(verifyHeaders, "Content-Type: application/vnd.git-lfs+json");
	verifyHeaders = curl_slist_append(verifyHeaders, "Accept: application/vnd.git-lfs+json");

	auto verifyResult = PerformPostRequestWithRetry(curl.get(),
	    verifyUrl, verifyPayload.data(),
	    verifyPayload.size(),
	    verifyHeaders,
	    username,
	    password);
	curl_slist_free_all(verifyHeaders);

	return (verifyResult.curl_result == CURLE_OK && verifyResult.response_code == 200);
}

}

LFSClient::LFSClient(GitAPI& gitAPI, const std::string& serverUrl, const std::string& username, const std::string& password, const std::vector<std::string>& lfsPatterns)
    : m_GitAPI(gitAPI)
    , m_ServerUrl(serverUrl)
    , m_Username(username)
    , m_Password(password)
    , m_LFSPatterns(lfsPatterns)
{
	m_LFSPathSpec = m_GitAPI.CreatePathSpec(lfsPatterns);
}

std::vector<char> LFSClient::CreatePointerFileContents(const std::vector<char>& fileContents) const
{
	unsigned char hash[SHA256_DIGEST_LENGTH];
	SHA256(reinterpret_cast<const unsigned char*>(fileContents.data()), fileContents.size(), hash);

	std::string hexHash;
	for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
	{
		char hex[3];
		snprintf(hex, sizeof(hex), "%02x", hash[i]);
		hexHash += hex;
	}

	std::string pointerContent = "version https://git-lfs.github.com/spec/v1\n"
	                             "oid sha256:"
	    + hexHash + "\n"
	                "size "
	    + std::to_string(fileContents.size()) + "\n";

	return std::vector<char>(pointerContent.begin(), pointerContent.end());
}

LFSClient::UploadResult LFSClient::UploadFile(const std::vector<char>& fileContents) const
{
	unsigned char hash[SHA256_DIGEST_LENGTH];
	SHA256(reinterpret_cast<const unsigned char*>(fileContents.data()), fileContents.size(), hash);

	std::string oid;
	for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
	{
		char hex[3];
		snprintf(hex, sizeof(hex), "%02x", hash[i]);
		oid += hex;
	}

	auto batchResponse = PerformBatchUploadRequest(m_ServerUrl, m_Username, m_Password, oid, fileContents.size());
	if (!batchResponse.success)
	{
		return UploadResult::Error;
	}

	if (!batchResponse.needsUpload)
	{
		return UploadResult::AlreadyExists;
	}

	auto uploadResult = PerformUpload(m_Username, m_Password, batchResponse.uploadUrl, fileContents);
	if (uploadResult != UploadResult::Uploaded)
	{
		return uploadResult;
	}

	if (!batchResponse.verifyUrl.empty())
	{
		if (!PerformVerify(m_Username, m_Password, batchResponse.verifyUrl, oid, fileContents.size()))
		{
			return UploadResult::Error;
		}
	}

	return UploadResult::Uploaded;
}

bool LFSClient::IsLFSTracked(const std::string& filePath) const
{
	return git_pathspec_matches_path(m_LFSPathSpec, GIT_PATHSPEC_IGNORE_CASE, filePath.c_str()) == 1;
}

std::vector<char> LFSClient::GetGitAttributesContents() const
{
	std::vector<char> result;
	for (const auto& pattern : m_LFSPatterns)
	{
		const std::string line = pattern + " filter=lfs diff=lfs merge=lfs -text\n";
		result.insert(result.end(), line.begin(), line.end());
	}

	return result;
}

LFSClient::~LFSClient()
{
	m_GitAPI.DestroyPathSpec(m_LFSPathSpec);
}