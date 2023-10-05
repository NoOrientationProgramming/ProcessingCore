/*
  This file is part of the DSP-Crowd project
  https://www.dsp-crowd.com

  Author(s):
      - Johannes Natter, office@dsp-crowd.com

  File created on 12.05.2019

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

#ifndef SYSTEM_DEBUGGING_H
#define SYSTEM_DEBUGGING_H

#ifndef CONFIG_DBG_HAVE_ENVIRONMENT
#define CONFIG_DBG_HAVE_ENVIRONMENT			0
#endif

#include <string>
#include <list>
#include <queue>
#include <time.h>

#include "Processing.h"
#include "TcpListening.h"
#include "TcpTransfering.h"
#include "SystemCommanding.h"

enum PeerType {
	PeerProc = 0,
	PeerLog,
	PeerCmd,
	PeerEnv,
};

struct SystemDebuggingPeer
{
	enum PeerType type;
	std::string typeDesc;
	Processing *pProc;
};

class SystemDebugging : public Processing
{

public:

	static SystemDebugging *create(Processing *pTreeRoot)
	{
		return new (std::nothrow) SystemDebugging(pTreeRoot);
	}

	void listenLocalSet();
	void portStartSet(uint16_t port);

protected:

	SystemDebugging() : Processing("SystemDebugging") {}
	SystemDebugging(Processing *pTreeRoot);
	virtual ~SystemDebugging() {}

private:

	SystemDebugging(const SystemDebugging &) : Processing("") {}
	SystemDebugging &operator=(const SystemDebugging &)
	{
		return *this;
	}

	/*
	 * Naming of functions:  objectVerb()
	 * Example:              peerAdd()
	 */

	/* member functions */
	Success initialize();
	Success process();
	Success shutdown();

	void peerListUpdate();
	bool disconnectRequestedCheck(TcpTransfering *pTrans);
	void peerRemove();
	void peerAdd(TcpListening *pListener, enum PeerType peerType, const char *pTypeDesc);
	void processTreeSend();
#if CONFIG_PROC_HAVE_LOG
	void logEntriesSend();
#endif
#if CONFIG_DBG_HAVE_ENVIRONMENT
	void environmentSend();
#endif

	void processInfo(char *pBuf, char *pBufEnd);

	/* member variables */
	Processing *mpTreeRoot;
	bool mListenLocal;
	std::list<struct SystemDebuggingPeer> mPeerList;
	uint32_t mUpdateMs;

	TcpListening *mpLstProc;
	TcpListening *mpLstLog;
	TcpListening *mpLstCmd;
#if CONFIG_DBG_HAVE_ENVIRONMENT
	TcpListening *mpLstEnv;
	std::string mEnvironment;
	bool mEnvironmentChanged;
	clock_t mEnvironmentChangedTime;
#endif

	std::string mProcTree;
	bool mProcTreeChanged;
	uint32_t mProcTreeChangedTime;
	bool mProcTreePeerAdded;

	uint16_t mPortStart;

	/* static functions */
	static void procTreeDetailedToggle(char *pArgs, char *pBuf, char *pBufEnd);
	static void procTreeColoredToggle(char *pArgs, char *pBuf, char *pBufEnd);
	static void logEntryCreated(
			const int severity,
			const char *filename,
			const char *function,
			const int line,
			const int16_t code,
			const char *msg,
			const size_t len);

	/* static variables */
	static bool procTreeDetailed;
	static bool procTreeColored;
	static std::queue<std::string> qLogEntries;

	/* constants */
	static const size_t maxPeers;

};

#endif

