/*
  This file is part of the DSP-Crowd project
  https://www.dsp-crowd.com

  Author(s):
      - Johannes Natter, office@dsp-crowd.com

  File created on 21.05.2019

  Copyright (C) 2019-now Authors and www.dsp-crowd.com

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#ifndef _WIN32
#include <unistd.h>
#else
#include "TcpTransfering.h"
#endif
#include "TcpListening.h"

using namespace std;

#define LOG_LVL	0

TcpListening::TcpListening()
	: Processing("TcpListening")
	, mPort(0)
	, mMaxConn(20)
	, mListeningFd(INVALID_SOCKET)
	, mConnCreated(0)
{
	mAddress.sin_family = AF_INET;
}

void TcpListening::portSet(uint16_t port, bool localOnly)
{
	if (localOnly)
		mAddress.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	else
		mAddress.sin_addr.s_addr = htonl(INADDR_ANY);
	mAddress.sin_port = htons(port);

	mPort = port;
}

void TcpListening::maxConnSet(size_t maxConn)
{
	mMaxConn = maxConn;
}

/*
Literature socket programming:
- http://man7.org/linux/man-pages/man2/socket.2.html
- http://man7.org/linux/man-pages/man2/setsockopt.2.html
- http://man7.org/linux/man-pages/man2/bind.2.html
- http://man7.org/linux/man-pages/man2/listen.2.html
- https://docs.microsoft.com/en-us/windows/win32/api/winsock/nf-winsock-wsastartup
*/
Success TcpListening::initialize()
{
	int opt = 1;
	bool ok;

	if (!mPort)
		return procErrLog(-1, "port not set");

#ifdef _WIN32
	ok = TcpTransfering::wsaInit();
	if (!ok)
		return procErrLog(-2, "could not init WSA");
#else
	(void)ok;
#endif
	procDbgLog(LOG_LVL, "creating listening socket");

	mListeningFd = ::socket(mAddress.sin_family, SOCK_STREAM, 0);
	if (mListeningFd == INVALID_SOCKET)
		return procErrLog(-4, "socket() failed: %s", intStrErr(errno).c_str());

	procDbgLog(LOG_LVL, "creating listening socket: done -> %d", mListeningFd);

	// IMPORTANT
	// No need to close socket in case of error
	// This is done in function shutdown()

	if (::setsockopt(mListeningFd, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt)))
		return procErrLog(-5, "setsockopt(SO_REUSEADDR) failed: %s", intStrErr(errno).c_str());

	if (::bind(mListeningFd, (struct sockaddr *)&mAddress, sizeof(mAddress)) < 0)
		return procErrLog(-6, "bind() failed: %s", intStrErr(errno).c_str());

	if (::listen(mListeningFd, 3) < 0)
		return procErrLog(-7, "listen() failed: %s", intStrErr(errno).c_str());

	return Positive;
}

/*
Literature socket programming:
- http://man7.org/linux/man-pages/man2/poll.2.html
- http://man7.org/linux/man-pages/man2/accept.2.html
*/
Success TcpListening::process()
{
	ppPeerFd.toPushTry();

	if (ppPeerFd.isFull() or ppPeerFd.size() >= mMaxConn)
		return Pending;

	fd_set rfds;
	struct timeval tv;
	int res;

	FD_ZERO(&rfds);
	FD_SET(mListeningFd, &rfds);

	tv.tv_sec = 0;
	tv.tv_usec = 1000;
#ifdef _WIN32
	res = ::select(0, &rfds, NULL, NULL, &tv);
#else
	res = ::select(mListeningFd + 1, &rfds, NULL, NULL, &tv);
#endif
	if (res < 0)
	{
		if (errno == EINTR)
		{
			procDbgLog(LOG_LVL, "select() failed: %s", intStrErr(errno).c_str());
			return Positive;
		}

		return procErrLog(-1, "select() failed: %s", intStrErr(errno).c_str());
	}

	if (!res) // timeout ok
		return Pending;

	procDbgLog(LOG_LVL, "listening socket has data");

	SOCKET peerSocketFd;
	struct sockaddr_in address = mAddress;
	socklen_t addrLen = sizeof(address);

	peerSocketFd = ::accept(mListeningFd, (struct sockaddr *)&address, &addrLen);
	if (peerSocketFd == INVALID_SOCKET)
	{
		procWrnLog("accept() failed: %s", intStrErr(errno).c_str());

		return Pending;
	}

	procDbgLog(LOG_LVL, "got socket fd -> %d", peerSocketFd);

	ppPeerFd.commit(peerSocketFd, nowMs());
	++mConnCreated;

	return Pending;
}

Success TcpListening::shutdown()
{
	SOCKET peerFd;

	while (!ppPeerFd.isEmpty())
	{
		peerFd = ppPeerFd.front();
		ppPeerFd.pop();

		procDbgLog(LOG_LVL, "closing unused peer socket %d", peerFd);
#ifdef _WIN32
		::closesocket(peerFd);
#else
		::close(peerFd);
#endif
		peerFd = INVALID_SOCKET;
		procDbgLog(LOG_LVL, "closing unused peer socket %d: done", peerFd);
	}

	if (mListeningFd != INVALID_SOCKET)
	{
		procDbgLog(LOG_LVL, "closing listening socket %d", mListeningFd);
#ifdef _WIN32
		::closesocket(mListeningFd);
#else
		::close(mListeningFd);
#endif
		mListeningFd = INVALID_SOCKET;
		procDbgLog(LOG_LVL, "closing listening socket %d: done", mListeningFd);
	}

	return Positive;
}

string TcpListening::intStrErr(int num)
{
	char buf[64];
	size_t len = sizeof(buf) - 1;

	buf[0] = 0;
	buf[len] = 0;

#ifdef _WIN32
	::strerror_s(buf, len, num);
#else
	::strerror_r(num, buf, len);
#endif
	return string(buf);
}

/* Literature
 * - https://linux.die.net/man/3/inet_ntoa
 * - https://man7.org/linux/man-pages/man3/inet_ntop.3.html
 * - https://linux.die.net/man/3/htons
 */
void TcpListening::processInfo(char *pBuf, char *pBufEnd)
{
	if (!mPort)
		return;

	char buf[128];
	size_t len = sizeof(buf) - 1;

	buf[0] = 0;
	buf[len] = 0;

	::inet_ntop(mAddress.sin_family, &mAddress.sin_addr, buf, len);

	dInfo("%s:%d\n", buf, ::ntohs(mAddress.sin_port));
	dInfo("Connections created:\t%d\n", mConnCreated);
}

