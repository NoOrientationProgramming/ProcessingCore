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

#ifndef MAIL_SENDING_H
#define MAIL_SENDING_H

#include "Processing.h"
#include "LibDspc.h"

#define dForEach_MailSeState(gen) \
		gen(MailSeStart) \
		gen(MailSeDoneWait) \

#define dGenMailSeStateEnum(s) s,
dProcessStateEnum(MailSeState);

class MailSending : public Processing
{

public:

	static MailSending *create()
	{
		return new (std::nothrow) MailSending;
	}

	void serverSet(const std::string &server);
	void passwordSet(const std::string &password);

	void recipientSet(const std::string &recipient);
	void senderSet(const std::string &sender);

	void subjectSet(const std::string &subject);
	void bodySet(const std::string &body);

protected:

	MailSending();
	virtual ~MailSending();

private:

	MailSending(const MailSending &) : Processing("") {}
	MailSending &operator=(const MailSending &) { return *this; }

	/*
	 * Naming of functions:  objectVerb()
	 * Example:              peerAdd()
	 */

	/* member functions */
	Success process();
	void processInfo(char *pBuf, char *pBufEnd);

	Success easyHandleCreate();
	Success curlEasyHandleBind();

	void nameAddrSplit(const std::string &strIn, std::string &name, std::string &addr);

	/* member variables */
	MailSeState mState;
	uint32_t mStartMs;
	std::string mServer;
	uint16_t mPort;
	std::string mPassword;
	std::string mRecipientAddr;
	std::string mRecipientName;
	std::string mSenderAddr;
	std::string mSenderName;
	std::string mSubject;
	std::string mBody;

	CURL *mpCurl;
	struct curl_slist *mpListRecipients;

	size_t mNumSent;

	Success mDone;
	CURLcode mCurlRes;
	long mRespCode;

	/* static functions */
	static void multiProcess();
	static void curlMultiDeInit();

	static size_t stringToCurlDataRead(char *ptr, size_t size, size_t nmemb, MailSending *pReq);

	/* static variables */
	static std::mutex mtxCurlMulti;
	static CURLM *pCurlMulti;

	/* constants */

};

#endif

