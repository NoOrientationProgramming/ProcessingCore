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

#include "Transfering.h"

class TcpTransfering : public Transfering
{

public:

	static TcpTransfering *create(int fd)
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

protected:

	TcpTransfering(int fd);
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
	void addrInfoSet();

	void processInfo(char *pBuf, char *pBufEnd);

	/* member variables */
	uint32_t mState;
	uint32_t mStartMs;
	std::mutex mSocketFdMtx;
	int mSocketFd;
	std::string mHostAddrStr;
	struct sockaddr_in mHostAddr;
	uint16_t mHostPort;
	int mErrno;
	bool mUsable;
	bool mInfoSet;

	// statistics
	uint32_t mBytesReceived;
	uint32_t mBytesSent;

	/* static functions */

	/* static variables */

	/* constants */

};

#endif

