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

#include "SystemDebugging.h"
#if CONFIG_DBG_HAVE_ENVIRONMENT
#include "env.h"
#endif

using namespace std;

#define LOG_LVL	0

typedef list<struct SystemDebuggingPeer>::iterator PeerIter;

bool SystemDebugging::procTreeDetailed = true;
bool SystemDebugging::procTreeColored = true;
queue<string> SystemDebugging::qLogEntries;

const size_t SystemDebugging::maxPeers = 100;

SystemDebugging::SystemDebugging(Processing *pTreeRoot)
	: Processing("SystemDebugging")
	, mpTreeRoot(pTreeRoot)
	, mListenLocal(false)
	, mUpdateMs(500)
	, mpLstProc(NULL)
	, mpLstLog(NULL)
	, mpLstCmd(NULL)
	, mpLstEnv(NULL)
	, mProcTree("")
	, mProcTreeChanged(false)
	, mProcTreePeerAdded(false)
	, mEnvironment("")
	, mEnvironmentChanged(false)
	, mPortStart(3000)
{
}

/* member functions */
void SystemDebugging::listenLocalSet()
{
	mListenLocal = true;
}

void SystemDebugging::portStartSet(uint16_t port)
{
	mPortStart = port;
}

Success SystemDebugging::initialize()
{
	mPeerList.clear();

	// proc tree
	mpLstProc = TcpListening::create();
	if (!mpLstProc)
		return procErrLog(-1, "could not create process");

	mpLstProc->portSet(mPortStart, mListenLocal);

	start(mpLstProc);
#if CONFIG_PROC_HAVE_LOG
	// log
	mpLstLog = TcpListening::create();
	if (!mpLstLog)
		return procErrLog(-1, "could not create process");

	mpLstLog->portSet(mPortStart + 1, mListenLocal);

	start(mpLstLog);
#endif
	// command
	mpLstCmd = TcpListening::create();
	if (!mpLstCmd)
		return procErrLog(-1, "could not create process");

	mpLstCmd->portSet(mPortStart + 2, mListenLocal);
	mpLstCmd->maxConnSet(4);

	start(mpLstCmd);

	intCmdReg("detailed", &SystemDebugging::procTreeDetailedToggle, "d", "toggle detailed process tree output");
	intCmdReg("colored", &SystemDebugging::procTreeColoredToggle, "c", "toggle colored process tree output");

#if CONFIG_DBG_HAVE_ENVIRONMENT
	mpLstEnv = TcpListening::create();
	if (!mpLstEnv)
		return procErrLog(-1, "could not create process");

	mpLstEnv->portSet(mPortStart + 3);

	start(mpLstEnv);
#endif

	pFctLogEntryCreatedSet(SystemDebugging::logEntryCreated);

	return Positive;
}

Success SystemDebugging::process()
{
	peerListUpdate();

	processTreeSend();
#if CONFIG_PROC_HAVE_LOG
	logEntriesSend();
#endif
#if CONFIG_DBG_HAVE_ENVIRONMENT
	environmentSend();
#endif

	return Pending;
}

Success SystemDebugging::shutdown()
{
	return Positive;
}

void SystemDebugging::peerListUpdate()
{
	peerRemove();
	peerAdd(mpLstProc, PeerProc, "process tree");
#if CONFIG_PROC_HAVE_LOG
	peerAdd(mpLstLog, PeerLog, "log");
#endif
	peerAdd(mpLstCmd, PeerCmd, "command");
#if CONFIG_DBG_HAVE_ENVIRONMENT
	peerAdd(mpLstEnv, PeerEnv, "environment");
#endif
}

void SystemDebugging::peerRemove()
{
	PeerIter iter;
	struct SystemDebuggingPeer peer;
	Processing *pProc;

	iter = mPeerList.begin();
	while (iter != mPeerList.end())
	{
		peer = *iter;
		pProc = peer.pProc;

		if (pProc->success() == Pending)
		{
			++iter;
			continue;
		}

		procDbgLog(LOG_LVL, "removing %s peer. process: %p", peer.typeDesc.c_str(), pProc);
		repel(pProc);

		iter = mPeerList.erase(iter);
	}
}

void SystemDebugging::peerAdd(TcpListening *pListener, enum PeerType peerType, const char *pTypeDesc)
{
	int peerFd;
	Processing *pProc = NULL;
	struct SystemDebuggingPeer peer;

	while (1)
	{
		if (pListener->ppPeerFd.isEmpty())
			break;

		peerFd = pListener->ppPeerFd.front();
		pListener->ppPeerFd.pop();

		if (peerType == PeerCmd)
		{
			pProc = SystemCommanding::create(peerFd);
			if (!pProc)
			{
				procErrLog(-1, "could not create process");
				continue;
			}

			whenFinishedRepel(start(pProc));

			continue;
		}

		pProc = TcpTransfering::create(peerFd);
		if (!pProc)
		{
			procErrLog(-1, "could not create process");
			continue;
		}

		start(pProc);

		procDbgLog(LOG_LVL, "adding %s peer. process: %p", pTypeDesc, pProc);

		peer.type = peerType;
		peer.typeDesc = pTypeDesc;
		peer.pProc = pProc;

		mPeerList.push_back(peer);

		if (peerType == PeerProc)
		{
			mProcTreeChangedTime -= mUpdateMs;
			mProcTreePeerAdded = true;
		}
#if CONFIG_DBG_HAVE_ENVIRONMENT
		else
		if (peerType == PeerEnv)
		{
			mEnvironment = "";
			mEnvironmentChangedTime -= mUpdateMs;
		}
#endif
	}
}

void SystemDebugging::processTreeSend()
{
	if (mProcTreeChanged)
	{
		uint32_t diffMs = nowMs() - mProcTreeChangedTime;

		if (diffMs < mUpdateMs)
			return;

		mProcTreeChanged = false;
	}

	char buffProcTree[1024];

	*buffProcTree = 0;

	mpTreeRoot->processTreeStr(buffProcTree, buffProcTree + sizeof(buffProcTree), procTreeDetailed, procTreeColored);

	string procTree(buffProcTree);

	bool procTreeUpdated = procTree != mProcTree or mProcTreePeerAdded;

	if (!procTreeUpdated)
		return;

	mProcTreePeerAdded = false;

	procDbgLog(LOG_LVL, "process tree changed");
	//procDbgLog(LOG_LVL, "\n%s", procTree.c_str());

	string msg("\033[2J\033[H");

	msg += procTree;

	PeerIter iter;
	struct SystemDebuggingPeer peer;
	TcpTransfering *pTrans = NULL;

	iter = mPeerList.begin();
	while (iter != mPeerList.end())
	{
		peer = *iter++;
		pTrans = (TcpTransfering *)peer.pProc;

		if (!pTrans->mSendReady)
			continue;

		if (peer.type == PeerProc)
			pTrans->send(msg.c_str(), msg.size());
	}

	mProcTree = procTree;

	mProcTreeChanged = true;
	mProcTreeChangedTime = nowMs();
}

#if CONFIG_PROC_HAVE_LOG
void SystemDebugging::logEntriesSend()
{
	string msg;
	PeerIter iter;
	struct SystemDebuggingPeer peer;
	TcpTransfering *pTrans = NULL;

	while (1)
	{
		if (!qLogEntries.size())
			break;

		msg = qLogEntries.front();
		qLogEntries.pop();

		if (!msg.size())
			break;

		msg += "\n";

		iter = mPeerList.begin();
		while (iter != mPeerList.end())
		{
			peer = *iter++;
			pTrans = (TcpTransfering *)peer.pProc;

			if (!pTrans->mSendReady)
				continue;

			if (peer.type == PeerLog)
				pTrans->send(msg.c_str(), msg.size());
		}
	}
}
#endif

#if CONFIG_DBG_HAVE_ENVIRONMENT
void SystemDebugging::environmentSend()
{
	if (mEnvironmentChanged)
	{
		uint32_t diffMs = nowMs() - mEnvironmentChangedTime;

		if (diffMs < mUpdateMs)
			return;

		mEnvironmentChanged = false;
	}

	lock_guard<mutex> lock(envMtx);

	StreamWriterBuilder envBuilder;
	envBuilder["indentation"] = "    ";
	string environment = writeString(envBuilder, env);

	if (environment == mEnvironment)
		return;

	string msg("\033[2J\033[H");

	msg += environment;

	PeerIter iter;
	struct SystemDebuggingPeer peer;
	TcpTransfering *pTrans = NULL;

	iter = mPeerList.begin();
	while (iter != mPeerList.end())
	{
		peer = *iter++;
		pTrans = (TcpTransfering *)peer.pProc;

		if (peer.type == PeerEnv)
			pTrans->send(msg.c_str(), msg.size());
	}

	mEnvironment = environment;

	mEnvironmentChanged = true;
	mEnvironmentChangedTime = nowMs();
}
#endif

void SystemDebugging::processInfo(char *pBuf, char *pBufEnd)
{
	dInfo("Update period [ms]\t\t%d\n", mUpdateMs);
}

/* static functions */
void SystemDebugging::procTreeDetailedToggle(const char *pArgs, char *pBuf, char *pBufEnd)
{
	(void)pArgs;
	(void)pBuf;
	(void)pBufEnd;

	procTreeDetailed = not procTreeDetailed;
}

void SystemDebugging::procTreeColoredToggle(const char *pArgs, char *pBuf, char *pBufEnd)
{
	(void)pArgs;
	(void)pBuf;
	(void)pBufEnd;

#if CONFIG_PROC_USE_DRIVER_COLOR
	procTreeColored = not procTreeColored;
#endif
}

void SystemDebugging::logEntryCreated(
		const int severity,
		const char *filename,
		const char *function,
		const int line,
		const int16_t code,
		const char *msg,
		const uint32_t len)
{
	(void)severity;
	(void)filename;
	(void)function;
	(void)line;
	(void)code;

	qLogEntries.emplace(msg, len);
}

