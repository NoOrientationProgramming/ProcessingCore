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
#include "fwVersion.h"
#include "env.h"

enum CmdState
{
	CmdRcvdWait = 0,
	CmdInterpret,
	CmdSendStart,
	CmdSentWait,
};

#define CMD(x)		(!strncmp(pEnv->buffInCmd, x, strlen(x)))

#define dNumCmds		32
Command commands[dNumCmds] = {};

extern TIM_HandleTypeDef htim1;

using namespace std;

SystemDebugging::SystemDebugging()
	: Processing("SystemDebugging")
	, mpTreeRoot(NULL)
	, state(CmdRcvdWait)
{
}

SystemDebugging::~SystemDebugging()
{
}

void SystemDebugging::treeRootSet(Processing *pTreeRoot)
{
	mpTreeRoot = pTreeRoot;
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
		if (pCmd->id and pCmd->func)
			continue;

		return pCmd;
	}

	return NULL;
}

/* member functions */
Success SystemDebugging::initialize()
{
	if (!mpTreeRoot)
		return procErrLog(0, -1, "tree root not set");

	return Positive;
}

Success SystemDebugging::process()
{
	commandInterpret();
	procTreeSend();

	return Pending;
}

void SystemDebugging::commandInterpret()
{
	char *pBuf = pEnv->buffOutCmd;
	char *pBufEnd = pBuf + sizeof(pEnv->buffOutCmd);
	Command *pCmd = commands;

	switch (state)
	{
	case CmdRcvdWait: // fetch

		if (!(pEnv->buffValid & dBuffValidInCmd))
			break;

		state = CmdInterpret;

		break;
	case CmdInterpret: // interpret/decode and execute

		if (CMD("aaaaa"))
		{
			pEnv->debugMode ^= 1;
			dInfo("Debug mode %d", pEnv->debugMode);
			state = CmdSendStart;
			break;
		}

		if (!pEnv->debugMode)
		{
			// don't answer
			pEnv->buffValid &= ~dBuffValidInCmd;
			state = CmdRcvdWait;

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

			state = CmdSendStart;
			break;
		}

		if (*pEnv->buffOutCmd)
			break;

		dInfo("Unknown command");
		state = CmdSendStart;

		break;
	case CmdSendStart: // write back

		pEnv->buffValid |= dBuffValidOutCmd;
		state = CmdSentWait;

		break;
	case CmdSentWait:

		if (pEnv->buffValid & dBuffValidOutCmd)
			break;

		pEnv->buffValid &= ~dBuffValidInCmd;
		state = CmdRcvdWait;

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
#if 0
	dInfo("En High sens\t%d\n", HAL_GPIO_ReadPin(HighSensEn_GPIO_Port, HighSensEn_Pin));
	dInfo("En RX\t\t%d\n", HAL_GPIO_ReadPin(RxEn_GPIO_Port, RxEn_Pin));
#endif
}

/* static functions */

