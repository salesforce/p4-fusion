#include "s3comm.h"
#include "lfs/lfs_client.h"
#include <aws/core/Aws.h>
#include <aws/core/Region.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/PutObjectRequest.h>
#include <aws/s3/model/HeadObjectRequest.h>
#include <aws/core/auth/AWSCredentials.h>
#include <aws/s3/S3ClientConfiguration.h>
#include <aws/core/utils/stream/PreallocatedStreamBuf.h>

S3Comm::S3Comm(const std::string& serverURL, const std::string& bucket, const std::string& repository, const std::string& username, const std::string& password)
    : m_ServerURL(serverURL)
    , m_Bucket(bucket)
    , m_Repository(repository)
    , m_Username(username)
    , m_Password(password)
{
}

Communicator::UploadResult S3Comm::UploadFile(const std::vector<char>& fileContents) const
{
	std::string oid = LFSClient::CalcOID(fileContents);

	Aws::Auth::AWSCredentials credentials(m_Username, m_Password);

	bool isHttps = m_ServerURL.find("https://") == 0;

	Aws::S3::S3ClientConfiguration config;
	config.endpointOverride = m_ServerURL;
	config.scheme = isHttps ? Aws::Http::Scheme::HTTPS : Aws::Http::Scheme::HTTP;
	config.verifySSL = isHttps;
	config.useVirtualAddressing = false;

	Aws::S3::S3Client s3Client(credentials, config,
	    Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Never, false);

	std::string objectKey = m_Repository + "/" + oid;

	//Check if object already exists
	Aws::S3::Model::HeadObjectRequest headRequest;
	headRequest.SetBucket(m_Bucket);
	headRequest.SetKey(objectKey);

	auto headResult = s3Client.HeadObject(headRequest);
	if (headResult.IsSuccess())
	{
		return UploadResult::AlreadyExists;
	}

	Aws::S3::Model::PutObjectRequest objectRequest;
	objectRequest.SetBucket(m_Bucket);
	objectRequest.SetKey(objectKey);

	auto streamBuf = Aws::MakeShared<Aws::Utils::Stream::PreallocatedStreamBuf>("",
	    reinterpret_cast<unsigned char*>(const_cast<char*>(fileContents.data())),
	    fileContents.size());
	auto stream = Aws::MakeShared<Aws::IOStream>("", streamBuf.get());
	stream->seekg(0, std::ios::beg);

	objectRequest.SetBody(stream);

	auto uploadResult = s3Client.PutObject(objectRequest);
	return uploadResult.IsSuccess() ? UploadResult::Uploaded : UploadResult::Error;
}
