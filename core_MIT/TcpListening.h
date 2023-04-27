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

#ifndef TCP_LISTENING_H
#define TCP_LISTENING_H

#include <list>
#ifdef _WIN32
#include <winsock2.h>
#else
#include <netinet/in.h>
#endif
#include <mutex>

#include "Processing.h"
#include "Pipe.h"

class TcpListening : public Processing
{

public:

	static TcpListening *create()
	{
		return new (std::nothrow) TcpListening;
	}

	void portSet(size_t port, bool localOnly = false);
	void maxConnSet(size_t maxConn);

	int nextPeerFd();
	Pipe<int> ppPeerFd;

protected:

	TcpListening();
	virtual ~TcpListening() {}

private:

	TcpListening(const TcpListening &) : Processing("") {}
	TcpListening &operator=(const TcpListening &)
	{
		return *this;
	}

	Success initialize();
	Success process();
	Success shutdown();

	void processInfo(char *pBuf, char *pBufEnd);

	size_t mPort;
	size_t mMaxConn;

	int mListeningFd;
	struct sockaddr_in mAddress;

	static std::mutex mtxGlobalInit;
	static bool globalInitDone;
	static void globalWsaDestruct();

	// statistics
	uint32_t mConnCreated;
};

#endif

