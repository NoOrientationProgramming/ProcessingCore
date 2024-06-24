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

#ifndef TCP_TRANSFERING_H
#define TCP_TRANSFERING_H

#include <string>

#ifdef _WIN32
// https://learn.microsoft.com/en-us/cpp/porting/modifying-winver-and-win32-winnt?view=msvc-170
#ifndef _WIN32_WINNT
#define _WIN32_WINNT _WIN32_WINNT_WIN10
#endif
#ifndef WINVER
#define WINVER _WIN32_WINNT_WIN10
#endif
#ifndef NTDDI_VERSION
#define NTDDI_VERSION NTDDI_WIN10_19H1
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#endif

/* Literature
 * - https://handsonnetworkprogramming.com/articles/differences-windows-winsock-linux-unix-bsd-sockets-compatibility/
 * - https://handsonnetworkprogramming.com/articles/socket-function-return-value-windows-linux-macos/
 */
#ifndef _WIN32
#ifndef SOCKET
#define SOCKET int
#define INVALID_SOCKET -1
#endif
#endif

#include "Transfering.h"

class TcpTransfering : public Transfering
{

public:

	static TcpTransfering *create(SOCKET fd)
	{
		return new (std::nothrow) TcpTransfering(fd);
	}

	static TcpTransfering *create(const std::string &hostAddr, uint16_t hostPort)
	{
		return new (std::nothrow) TcpTransfering(hostAddr, hostPort);
	}

	ssize_t read(void *pBuf, size_t lenReq);
	ssize_t readFlush();
	ssize_t send(const void *pData, size_t lenReq);
#ifdef _WIN32
	static bool wsaInit();
#endif
	static void sockaddrInfoGet(struct sockaddr_storage &addr,
							std::string &strAddr,
							uint16_t &numPort,
							bool &isIPv6);

protected:

	TcpTransfering(SOCKET fd);
	TcpTransfering(const std::string &hostAddr, uint16_t hostPort);
	virtual ~TcpTransfering() {}

private:

	TcpTransfering() : Transfering("") {}
	TcpTransfering(const TcpTransfering &) : Transfering("") {}
	TcpTransfering &operator=(const TcpTransfering &)
	{
		return *this;
	}

	/*
	 * Naming of functions:  objectVerb()
	 * Example:              peerAdd()
	 */

	/* member functions */
	Success process();
	Success shutdown();

	void disconnect(int err = 0);
	Success socketOptionsSet();
	Success connClientDone();
	void addrInfoSet();
	struct sockaddr_storage *addrStringToSock(const std::string &strAddr, uint16_t numPort);

	int errGet();
	std::string errnoToStr(int num);
	void processInfo(char *pBuf, char *pBufEnd);

	/* member variables */
	uint32_t mStartMs;
#if CONFIG_PROC_HAVE_DRIVERS
	std::mutex mSocketFdMtx;
#endif
	SOCKET mSocketFd;
	std::string mHostAddrStr;
	uint16_t mHostPort;
	struct sockaddr_storage *mpHostAddr;
	int mErrno;
	bool mInfoSet;
	bool mIsIPv6Local;
	bool mIsIPv6Remote;

	// statistics
	size_t mBytesReceived;
	size_t mBytesSent;

	/* static functions */
	static uint32_t millis();
	static bool fileNonBlockingSet(SOCKET fd);
#ifdef _WIN32
	static void globalWsaDestruct();

	/* static variables */
#if CONFIG_PROC_HAVE_DRIVERS
	static std::mutex mtxGlobalInit;
#endif
	static bool globalInitDone;
#endif

	/* constants */

};

#endif

