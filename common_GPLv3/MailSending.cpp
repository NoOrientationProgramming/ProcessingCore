/*
  This file is part of the DSP-Crowd project
  https://www.dsp-crowd.com

  Author(s):
      - Johannes Natter, office@dsp-crowd.com

  File created on 10.01.2023

  Copyright (C) 2023-now Authors and www.dsp-crowd.com

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

#include "MailSending.h"

#if 1
#define dGenMailSeStateString(s) #s,
dProcessStateStr(MailSeState);
#endif

using namespace std;

#define LOG_LVL	0

#define dMailSendTimeoutMs			1000
#define dSmtpCodeActionOkeyCompleted	250

mutex MailSending::mtxCurlMulti;
CURLM *MailSending::pCurlMulti = NULL;

MailSending::MailSending()
	: Processing("MailSending")
	, mState(MailSeStart)
	, mStartMs(0)
	, mServer("")
	, mPort(465)
	, mPassword("")
	, mRecipientAddr("")
	, mRecipientName("")
	, mSenderAddr("")
	, mSenderName("")
	, mSubject("")
	, mBody("")
	, mpCurl(NULL)
	, mpListRecipients(NULL)
	, mNumSent(0)
	, mDone(Pending)
	, mCurlRes(CURLE_OK)
	, mRespCode(0)
{}

MailSending::~MailSending()
{
	if (mpListRecipients)
	{
		curl_slist_free_all(mpListRecipients);
		mpListRecipients = NULL;
	}

	if (mpCurl)
	{
		curl_easy_cleanup(mpCurl);
		mpCurl = NULL;
	}
}

/* member functions */

void MailSending::serverSet(const string &server)
{
	mServer = server;
}

void MailSending::passwordSet(const string &password)
{
	mPassword = password;
}

void MailSending::recipientSet(const string &recipient)
{
	nameAddrSplit(recipient, mRecipientName, mRecipientAddr);
#if 0
	procWrnLog("Rec. addr:  '%s'", mRecipientAddr.c_str());
	procWrnLog("Rec. name:  '%s'", mRecipientName.c_str());
#endif
}

void MailSending::senderSet(const string &sender)
{
	nameAddrSplit(sender, mSenderName, mSenderAddr);
}

void MailSending::subjectSet(const string &subject)
{
	mSubject = subject;
}

void MailSending::bodySet(const string &body)
{
	mBody = body;
}

Success MailSending::process()
{
	uint32_t curTimeMs = millis();
	uint32_t diffMs = curTimeMs - mStartMs;
	Success success;
#if 0
	procWrnLog("mState = %s", MailSeStateString[mState]);
#endif
	switch (mState)
	{
	case MailSeStart:

		curlGlobalInit();

		success = easyHandleCreate();
		if (success != Positive)
			return procErrLog(-1, "could not create curl easy handle");

		success = curlEasyHandleBind();
		if (success != Positive)
			return procErrLog(-1, "could not bind curl easy handle");

		multiProcess();

		mStartMs = curTimeMs;
		mState = MailSeDoneWait;

		break;
	case MailSeDoneWait:

		multiProcess();

		if (diffMs > dMailSendTimeoutMs)
			return procErrLog(-1, "timeout sending mail");

		if (mDone == Pending)
			break;

		if (mCurlRes != CURLE_OK)
			return procErrLog(-1, "curl performing failed: %s (%d)", curl_easy_strerror(mCurlRes), mCurlRes);

		procDbgLog(LOG_LVL, "server returned status code %d", mRespCode);

		if (mRespCode != dSmtpCodeActionOkeyCompleted)
			return procErrLog(-1, "SMTP server did not return %d", dSmtpCodeActionOkeyCompleted);

		return Positive;

		break;
	default:
		break;
	}

	return Pending;
}

/*
Literature
- https://curl.haxx.se/libcurl/c/
- https://curl.haxx.se/libcurl/c/libcurl-easy.html
- https://curl.haxx.se/libcurl/c/multithread.html
- https://curl.haxx.se/libcurl/c/debug.html
- https://curl.se/libcurl/c/CURLOPT_URL.html
- https://curl.se/libcurl/c/CURLOPT_READFUNCTION.html
- https://curl.se/libcurl/c/CURLOPT_READDATA.html
- https://curl.se/libcurl/c/CURLOPT_UPLOAD_BUFFERSIZE.html
*/
Success MailSending::easyHandleCreate()
{
	string strUrl, strUserPwd, strBodyPrefix;
	Success success;

	procDbgLog(LOG_LVL, "Recipient");
	procDbgLog(LOG_LVL, "  Name          = %s", mRecipientName.c_str());
	procDbgLog(LOG_LVL, "  Address       = %s", mRecipientAddr.c_str());
#if 0
	procDbgLog(LOG_LVL, "Sender");
	procDbgLog(LOG_LVL, "  Name          = %s", mSenderName.c_str());
	procDbgLog(LOG_LVL, "  Address       = %s", mSenderAddr.c_str());
#endif
	procDbgLog(LOG_LVL, "Subject         = %s", mSubject.c_str());

	(void)success;

	mpCurl = curl_easy_init();
	if (!mpCurl)
		return procErrLog(-1, "curl_easy_init() returned 0");

	strUrl = "smtps://";
	strUrl += mServer + ":" + to_string(mPort);
	strUrl += "/target";
	curl_easy_setopt(mpCurl, CURLOPT_URL, strUrl.c_str());

	mpListRecipients = NULL;
	mpListRecipients = curl_slist_append(mpListRecipients, mRecipientAddr.c_str());
	curl_easy_setopt(mpCurl, CURLOPT_MAIL_RCPT, mpListRecipients);

	strUserPwd = mSenderAddr + ":" + mPassword;
	curl_easy_setopt(mpCurl, CURLOPT_USERPWD, strUserPwd.c_str());

	curl_easy_setopt(mpCurl, CURLOPT_MAIL_FROM, mSenderAddr.c_str());

	curl_easy_setopt(mpCurl, CURLOPT_USERAGENT, "TGSA");
	curl_easy_setopt(mpCurl, CURLOPT_UPLOAD, 1L);
	curl_easy_setopt(mpCurl, CURLOPT_MAXREDIRS, 50L);
	curl_easy_setopt(mpCurl, CURLOPT_HTTP_VERSION, (long)CURL_HTTP_VERSION_2TLS);
	curl_easy_setopt(mpCurl, CURLOPT_USE_SSL, (long)CURLUSESSL_ALL);
	curl_easy_setopt(mpCurl, CURLOPT_FTP_SKIP_PASV_IP, 1L);
	curl_easy_setopt(mpCurl, CURLOPT_TCP_KEEPALIVE, 1L);
	curl_easy_setopt(mpCurl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)(mBody.size() + 20));
	curl_easy_setopt(mpCurl, CURLOPT_BUFFERSIZE, 59L);
	curl_easy_setopt(mpCurl, CURLOPT_UPLOAD_BUFFERSIZE, 59L); // Not working. Minimum is 16k

	strBodyPrefix += "To: ";
	strBodyPrefix += mRecipientName;
	strBodyPrefix += " <";
	strBodyPrefix += mRecipientAddr;
	strBodyPrefix += ">";
	strBodyPrefix += "\r\n";

	strBodyPrefix += "From: ";
	strBodyPrefix += mSenderName;
	strBodyPrefix += " <";
	strBodyPrefix += mSenderAddr;
	strBodyPrefix += ">";
	strBodyPrefix += "\r\n";

	strBodyPrefix += "Subject: ";
	strBodyPrefix += mSubject;
	strBodyPrefix += "\r\n";
	strBodyPrefix += "\r\n";

	mBody = strBodyPrefix + mBody;

	curl_easy_setopt(mpCurl, CURLOPT_READFUNCTION, MailSending::stringToCurlDataRead);
	curl_easy_setopt(mpCurl, CURLOPT_READDATA, this);

	curl_easy_setopt(mpCurl, CURLOPT_PRIVATE, this);

	//curl_easy_setopt(mpCurl, CURLOPT_VERBOSE, 1L);

	return Positive;
}

Success MailSending::curlEasyHandleBind()
{
	lock_guard<mutex> lock(mtxCurlMulti);

	if (!pCurlMulti)
		pCurlMulti = curl_multi_init();

	if (!pCurlMulti)
		return procErrLog(-1, "could not create curl multi handle");

	CURLMcode code;

	code = curl_multi_add_handle(pCurlMulti, mpCurl);
	if (code != CURLM_OK)
		return procErrLog(-1, "could not bind curl easy handle");

	return Positive;
}

void MailSending::nameAddrSplit(const string &strIn, string &name, string &addr)
{
	size_t pos;

	name = "";

	pos = strIn.find_last_of(' ');
	if (pos != string::npos)
	{
		name = strIn.substr(0, pos);
		++pos;
	}

	if (pos == string::npos)
		pos = 0;

	addr = strIn.substr(pos);
}

void MailSending::processInfo(char *pBuf, char *pBufEnd)
{
#if 1
	dInfo("State\t\t\t%s\n", MailSeStateString[mState]);
#endif
}

/* static functions */

/*
Literature
- https://en.wikipedia.org/wiki/List_of_SMTP_server_return_codes#%E2%80%94_2yz_Positive_completion
*/
void MailSending::multiProcess()
{
	lock_guard<mutex> lock(mtxCurlMulti);

	int numRunningRequests, numMsgsLeft;
	CURLMsg *curlMsg;
	CURL *pCurl;
	MailSending *pReq;

	curl_multi_perform(pCurlMulti, &numRunningRequests);

	while (curlMsg = curl_multi_info_read(pCurlMulti, &numMsgsLeft), curlMsg)
	{
		if (curlMsg->msg != CURLMSG_DONE)
			continue;

		pCurl = curlMsg->easy_handle;

		curl_easy_getinfo(pCurl, CURLINFO_PRIVATE, &pReq);

		pReq->mCurlRes = curlMsg->data.result;
		curl_easy_getinfo(pCurl, CURLINFO_RESPONSE_CODE, &pReq->mRespCode);

		curl_multi_remove_handle(pCurlMulti, pCurl);

		if (pReq->mpListRecipients)
		{
			curl_slist_free_all(pReq->mpListRecipients);
			pReq->mpListRecipients = NULL;
		}

		curl_easy_cleanup(pReq->mpCurl);
		pReq->mpCurl = NULL;

		pReq->mDone = Positive;
	}
}

void MailSending::curlMultiDeInit()
{
	lock_guard<mutex> lock(mtxCurlMulti);

	if (!pCurlMulti)
		return;

	curl_multi_cleanup(pCurlMulti);
	pCurlMulti = NULL;

	dbgLog(0, "MailSending(): multi curl cleanup done");
}

extern "C" size_t MailSending::stringToCurlDataRead(char *ptr, size_t size, size_t nmemb, MailSending *pReq)
{
	size_t numReq = size * nmemb;
	size_t numLeft = pReq->mBody.size() - pReq->mNumSent;
	size_t numRead = MIN(numReq, numLeft);

	if (!numRead)
		return 0;

	const char *pRead = pReq->mBody.c_str() + pReq->mNumSent;

	memcpy(ptr, pRead, numRead);
	pReq->mNumSent += numRead;
#if 0
	wrnLog("Mail data sent: %d", pReq->mNumSent);
#endif
	return numRead;
}

