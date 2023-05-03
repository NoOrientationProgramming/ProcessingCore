/*
  This file is part of the DSP-Crowd project
  https://www.dsp-crowd.com

  Author(s):
      - Johannes Natter, office@dsp-crowd.com

  File created on 03.01.2023

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

#include <iostream>
#include <chrono>
#include <string.h>

#ifdef _WIN32
/* See http://stackoverflow.com/questions/12765743/getaddrinfo-on-win32 */
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501  /* Windows XP. */
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#endif

#include "LibDspc.h"

using namespace std;
using namespace Json;
using namespace chrono;
using namespace CryptoPP;

#define dLenIp4Max		15

const char *hexDigits = "0123456789abcdef";

static mutex mtxCurlGlobal;
static bool curlGlobalInitDone = false;

void curlGlobalInit()
{
	lock_guard<mutex> lock(mtxCurlGlobal);

	if (curlGlobalInitDone)
		return;

	Processing::globalDestructorRegister(curlGlobalDeInit);

	curl_global_init(CURL_GLOBAL_ALL);
	curlGlobalInitDone = true;

	dbgLog(0, "global curl init done");
}

/*
Literature
- https://curl.haxx.se/mail/lib-2016-09/0047.html
- https://stackoverflow.com/questions/29845527/how-to-properly-uninitialize-openssl
- https://wiki.openssl.org/index.php/Library_Initialization
- Wichtig
  - https://rachelbythebay.com/w/2012/12/14/quiet/
*/
void curlGlobalDeInit()
{
	lock_guard<mutex> lock(mtxCurlGlobal);

	if (!curlGlobalInitDone)
		return;

	curl_global_cleanup();
	curlGlobalInitDone = false;

	dbgLog(0, "global curl deinit done");
}

// Debugging

string appVersion()
{
	string err = "<unknown>-yy.mm-n";

	const Resource *pHistory = resourceFind("history_txt");
	if (!pHistory)
		return err;

	const char *pVersStart = strchr(pHistory->pSrc, 'T');
	if (!pVersStart)
		return err;

	const char *pVersEnd = strchr(pVersStart, '\n');
	if (!pVersStart)
		return err;

	size_t len = pVersEnd - pVersStart;
	return string(pVersStart, len);
}

void hexDump(const void *pData, size_t len, size_t colWidth)
{
	const char *pByte = (const char *)pData;
	uint32_t addressAbs = 0;
	char bufLine[256];
	char *pBufEnd = bufLine + sizeof(bufLine);
	char *pBuf = bufLine;
	const char *pLine = pByte;
	uint8_t lenPrinted;
	uint8_t numBytesPerLine = colWidth;
	size_t i = 0;

	while (len)
	{
		pBuf = bufLine;
		*pBuf = 0;
		pLine = pByte;
		lenPrinted = 0;

		dInfo("%08x", addressAbs);

		for (i = 0; i < numBytesPerLine; ++i)
		{
			if (!(i & 7))
				dInfo(" ");

			if (!len)
			{
				dInfo("   ");
				continue;
			}

			dInfo(" %02x", (uint8_t)*pByte);

			++pByte;
			--len;
			++lenPrinted;
		}

		dInfo("  |");

		for (i = 0; i < lenPrinted; ++i, ++pLine, ++pBuf)
		{
			char c = *pLine;

			if (c < 32 or c > 126)
			{
				*pBuf = '.';
				continue;
			}

			*pBuf = c;
		}

		dInfo("|");

		cout << bufLine << endl;

		addressAbs += lenPrinted;
	}
}

string toHexStr(const string &strIn)
{
	string strOut;
	uint8_t ch;

	strOut.reserve(strIn.size() * 2);

	for (size_t i = 0; i < strIn.size(); ++i)
	{
		ch = strIn[i];

		strOut.push_back(hexDigits[ch >> 4]);
		strOut.push_back(hexDigits[ch & 0xF]);
	}

	return strOut;
}

size_t strReplace(string &strIn, const string &strFind, const string &strReplacement)
{
	size_t pos = strIn.find(strFind);

	if (pos == string::npos)
		return pos;

	string strTail = strIn.substr(pos + strFind.size());

	strIn = strIn.substr(0, pos) + strReplacement + strTail;

	return pos;
}

void jsonPrint(const Value &val)
{
	StyledWriter jWriter;
	cout << endl << jWriter.write(val) << endl;
}

// Cryptography

string sha256(const string &msg, const string &prefix)
{
	SHA256 hasher;
	string digest;

	if (prefix.size())
		hasher.Update((const byte *)prefix.data(), prefix.size());

	hasher.Update((const byte *)msg.data(), msg.size());

	digest.resize(hasher.DigestSize());
	hasher.Final((byte *)&digest[0]);

	return digest;
}

string sha256(const SecByteBlock &msg, const string &prefix)
{
	SHA256 hasher;
	string digest;

	if (prefix.size())
		hasher.Update((const byte *)prefix.data(), prefix.size());

	hasher.Update(msg.data(), msg.size());

	digest.resize(hasher.DigestSize());
	hasher.Final((byte *)&digest[0]);

	return digest;
}

bool isValidSha256(const string &digest)
{
	if (digest.size() != (SHA256::DIGESTSIZE << 1))
		return false;

	const char *pCh = digest.c_str();

	for (size_t i = 0; i < digest.size(); ++i, ++pCh)
	{
		if (*pCh >= '0' and *pCh <= '9')
			continue;

		if (*pCh >= 'a' and *pCh <= 'f')
			continue;

		if (*pCh >= 'A' and *pCh <= 'F')
			continue;

		return false;
	}

	return true;
}

// Internet

bool isValidEmail(const string &mail)
{
	size_t pos;

	pos = mail.find('@');
	if (pos == string::npos)
		return false;

	return true;
}

bool isValidIp4(const string &ip)
{
	uint32_t n1, n2, n3, n4;
	int res;

	if (ip.size() > dLenIp4Max)
		return false;

	res = sscanf(ip.c_str(), "%u.%u.%u.%u", &n1, &n2, &n3, &n4);

	if (res != 4)
		return false;

	if (!n1 or (n1 > 255) or (n2 > 255) or (n3 > 255) or (n4 > 255))
		return false;

	return true;
}

string remoteAddr(int socketFd)
{
	struct sockaddr_in addr;
	socklen_t addrLen;

	memset(&addr, 0, sizeof(addr));

	addrLen = sizeof(addr);
	::getpeername(socketFd, (struct sockaddr*)&addr, &addrLen);

	return ::inet_ntoa(addr.sin_addr);
}

// Strings

void strToVecStr(const string &str, VecStr &vStr, char delim)
{
	istringstream ss(str);
	string line;

	while (getline(ss, line, delim))
		vStr.push_back(line);
}

