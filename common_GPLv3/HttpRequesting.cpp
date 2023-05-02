/*
  This file is part of the DSP-Crowd project
  https://www.dsp-crowd.com

  Author(s):
      - Johannes Natter, office@dsp-crowd.com

  File created on 15.09.2018

  Copyright (C) 2018-now Authors and www.dsp-crowd.com

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include <iostream>
#include <sstream>
#include <iomanip>
#include <regex>
#include "HttpRequesting.h"

#define LOG_LVL			0

//#define ENABLE_CURL_SHARE

using namespace std;

mutex HttpRequesting::mtxCurlMulti;
CURLM *HttpRequesting::pCurlMulti = NULL;

mutex HttpRequesting::sessionMtx;
list<HttpSession> HttpRequesting::sessions;

HttpRequesting::HttpRequesting()
	: Processing("HttpRequesting")
	, mUrl("")
	, mType("get")
	, mUserPw("")
	, mData("")
	, mAuthMethod("basic")
	, mTlsVersion("")
	, mpCurl(NULL)
	, mpHeaderList(NULL)
	, mCurlRes(CURLE_OK)
	, mRespCode(0)
	, mRespHdr("")
	, mRespData("")
	, mRetries(2)
	, mDone(Pending)
{
}

HttpRequesting::HttpRequesting(const string &url)
	: Processing("HttpRequesting")
	, mUrl(url)
	, mType("get")
	, mUserPw("")
	, mData("")
	, mAuthMethod("basic")
	, mTlsVersion("")
	, mpCurl(NULL)
	, mpHeaderList(NULL)
	, mCurlRes(CURLE_OK)
	, mRespCode(0)
	, mRespHdr("")
	, mRespData("")
	, mRetries(2)
	, mDone(Pending)
{
}

/*
 * Literature
 * - https://curl.se/libcurl/c/curl_multi_remove_handle.html
 */
HttpRequesting::~HttpRequesting()
{
	if (mpHeaderList)
	{
		curl_slist_free_all(mpHeaderList);
		mpHeaderList = NULL;
	}

	if (mpCurl)
	{
		Guard lock(mtxCurlMulti);
		CURLMcode code;

		code = curl_multi_remove_handle(pCurlMulti, mpCurl);
		if (code != CURLM_OK)
			procWrnLog("could not unbind curl easy handle");

		curl_easy_cleanup(mpCurl);
		mpCurl = NULL;
	}
}

void HttpRequesting::urlSet(const string &url)
{
	if (!url.size())
		return;

	mUrl = url;
}

void HttpRequesting::typeSet(const string &type)
{
	if (!type.size())
		return;

	mType = type;
}

void HttpRequesting::userPwSet(const string &userPw)
{
	if (!userPw.size())
		return;

	mUserPw = userPw;
}

void HttpRequesting::hdrAdd(const string &hdr)
{
	mHdr = hdr;
}

void HttpRequesting::dataSet(const string &data)
{
	mData = data;
}

void HttpRequesting::authMethodSet(const string &authMethod)
{
	if (!authMethod.size())
		return;

	mAuthMethod = authMethod;
}

void HttpRequesting::tlsVersionSet(const string &tlsVersion)
{
	if (!tlsVersion.size())
		return;

	mTlsVersion = tlsVersion;
}

uint16_t HttpRequesting::respCode() const
{
	return mRespCode;
}

const string &HttpRequesting::respHdr() const
{
	return mRespHdr;
}

const string &HttpRequesting::respData() const
{
	return mRespData;
}

Success HttpRequesting::initialize()
{
	Success success;

	curlGlobalInit();

	success = easyHandleCreate();
	if (success != Positive)
		return procErrLog(-1, "could not create curl easy handle");

	success = curlEasyHandleBind();
	if (success != Positive)
		return procErrLog(-1, "could not bind curl easy handle");

	multiProcess();

	return Positive;
}

/*
Literature
- https://curl.haxx.se/libcurl/c/
- https://curl.se/libcurl/c/curl_multi_add_handle.html
- https://curl.se/libcurl/c/libcurl-errors.html
*/
Success HttpRequesting::curlEasyHandleBind()
{
	Guard lock(mtxCurlMulti);

	if (!pCurlMulti)
		pCurlMulti = curl_multi_init();

	if (!pCurlMulti)
		return procErrLog(-1, "curl multi handle not set");

	CURLMcode code;

	code = curl_multi_add_handle(pCurlMulti, mpCurl);
	if (code != CURLM_OK)
		return procErrLog(-1, "could not bind curl easy handle");

	return Positive;
}

/*
Literature regex
- https://regexr.com/
- https://regex101.com/
- https://www.regular-expressions.info/posixbrackets.html
- http://www.cplusplus.com/reference/regex/ECMAScript/

Literature
- https://curl.haxx.se/libcurl/c/
- https://curl.haxx.se/libcurl/c/libcurl-easy.html
- https://curl.haxx.se/libcurl/c/post-callback.html
- https://curl.haxx.se/libcurl/c/multithread.html
- https://curl.haxx.se/libcurl/c/debug.html
- https://curl.haxx.se/libcurl/c/CURLOPT_URL.html
- https://curl.haxx.se/libcurl/c/CURLOPT_HTTPAUTH.html
- https://curl.haxx.se/libcurl/c/CURLOPT_SSLVERSION.html
- https://curl.haxx.se/libcurl/c/CURLOPT_SSL_VERIFYPEER.html
- https://curl.haxx.se/libcurl/c/CURLOPT_SSL_VERIFYHOST.html
- https://curl.haxx.se/libcurl/c/CURLOPT_CAPATH.html
- https://curl.haxx.se/libcurl/c/CURLOPT_PRIVATE.html
- https://curl.haxx.se/libcurl/c/CURLOPT_SSL_OPTIONS.html
- https://curl.haxx.se/docs/sslcerts.html
- https://curl.haxx.se/mail/archive-2015-05/0006.html
- https://gist.github.com/leprechau/e6b8fef41a153218e1f4
- https://gist.github.com/whoshuu/2dc858b8730079602044
- https://curl.haxx.se/libcurl/c/libcurl-multi.html
  - https://curl.haxx.se/libcurl/c/multi-app.html
- https://curl.haxx.se/mail/lib-2018-12/0011.html
*/
Success HttpRequesting::easyHandleCreate()
{
	string tlsVersion;
	Success success = Positive;

	if (mUrl[4] == 's')
		tlsVersion = "TLSv1.2";

	if (mTlsVersion.size())
		tlsVersion = mTlsVersion;
#if 0
	procDbgLog(LOG_LVL, "url        = %s", mUrl.c_str());
	procDbgLog(LOG_LVL, "type       = %s", mType.c_str());
	procDbgLog(LOG_LVL, "hdr        = %s", mHdr.c_str());
	procDbgLog(LOG_LVL, "data       = %s", mData.c_str());
	procDbgLog(LOG_LVL, "authMethod = %s", mAuthMethod.c_str());
	procDbgLog(LOG_LVL, "tlsVersion = %s", tlsVersion.c_str());
#endif
#ifdef ENABLE_CURL_SHARE
	if (sessionCreate(address, port) != Positive)
		return procErrLog(-1, "could not create session");
#endif

	mpCurl = curl_easy_init();
	if (!mpCurl)
		return procErrLog(-1, "curl_easy_init() returned 0");

	curl_easy_setopt(mpCurl, CURLOPT_URL, mUrl.c_str());

	if (mAuthMethod == "digest")
		curl_easy_setopt(mpCurl, CURLOPT_HTTPAUTH, CURLAUTH_DIGEST); // default: CURLAUTH_BASIC

	if (tlsVersion != "")
	{
		curl_easy_setopt(mpCurl, CURLOPT_SSL_VERIFYPEER, 1L);
		curl_easy_setopt(mpCurl, CURLOPT_SSL_VERIFYHOST, 0L);
		curl_easy_setopt(mpCurl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA | CURLSSLOPT_NO_REVOKE);
		//curl_easy_setopt(mpCurl, CURLOPT_SSL_FALSESTART, 1L); // may safe time => test
	}

	if (tlsVersion == "SSLv2")
		curl_easy_setopt(mpCurl, CURLOPT_SSLVERSION, CURL_SSLVERSION_SSLv2);
	else if (tlsVersion == "SSLv3")
		curl_easy_setopt(mpCurl, CURLOPT_SSLVERSION, CURL_SSLVERSION_SSLv3);
	else if (tlsVersion == "TLSv1")
		curl_easy_setopt(mpCurl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1);
	else if (tlsVersion == "TLSv1.0")
		curl_easy_setopt(mpCurl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_0);
	else if (tlsVersion == "TLSv1.1")
		curl_easy_setopt(mpCurl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_1);
	else if (tlsVersion == "TLSv1.2")
		curl_easy_setopt(mpCurl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);
	else if (tlsVersion == "TLSv1.3")
		curl_easy_setopt(mpCurl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_3);
	else if (tlsVersion != "")
	{
		success = procErrLog(-1, "unknown TLS version");
		goto errCleanupCurl;
	}

	if (mHdr != "")
	{
		stringstream ssHdr(mHdr);
		string hdrToken;

		while (getline(ssHdr, hdrToken, '\n'))
			mpHeaderList = curl_slist_append(mpHeaderList, hdrToken.c_str());

		curl_easy_setopt(mpCurl, CURLOPT_HTTPHEADER, mpHeaderList);
	}

	if (mType == "post" or mType == "put")
		curl_easy_setopt(mpCurl, CURLOPT_POSTFIELDS, mData.c_str());

	if (mUserPw.size())
		curl_easy_setopt(mpCurl, CURLOPT_USERPWD, mUserPw.c_str());

	curl_easy_setopt(mpCurl, CURLOPT_HEADERFUNCTION, HttpRequesting::curlDataToStringWrite);
	curl_easy_setopt(mpCurl, CURLOPT_HEADERDATA, &mRespHdr);

	curl_easy_setopt(mpCurl, CURLOPT_WRITEFUNCTION, HttpRequesting::curlDataToStringWrite);
	curl_easy_setopt(mpCurl, CURLOPT_WRITEDATA, &mRespData);

	curl_easy_setopt(mpCurl, CURLOPT_PRIVATE, this);

#ifdef ENABLE_CURL_SHARE
	curl_easy_setopt(mpCurl, CURLOPT_COOKIEFILE, "");
	curl_easy_setopt(mpCurl, CURLOPT_SHARE, mSession->pCurlShare);
#endif
	//curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

	return Positive;

errCleanupCurl:
	if (mpHeaderList)
	{
		curl_slist_free_all(mpHeaderList);
		mpHeaderList = NULL;
	}

	curl_easy_cleanup(mpCurl);

#ifdef ENABLE_CURL_SHARE
	sessionTerminate();
#endif

	return success;
}

Success HttpRequesting::process()
{
	Success success = Positive;

	multiProcess();

	if (Pending == mDone)
		return Pending;

	if (CURLE_OK != mCurlRes)
		success = procErrLog(-1, "curl performing failed: %s (%d)", curl_easy_strerror(mCurlRes), mCurlRes);

	procDbgLog(LOG_LVL, "server returned status code %d", mRespCode);

	return success;
}

/*
Literature libcurl
- https://curl.haxx.se/libcurl/c/libcurl-share.html
- https://curl.haxx.se/libcurl/c/curl_share_init.html
- https://curl.haxx.se/libcurl/c/CURLOPT_SHARE.html
- https://curl.haxx.se/libcurl/c/curl_share_setopt.html
- https://ec.haxx.se/libcurl-sharing.html
- https://curl.haxx.se/mail/lib-2016-04/0139.html
- https://curl.haxx.se/libcurl/c/example.html
- https://curl.haxx.se/libcurl/c/threaded-shared-conn.html
- https://curl.haxx.se/libcurl/c/threaded-ssl.html
*/
Success HttpRequesting::sessionCreate(const std::string &address, const uint16_t port)
{
	Guard lock(sessionMtx);

	list<HttpSession>::iterator iter;
	bool sessionFound = false;

	procDbgLog(LOG_LVL, "remote socket for session: %s:%d", address.c_str(), port);
	procDbgLog(LOG_LVL, "current number of sessions: %d", sessions.size());

	for (iter = sessions.begin(); iter != sessions.end(); ++iter)
	{
		procDbgLog(LOG_LVL, "%d %s %d", iter->numReferences, iter->address.c_str(), iter->port);

		if (iter->address == address and iter->port == port)
		{
			mSession = iter;
			sessionFound = true;
			break;
		}
	}

	// uncomment if you want to use only one request per session
	// not recommendet
	//sessionFound = false;

	if (sessionFound)
	{
		procDbgLog(LOG_LVL, "reusing existing session");

		++mSession->numReferences;
	} else {
		procDbgLog(LOG_LVL, "no existing session found. Creating");

		HttpSession session;
		sessions.push_front(session);
		mSession = sessions.begin();

		mSession->numReferences = 1;
		mSession->maxReferences = 1;
		mSession->address = address;
		mSession->port = port;

		mSession->sharedDataMtxList.resize(numSharedDataTypes, NULL);

		for (size_t i = 0; i < numSharedDataTypes; ++i)
		{
			mSession->sharedDataMtxList[i] = new (nothrow) mutex;

			if (!mSession->sharedDataMtxList[i])
			{
				sharedDataMtxListDelete();
				sessions.erase(mSession);

				return procErrLog(-1, "could not allocate shared data mutexes for session");
			}
		}

		mSession->pCurlShare = curl_share_init();
		if (!mSession->pCurlShare)
		{
			sharedDataMtxListDelete();
			sessions.erase(mSession);

			return procErrLog(-1, "curl_share_init() returned 0");
		}

		int code = CURLSHE_OK;

		code += curl_share_setopt(mSession->pCurlShare, CURLSHOPT_SHARE, CURL_LOCK_DATA_COOKIE);
		code += curl_share_setopt(mSession->pCurlShare, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
		code += curl_share_setopt(mSession->pCurlShare, CURLSHOPT_SHARE, CURL_LOCK_DATA_SSL_SESSION);
		code += curl_share_setopt(mSession->pCurlShare, CURLSHOPT_SHARE, CURL_LOCK_DATA_CONNECT);

		code += curl_share_setopt(mSession->pCurlShare, CURLSHOPT_USERDATA, &mSession->sharedDataMtxList);
		code += curl_share_setopt(mSession->pCurlShare, CURLSHOPT_LOCKFUNC, HttpRequesting::sharedDataLock);
		code += curl_share_setopt(mSession->pCurlShare, CURLSHOPT_UNLOCKFUNC, HttpRequesting::sharedDataUnLock);

		if (code != CURLSHE_OK)
		{
			curl_share_cleanup(mSession->pCurlShare);
			sharedDataMtxListDelete();
			sessions.erase(mSession);

			return procErrLog(-1, "curl_share_setopt() failed");
		}
	}

	if (mSession->numReferences > mSession->maxReferences)
		mSession->maxReferences = mSession->numReferences;

	procDbgLog(LOG_LVL, "current number of session references: %d", mSession->numReferences);

	return Positive;
}

void HttpRequesting::sessionTerminate()
{
	Guard lock(sessionMtx);

	procDbgLog(LOG_LVL, "dereferencing session: %s:%d", mSession->address.c_str(), mSession->port);

	--mSession->numReferences;
	procDbgLog(LOG_LVL, "%d session references left", mSession->numReferences);

	if (mSession->numReferences)
		return;

	procDbgLog(LOG_LVL, "terminating session. max number of session references were %d", mSession->maxReferences);

	curl_share_cleanup(mSession->pCurlShare);
	sharedDataMtxListDelete();
	sessions.erase(mSession);
}

void HttpRequesting::sharedDataMtxListDelete()
{
	size_t i = 0;
	while (i < numSharedDataTypes and mSession->sharedDataMtxList[i])
		delete mSession->sharedDataMtxList[i++];
}

/*
Literature
- https://curl.haxx.se/libcurl/c/curl_multi_perform.html
- https://curl.haxx.se/libcurl/c/curl_multi_info_read.html
- https://curl.haxx.se/libcurl/c/curl_multi_remove_handle.html
- https://curl.haxx.se/libcurl/c/CURLINFO_RESPONSE_CODE.html
- https://curl.haxx.se/libcurl/c/CURLINFO_PRIVATE.html
*/
void HttpRequesting::multiProcess()
{
	Guard lock(mtxCurlMulti);

	int numRunningRequests, numMsgsLeft;
	CURLMsg *curlMsg;
	CURL *pCurl;
	HttpRequesting *pReq;

	curl_multi_perform(pCurlMulti, &numRunningRequests);

	while (curlMsg = curl_multi_info_read(pCurlMulti, &numMsgsLeft), curlMsg)
	{

		if (CURLMSG_DONE != curlMsg->msg)
			continue;

		pCurl = curlMsg->easy_handle;

		curl_easy_getinfo(pCurl, CURLINFO_PRIVATE, &pReq);

		pReq->mCurlRes = curlMsg->data.result;
		curl_easy_getinfo(pCurl, CURLINFO_RESPONSE_CODE, &pReq->mRespCode);

		curl_multi_remove_handle(pCurlMulti, pCurl);

		if (pReq->mpHeaderList)
		{
			curl_slist_free_all(pReq->mpHeaderList);
			pReq->mpHeaderList = NULL;
		}

		pReq->mpCurl = NULL;
		curl_easy_cleanup(pCurl);

#ifdef ENABLE_CURL_SHARE
		pReq->sessionTerminate();
#endif

		pReq->mDone = Positive;
	}

#if 0
	--mRetries;
	if (mRetries)
	{
		procDbgLog(LOG_LVL, "retries left %d", mRetries);
		success = Pending;
	} else
		procDbgLog(LOG_LVL, "no retry");
#endif
}

/*
Literature
- https://curl.haxx.se/mail/lib-2016-09/0047.html
- https://stackoverflow.com/questions/29845527/how-to-properly-uninitialize-openssl
- https://wiki.openssl.org/index.php/Library_Initialization
- Wichtig
  - https://rachelbythebay.com/w/2012/12/14/quiet/
*/
void HttpRequesting::curlMultiDeInit()
{
	Guard lock(mtxCurlMulti);

	if (!pCurlMulti)
		return;

	curl_multi_cleanup(pCurlMulti);
	pCurlMulti = NULL;

	dbgLog(0, "HttpRequesting(): multi curl cleanup done");
}

extern "C" void HttpRequesting::sharedDataLock(CURL *handle, curl_lock_data data, curl_lock_access access, void *userptr)
{
	int dataIdx = data - 1;

	(void)handle;
	(void)access;

	if (dataIdx < numSharedDataTypes)
		(*((vector<mutex *> *)userptr))[dataIdx]->lock();
	else
		cerr << "curl shared data lock: dataIdx(" << dataIdx << ") >= numSharedDataTypes(4)" << endl;
}

extern "C" void HttpRequesting::sharedDataUnLock(CURL *handle, curl_lock_data data, void *userptr)
{
	int dataIdx = data - 1;

	(void)handle;

	if (dataIdx < numSharedDataTypes)
		(*((vector<mutex *> *)userptr))[dataIdx]->unlock();
	else
		cerr << "curl shared data unlock: dataIdx(" << dataIdx << ") >= numSharedDataTypes(4)" << endl;
}

extern "C" size_t HttpRequesting::curlDataToStringWrite(void *ptr, size_t size, size_t nmemb, string *pData)
{
	pData->append((char *)ptr, size * nmemb);

	return size * nmemb;
}

