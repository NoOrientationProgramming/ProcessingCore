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
#include <time.h>
#if CONFIG_PROC_HAVE_LOG
#include "Log.h"
#endif

#include "Processing.h"
#include "TcpListening.h"
#include "TcpTransfering.h"

enum PeerType {
	PeerProc = 0,
#if CONFIG_PROC_HAVE_LOG
	PeerLog,
#endif
	PeerCmd,
#if CONFIG_DBG_HAVE_ENVIRONMENT
	PeerEnv,
#endif
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
	TcpListening *mpLstEnv;

	std::string mProcTree;
	bool mProcTreeChanged;
	clock_t mProcTreeChangedTime;
	bool mProcTreePeerAdded;

	std::string mEnvironment;
	bool mEnvironmentChanged;
	uint16_t mPortStart;
	clock_t mEnvironmentChangedTime;
#if CONFIG_PROC_HAVE_LOG
	Pipe<Json::Value> ppLogEntries;
#endif

	/* static functions */
	static std::string procTreeDetailedToggle(const std::string &args);
	static std::string procTreeColoredToggle(const std::string &args);

	/* static variables */
	static bool procTreeDetailed;
	static bool procTreeColored;

	/* constants */
	static const size_t maxPeers;

};

#endif

