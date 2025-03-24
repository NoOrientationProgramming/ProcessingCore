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

#define CMD(x)		(!strncmp(pEnv->buffInCmd, x, strlen(x)))

#ifndef dFwVersion
#define dFwVersion "<unknown firmware version>"
#endif

#define dNumCmds		32
Command commands[dNumCmds] = {};

SystemDebugging::SystemDebugging(Processing *pTreeRoot)
	: Processing("SystemDebugging")
	, mpTreeRoot(pTreeRoot)
	, mStateCmd(StCmdRcvdWait)
	, mpSwt(NULL)
{
	mState = StStart;
}

bool SystemDebugging::ready()
{
	return false;
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

/* member functions */

Success SystemDebugging::process()
{
	switch (mState)
	{
	case StStart:

		if (!mpTreeRoot)
			return procErrLog(-1, "tree root not set");

		mpSwt = SingleWireTransfering::create();
		if (!mpSwt)
			return procErrLog(-1, "could not create process");

		start(mpSwt);

		mState = StSendReadyWait;

		break;
	case StSendReadyWait:

		if (!mpSwt->mSendReady)
			break;

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
	char *pBuf = pEnv->buffOutCmd;
	char *pBufEnd = pBuf + sizeof(pEnv->buffOutCmd);
	Command *pCmd = commands;

	switch (mStateCmd)
	{
	case StCmdRcvdWait: // fetch

		if (!(pEnv->buffValid & dBuffValidInCmd))
			break;

		mStateCmd = StCmdInterpret;

		break;
	case StCmdInterpret: // interpret/decode and execute

		if (CMD("aaaaa"))
		{
			pEnv->debugMode ^= 1;
			dInfo("Debug mode %d", pEnv->debugMode);
			mStateCmd = StCmdSendStart;
			break;
		}

		if (!pEnv->debugMode)
		{
			// don't answer
			pEnv->buffValid &= ~dBuffValidInCmd;
			mStateCmd = StCmdRcvdWait;

			break;
		}

		procInfLog("Received command: %s", pEnv->buffInCmd);

		*pEnv->buffOutCmd = 0;

		for (size_t i = 0; i < dNumCmds; ++i, ++pCmd)
		{
			if (!CMD(pCmd->id))
				continue;

			if (!pCmd->func)
				continue;

			const char *pArg = pEnv->buffInCmd + strlen(pCmd->id);

			if (*pArg)
				++pArg;

			pCmd->func(pArg, pBuf, pBufEnd);

			if (!*pEnv->buffOutCmd)
				dInfo("Done");

			mStateCmd = StCmdSendStart;
			break;
		}

		if (*pEnv->buffOutCmd)
			break;

		dInfo("Unknown command");
		mStateCmd = StCmdSendStart;

		break;
	case StCmdSendStart: // write back

		pEnv->buffValid |= dBuffValidOutCmd;
		mStateCmd = StCmdSentWait;

		break;
	case StCmdSentWait:

		if (pEnv->buffValid & dBuffValidOutCmd)
			break;

		pEnv->buffValid &= ~dBuffValidInCmd;
		mStateCmd = StCmdRcvdWait;

		break;
	default:
		break;
	}
}

void SystemDebugging::procTreeSend()
{
	if (!pEnv->debugMode)
		return; // minimize CPU load in production

	if (pEnv->buffValid & dBuffValidOutProc)
		return;

	mpTreeRoot->processTreeStr(pEnv->buffOutProc, pEnv->buffOutProc + sizeof(pEnv->buffOutProc), true, true);

	pEnv->buffValid |= dBuffValidOutProc;
}

void SystemDebugging::processInfo(char *pBuf, char *pBufEnd)
{
	dInfo("Firmware\t\t%s\n", dFwVersion);
#if 1
	dInfo("State\t\t\t%s\n", ProcStateString[mState]);
	dInfo("State cmd\t\t\t%s\n", CmdStateString[mStateCmd]);
#endif
#if 0
	dInfo("En High sens\t%d\n", HAL_GPIO_ReadPin(HighSensEn_GPIO_Port, HighSensEn_Pin));
	dInfo("En RX\t\t%d\n", HAL_GPIO_ReadPin(RxEn_GPIO_Port, RxEn_Pin));
#endif
}

/* static functions */

