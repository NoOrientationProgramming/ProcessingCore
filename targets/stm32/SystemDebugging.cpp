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

#if 1
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

#if 1
#define dGenCmdStateString(s) #s,
dProcessStateStr(CmdState);
#endif

using namespace std;

#define CMD(x)		(!strncmp(pSwt->mBufInCmd, x, strlen(x)))

#ifndef dFwVersion
#define dFwVersion "<unknown firmware version>"
#endif

#ifndef dKeyModeDebug
#define dKeyModeDebug "aaaaa"
#endif

static SingleWireTransfering *pSwt = NULL;
#if CONFIG_PROC_HAVE_DRIVERS
static mutex mtxLogEntries;
#endif
static int levelLog = 3;
static bool logOvf = false;

#define dNumCmds		32
Command commands[dNumCmds] = {};

SystemDebugging::SystemDebugging(Processing *pTreeRoot)
	: Processing("SystemDebugging")
	, mpTreeRoot(pTreeRoot)
	, mpSend(NULL)
	, mpUser(NULL)
	, mReady(false)
	, mStateCmd(StCmdRcvdWait)
	, mModeDebug(0)
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

bool SystemDebugging::cmdReg(const char *pId, CmdFunc pFunc)
{
	Command *pCmd = freeCmdStructGet();

	if (!pCmd)
	{
		errLog(-1, "Max registered commands reached");
		return false;
	}

	pCmd->id = pId;
	pCmd->func = pFunc;

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
	char *pBufEnd = pBuf + sizeof(pSwt->mBufOutCmd);
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
			mModeDebug |= 1;
			dInfo("Debug mode %d", mModeDebug);
			mStateCmd = StCmdSendStart;
			break;
		}

		if (!mModeDebug)
		{
			// don't answer
			pSwt->mValidBuf &= ~dBufValidInCmd;
			mStateCmd = StCmdRcvdWait;

			break;
		}

		procInfLog("Received command: %s", pSwt->mBufInCmd);

		*pSwt->mBufOutCmd = 0;

		for (size_t i = 0; i < dNumCmds; ++i, ++pCmd)
		{
			if (!CMD(pCmd->id))
				continue;

			if (!pCmd->func)
				continue;

			const char *pArg = pSwt->mBufInCmd + strlen(pCmd->id);

			if (*pArg)
				++pArg;

			pCmd->func(pArg, pBuf, pBufEnd);

			if (!*pSwt->mBufOutCmd)
				dInfo("Done");

			mStateCmd = StCmdSendStart;
			break;
		}

		if (*pSwt->mBufOutCmd)
			break;

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
	if (!mModeDebug)
		return; // minimize CPU load in production

	if (pSwt->mValidBuf & dBufValidOutProc)
		return;

	mpTreeRoot->processTreeStr(
				pSwt->mBufOutProc,
				pSwt->mBufOutProc + sizeof(pSwt->mBufOutProc),
				true, true);

	pSwt->mValidBuf |= dBufValidOutProc;
}

void SystemDebugging::processInfo(char *pBuf, char *pBufEnd)
{
	dInfo("Firmware\t\t%s\n", dFwVersion);
#if 1
	dInfo("State\t\t%s\n", ProcStateString[mState]);
	dInfo("State cmd\t\t%s\n", CmdStateString[mStateCmd]);
#endif
#if 0
	dInfo("En High sens\t%d\n", HAL_GPIO_ReadPin(HighSensEn_GPIO_Port, HighSensEn_Pin));
	dInfo("En RX\t\t%d\n", HAL_GPIO_ReadPin(RxEn_GPIO_Port, RxEn_Pin));
#endif
}

/* static functions */

Command *SystemDebugging::freeCmdStructGet()
{
	Command *pCmd = commands;

	for (size_t i = 0; i < dNumCmds; ++i, ++pCmd)
	{
		if (pCmd->id && pCmd->func)
			continue;

		return pCmd;
	}

	return NULL;
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

