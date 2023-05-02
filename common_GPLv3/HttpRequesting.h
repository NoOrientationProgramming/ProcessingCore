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

#ifndef HTTP_REQUESTING_H
#define HTTP_REQUESTING_H

#include <string>
#include <list>
#include <vector>

#include "Processing.h"
#include "LibDspc.h"

#define numSharedDataTypes		4
#define dHttpDefaultTimeoutMs		2700

#define dHttpResponseCodeOk		200

struct HttpSession
{
	size_t numReferences;
	size_t maxReferences;
	std::string address;
	uint16_t port;
	CURL *pCurlShare;
	std::vector<std::mutex *> sharedDataMtxList;
	std::vector<std::mutex *> sslMtxList;
};

class HttpRequesting : public Processing
{

public:

	static HttpRequesting *create()
	{
		return new (std::nothrow) HttpRequesting;
	}
	static HttpRequesting *create(const std::string &url)
	{
		return new (std::nothrow) HttpRequesting(url);
	}

	void urlSet(const std::string &url);
	void typeSet(const std::string &type);
	void userPwSet(const std::string &userPw);
	void hdrAdd(const std::string &hdr);
	void dataSet(const std::string &data);
	void authMethodSet(const std::string &authMethod);
	void tlsVersionSet(const std::string &tlsVersion);

	uint16_t respCode() const;
	const std::string &respHdr() const;
	const std::string &respData() const;

protected:

	HttpRequesting();
	HttpRequesting(const std::string &url);
	virtual ~HttpRequesting();

private:

	HttpRequesting(const HttpRequesting &) : Processing("") {}
	HttpRequesting &operator=(const HttpRequesting &)
	{
		return *this;
	}

	Success initialize();
	Success process();
	Success easyHandleCreate();
	Success curlEasyHandleBind();
	Success sessionCreate(const std::string &address, const uint16_t port);
	void sessionTerminate();
	void sharedDataMtxListDelete();

	std::string mUrl;
	std::string mType;
	std::string mUserPw;
	std::string mHdr;
	std::string mData;
	std::string mAuthMethod;
	std::string mTlsVersion;

	CURL *mpCurl;
	struct curl_slist *mpHeaderList;

	CURLcode mCurlRes;
	long mRespCode;
	std::string mRespHdr;
	std::string mRespData;

	std::list<HttpSession>::iterator mSession;
	uint8_t mRetries;
	Success mDone;

	static std::mutex mtxCurlMulti;
	static CURLM *pCurlMulti;

	static std::mutex sessionMtx;
	static std::list<HttpSession> sessions;

	static void multiProcess();
	static void curlMultiDeInit();
	static void sharedDataLock(CURL *handle, curl_lock_data data, curl_lock_access access, void *userptr);
	static void sharedDataUnLock(CURL *handle, curl_lock_data data, void *userptr);
	static size_t curlDataToStringWrite(void *ptr, size_t size, size_t nmemb, std::string *pData);

};

#endif

