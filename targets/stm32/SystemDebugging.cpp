/*
  This file is part of the DSP-Crowd project
  https://www.dsp-crowd.com

  Author(s):
      - Johannes Natter, office@dsp-crowd.com

  File created on 30.11.2019

  Copyright (C) 2019, Johannes Natter

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

#include <string.h>

#include "SystemDebugging.h"

#define dForEach_ProcState(gen) \
		gen(StStart) \
		gen(StSendReadyWait) \
		gen(StMain) \

#define dGenProcStateEnum(s) s,
dProcessStateEnum(ProcState);

#if 0
#define dGenProcStateString(s) #s,
dProcessStateStr(ProcState);
#endif

#define dForEach_CmdState(gen) \
		gen(StCmdRcvdWait) \
		gen(StCmdInterpret) \
		gen(StCmdSendStart) \
		gen(StCmdSentWait) \

#define dGenCmdStateEnum(s) s,
dProcessStateEnum(CmdState);

#if 0
#define dGenCmdStateString(s) #s,
dProcessStateStr(CmdState);
#endif

using namespace std;

#define CMD(x)		(!strncmp(pSwt->mBufInCmd, x, strlen(x)))

#ifndef dKeyModeDebug
#define dKeyModeDebug "aaaaa"
#endif

const uint16_t cCntDelayMin = 5000;

static SingleWireTransfering *pSwt = NULL;
#if CONFIG_PROC_HAVE_DRIVERS
static mutex mtxLogEntries;
#endif
static int levelLog = 3;
static bool logOvf = false;
static uint16_t idxInfo = 0;

#define dNumCmds		23
Command commands[dNumCmds] = {};

SystemDebugging::SystemDebugging(Processing *pTreeRoot)
	: Processing("SystemDebugging")
	, mpTreeRoot(pTreeRoot)
	, mpSend(NULL)
	, mpUser(NULL)
	, mReady(false)
	, mStateCmd(StCmdRcvdWait)
	, mCntDelay(0)
{
	mState = StStart;
}

/* member functions */

void SystemDebugging::fctDataSendSet(FuncDataSend pFct, void *pUser)
{
	mpSend = pFct;
	mpUser = pUser;
}

void SystemDebugging::dataReceived(char *pData, size_t len)
{
	if (!pSwt)
		return;

	pSwt->dataReceived(pData, len);
}

void SystemDebugging::dataSent()
{
	if (!pSwt)
		return;

	pSwt->dataSent();
}

bool SystemDebugging::ready()
{
	return mReady;
}

bool SystemDebugging::logOverflowed()
{
	return logOvf;
}

Command *freeCmdStructGet()
{
	Command *pCmd = commands;

	for (size_t i = 0; i < dNumCmds; ++i, ++pCmd)
	{
		if (pCmd->pId && pCmd->pFctExec)
			continue;

		return pCmd;
	}

	return NULL;
}

bool cmdReg(
		const char *pId,
		FuncCommand pFct,
		const char *pShortcut,
		const char *pDesc,
		const char *pGroup)
{
	Command *pCmd = freeCmdStructGet();

	if (!pCmd)
	{
		errLog(-1, "Max registered commands reached");
		return false;
	}

	pCmd->pId = pId;
	pCmd->pFctExec = pFct;
	pCmd->pShortcut = pShortcut;
	pCmd->pDesc = pDesc;
	pCmd->pGroup = pGroup;

	infLog("Registered command '%s'", pId);

	return true;
}

void SystemDebugging::levelLogSet(int lvl)
{
	levelLog = lvl;
}

Success SystemDebugging::process()
{
	switch (mState)
	{
	case StStart:

		if (!mpTreeRoot)
			return procErrLog(-1, "tree root not set");

		if (!mpSend)
			return procErrLog(-1, "send function not set");

		pSwt = SingleWireTransfering::create();
		if (!pSwt)
			return procErrLog(-1, "could not create process");

		pSwt->fctDataSendSet(mpSend, mpUser);

		start(pSwt);

		cmdReg("infoHelp", cmdInfoHelp);

		mState = StSendReadyWait;

		break;
	case StSendReadyWait:

		if (!pSwt->mSendReady)
			break;

		entryLogCreateSet(SystemDebugging::entryLogCreate);

		mReady = true;

		mState = StMain;

		break;
	case StMain:

		commandInterpret();
		procTreeSend();

		break;
	default:
		break;
	}

	return Pending;
}

void SystemDebugging::commandInterpret()
{
	char *pBuf = pSwt->mBufOutCmd;
	char *pBufEnd = pBuf + sizeof(pSwt->mBufOutCmd) - 1;
	Command *pCmd = commands;

	switch (mStateCmd)
	{
	case StCmdRcvdWait: // fetch

		if (!(pSwt->mValidBuf & dBufValidInCmd))
			break;

		mStateCmd = StCmdInterpret;

		break;
	case StCmdInterpret: // interpret/decode and execute

		if (CMD(dKeyModeDebug))
		{
			pSwt->mModeDebug |= 1;
			dInfo("Debug mode %d", pSwt->mModeDebug);
			mStateCmd = StCmdSendStart;
			break;
		}

		if (!pSwt->mModeDebug)
		{
			pSwt->mValidBuf &= ~dBufValidInCmd; // don't answer
			mStateCmd = StCmdRcvdWait;
			break;
		}

		//procInfLog("Received command: %s", pSwt->mBufInCmd);

		*pSwt->mBufOutCmd = 0;

		for (size_t i = 0; i < dNumCmds; ++i, ++pCmd)
		{
			if (!CMD(pCmd->pId))
				continue;

			if (!pCmd->pFctExec)
				continue;

			char *pArg = pSwt->mBufInCmd + strlen(pCmd->pId);

			if (*pArg)
				++pArg;

			pCmd->pFctExec(pArg, pBuf, pBufEnd);

			mStateCmd = StCmdSendStart;
			return;
		}

		dInfo("Unknown command");
		mStateCmd = StCmdSendStart;

		break;
	case StCmdSendStart: // write back

		pSwt->mValidBuf |= dBufValidOutCmd;
		mStateCmd = StCmdSentWait;

		break;
	case StCmdSentWait:

		if (pSwt->mValidBuf & dBufValidOutCmd)
			break;

		pSwt->mValidBuf &= ~dBufValidInCmd;
		mStateCmd = StCmdRcvdWait;

		break;
	default:
		break;
	}
}

void SystemDebugging::procTreeSend()
{
	if (!pSwt->mModeDebug)
		return; // minimize CPU load in production

	if (mCntDelay < cCntDelayMin)
	{
		++mCntDelay;
		return;
	}

	if (pSwt->mValidBuf & dBufValidOutProc)
		return;

	mCntDelay = 0;

	mpTreeRoot->processTreeStr(
				pSwt->mBufOutProc,
				pSwt->mBufOutProc + sizeof(pSwt->mBufOutProc) - 1,
				true, true);

	pSwt->mValidBuf |= dBufValidOutProc;
}

void SystemDebugging::processInfo(char *pBuf, char *pBufEnd)
{
#if 0
	dInfo("State\t\t%s\n", ProcStateString[mState]);
	dInfo("State cmd\t\t%s\n", CmdStateString[mStateCmd]);
#endif
#if 0
	dInfo("En High sens\t%d\n", HAL_GPIO_ReadPin(HighSensEn_GPIO_Port, HighSensEn_Pin));
	dInfo("En RX\t\t%d\n", HAL_GPIO_ReadPin(RxEn_GPIO_Port, RxEn_Pin));
#endif
}

/* static functions */

void SystemDebugging::cmdInfoHelp(char *pArgs, char *pBuf, char *pBufEnd)
{
	Command *pCmd;
	(void)pArgs;

	if (idxInfo >= dNumCmds)
		goto emptySend;

	pCmd = &commands[idxInfo];
	++idxInfo;

	if (!pCmd->pId || !pCmd->pFctExec)
		goto emptySend;

	dInfo("%s|%s|%s|%s",
		pCmd->pId,
		pCmd->pShortcut,
		pCmd->pDesc,
		pCmd->pGroup);

	return;

emptySend:
	*pBuf = 0;
	idxInfo = 0;
}

void SystemDebugging::entryLogCreate(
		const int severity,
		const char *filename,
		const char *function,
		const int line,
		const int16_t code,
		const char *msg,
		const size_t len)
{
#if CONFIG_PROC_HAVE_DRIVERS
	Guard lock(mtxLogEntries);
#endif
	(void)filename;
	(void)function;
	(void)line;
	(void)code;

	if (severity > levelLog)
		return;

	if (pSwt->mValidBuf & dBufValidOutLog)
	{
		logOvf = true;
		return;
	}

	char *pBufLog = pSwt->mBufOutLog;
	size_t lenMax = sizeof(pSwt->mBufOutLog) - 1;
	size_t lenReq = PMIN(len, lenMax);

	memcpy(pBufLog, msg, lenReq);
	pBufLog[lenReq] = 0;

	pSwt->mValidBuf |= dBufValidOutLog;
}

