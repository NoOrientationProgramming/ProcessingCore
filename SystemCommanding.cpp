/*
  This file is part of the DSP-Crowd project
  https://www.dsp-crowd.com

  Author(s):
      - Johannes Natter, office@dsp-crowd.com

  File created on 30.11.2019

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

#include <string.h>

#include "SystemCommanding.h"
//#include "LibDspc.h"

#define dForEach_ProcState(gen) \
		gen(StStart) \
		gen(StSendReadyWait) \
		gen(StMain) \
		gen(StTmp) \

#define dGenProcStateEnum(s) s,
dProcessStateEnum(ProcState);

#if 1
#define dGenProcStateString(s) #s,
dProcessStateStr(ProcState);
#endif

using namespace std;

#define LOG_LVL	0

#ifndef dPackageName
#define dPackageName "<unknown package>"
#endif

const string cWelcomeMsg = "\r\n" dPackageName "\r\n" \
			"System Terminal\r\n\r\n" \
			"type 'help' or just 'h' for a list of available commands\r\n\r\n" \
			"# ";
const string cSeqCtrlC = "\xff\xf4\xff\xfd\x06";
const size_t cLenSeqCtrlC = cSeqCtrlC.size();

const string cInternalCmdCls = "dbg";
const int cSizeCmdIdMax = 16;
const size_t cSizeBufCmdIn = 63;
const size_t cSizeBufFragmentMax = cSizeBufCmdIn - 5;
const size_t cSizeBufCmdOut = 512;

mutex SystemCommanding::mtxGlobalInit;
bool SystemCommanding::globalInitDone = false;

static list<SystemCommand> cmds;
static mutex mtxCmds;
static mutex mtxCmdExec;

static bool commandSort(SystemCommand &cmdFirst, SystemCommand &cmdSecond);

SystemCommanding::SystemCommanding(SOCKET fd)
	: Processing("SystemCommanding")
	, mStartMs(0)
	, mSocketFd(fd)
	, mpTrans(NULL)
	, mpCmdLast(NULL)
	, mArgLast("")
	, mBufFragment("")
{
	mState = StStart;
}

/* member functions */
Success SystemCommanding::initialize()
{
#if CONFIG_PROC_HAVE_DRIVERS
	Guard lock(mtxGlobalInit);
#endif
	if (mSocketFd == INVALID_SOCKET)
		return procErrLog(-1, "socket file descriptor not set");

	if (!globalInitDone)
	{
		/* register standard commands here */
		//intCmdReg("dummy",		dummyExecute,		"",		"dummy command");
		intCmdReg("help",		helpPrint,		"h",		"this help screen");
		//intCmdReg("broadcast",	messageBroadcast,	"b",		"broadcast message to other command terminals");
		//intCmdReg("memWrite",	memoryWrite,		"w",		"write memory");

		globalInitDone = true;
	}

	mpTrans = TcpTransfering::create(mSocketFd);
	if (!mpTrans)
		return procErrLog(-1, "could not create process");

	start(mpTrans);

	return Positive;
}

Success SystemCommanding::process()
{
	//uint32_t curTimeMs = millis();
	//uint32_t diffMs = curTimeMs - mStartMs;
	Success success;
	//bool ok;
	//int res;
#if 0
	dStateTrace;
#endif
	success = mpTrans->success();

	if (success != Pending)
		return success;

	switch (mState)
	{
	case StStart:

		mState = StSendReadyWait;

		break;
	case StSendReadyWait:

		if (!mpTrans->mSendReady)
			break;

		mpTrans->send(cWelcomeMsg.c_str(), cWelcomeMsg.size());

		mState = StMain;

		break;
	case StMain:

		success = commandReceive();
		if (success == Pending)
			break;

		if (success == Positive)
			break;

		return Positive;

		break;
	case StTmp:

		break;
	default:
		break;
	}

	return Pending;
}

Success SystemCommanding::commandReceive()
{
	char buf[cSizeBufCmdIn];
	ssize_t lenReq, lenPlanned, lenDone;
	bool newlineFound = false;

	buf[0] = 0;

	lenReq = sizeof(buf) - 1;
	lenPlanned = lenReq;

	lenDone = mpTrans->read(buf, lenPlanned);
	if (!lenDone)
		return Pending;

	if (lenDone < 0)
		return -1;

	buf[lenDone] = 0;

	if (lenDone >= lenPlanned)
	{
		string msg = "command too long";

		procWrnLog("%s", msg.c_str());
		mpTrans->send(msg.c_str(), msg.size());

		return Pending;
	}

	//procWrnLog("Command received");
	//hexDump(buf, lenDone);

	if ((buf[0] == 0x03) or (buf[0] == 0x04))
	{
		procInfLog("end of transmission");
		return -1;
	}

	if (!strncmp(buf, cSeqCtrlC.c_str(), cLenSeqCtrlC))
	{
		procInfLog("transmission cancelled");
		return -1;
	}

	if (buf[lenDone - 1] == '\n')
	{
		--lenDone;
		buf[lenDone] = 0;
		newlineFound = true;
	}

	if (buf[lenDone - 1] == '\r')
	{
		--lenDone;
		buf[lenDone] = 0;
	}

	string strFragment(buf);
	string msg;

	if (!newlineFound)
	{
		//procWrnLog("Only got fragment: '%s'", strFragment.c_str());
		mBufFragment += strFragment;

		if (mBufFragment.size() > cSizeBufFragmentMax)
		{
			mBufFragment.clear();

			msg = "Fragment buffer overflow";
			procWrnLog("%s", msg.c_str());

			msg += "\r\n# ";
			mpTrans->send(msg.c_str(), msg.size());
		}

		return Pending;
	}

	string cmd = mBufFragment + strFragment;
	mBufFragment.clear();
#if 0
	procWrnLog("Command received: '%s'", cmd.c_str());
#endif
	snprintf(buf, lenReq, "%s", cmd.c_str());

	char *pCmd, *pArgs;
	char *pFound;

	pCmd = buf;
	pArgs = &buf[cmd.size()];

	pFound = strchr(buf, ' ');
	if (pFound)
	{
		*pFound = 0;
		pArgs = pFound + 1;
	}

	commandExecute(pCmd, pArgs);

	return Positive;
}

void SystemCommanding::lfToCrLf(char *pBuf, string &str)
{
	size_t lenBuf = strlen(pBuf) + 1;
	char *pBufLineStart, *pBufIter;
	int8_t lastLine;

	str.clear();

	if (!pBuf or !*pBuf)
		return;

	str.reserve(lenBuf);

	pBufLineStart = pBufIter = pBuf;
	lastLine = 0;

	while (1)
	{
		if (pBufIter >= pBuf + lenBuf)
			break;

		if (*pBufIter and *pBufIter != '\n')
		{
			++pBufIter;
			continue;
		}

		if (!*pBufIter)
		{
			if (!*(pBufIter - 1)) // last line drawn already
				break;

			lastLine = 1;
		}

		*pBufIter = 0; // terminate current line starting at pBufLineStart

		str += pBufLineStart;
		str += "\r\n";

		++pBufIter;
		pBufLineStart = pBufIter;

		if (lastLine)
			break;
	}
}

void SystemCommanding::commandExecute(const char *pCmd, char *pArgs)
{
	string msg;

	char bufOut[cSizeBufCmdOut];
	size_t lenBuf = sizeof(bufOut) - 1;
	list<SystemCommand>::const_iterator iter;

	bufOut[0] = 0;

	if (!*pCmd)
	{
		if (!mpCmdLast)
		{
			msg = "no last command";
			procWrnLog("%s", msg.c_str());

			msg += "\r\n# ";
			mpTrans->send(msg.c_str(), msg.size());

			return;
		}

		msg = "# ";
		msg += mpCmdLast->id;

		if (mArgLast.size())
		{
			msg += " ";
			msg += mArgLast;
		}

		msg += "\r\n";

		mpTrans->send(msg.c_str(), msg.size());

		sprintf(pArgs, "%s", mArgLast.c_str());
		mpCmdLast->func(pArgs, bufOut, bufOut + lenBuf);
		bufOut[lenBuf] = 0;

		lfToCrLf(bufOut, msg);

		if (msg.size() and msg.back() != '\n')
			msg += "\r\n";

		msg += "# ";
		mpTrans->send(msg.c_str(), msg.size());

		return;
	}

	iter = cmds.begin();
	for (; iter != cmds.end(); ++iter)
	{
		if (strcmp(pCmd, iter->id.c_str()) && strcmp(pCmd, iter->shortcut.c_str()))
			continue;
#if 0
		procWrnLog("Command executed   '%s'", pCmd);
		procWrnLog("Command arguments  '%s'", pArgs);
#endif
		mpCmdLast = &(*iter);
		mArgLast = string(pArgs);

		iter->func(pArgs, bufOut, bufOut + lenBuf);
		bufOut[lenBuf] = 0;

		lfToCrLf(bufOut, msg);

		if (msg.size() and msg.back() != '\n')
			msg += "\r\n";

		msg += "# ";
		mpTrans->send(msg.c_str(), msg.size());

		return;
	}

	msg = "command not found";
	procWrnLog("%s", msg.c_str());

	msg += "\r\n# ";
	mpTrans->send(msg.c_str(), msg.size());
}

void SystemCommanding::processInfo(char *pBuf, char *pBufEnd)
{
#if 1
	dInfo("State\t\t\t%s\n", ProcStateString[mState]);
	dInfo("Last command\t\t");

	if (mpCmdLast)
	{
		dInfo("%s", mpCmdLast->id.c_str());
		if (mArgLast.size())
			dInfo(" %s", mArgLast.c_str());
	}
	else
		dInfo("<none>");

	dInfo("\n");
#endif
}

/* static functions */

void cmdReg(
		const string &id,
		FuncCommand cmdFunc,
		const string &shortcut,
		const string &desc,
		const string &group)
{
	dbgLog(LOG_LVL, "registering command %s", id.c_str());
#if CONFIG_PROC_HAVE_DRIVERS
	Guard lock(mtxCmds);
#endif
	SystemCommand cmd, newCmd = {id, cmdFunc, shortcut, desc, group};
	list<SystemCommand>::iterator iter;

	iter = cmds.begin();
	for (; iter != cmds.end(); ++iter)
	{
		cmd = *iter;

		if (newCmd.id == cmd.id)
		{
			wrnLog("id '%s' already registered. skipping", cmd.id.c_str());
			return;
		}

		if (newCmd.id == cmd.id)
		{
			wrnLog("shortcut '%s' already registered. skipping", cmd.shortcut.c_str());
			return;
		}

		if (newCmd.id == cmd.id)
		{
			wrnLog("function pointer 0x%08X already registered. skipping", cmd.func);
			return;
		}
	}

	cmds.push_back(newCmd);
	cmds.sort(commandSort);

	dbgLog(LOG_LVL, "registering command %s: done", id.c_str());
}

void intCmdReg(const string &id, FuncCommand cmdFunc, const string &shortcut, const string &desc)
{
	cmdReg(id, cmdFunc, shortcut, desc, cInternalCmdCls);
}

void SystemCommanding::dummyExecute(char *pArgs, char *pBuf, char *pBufEnd)
{
	(void)pArgs;
	(void)pBuf;
	(void)pBufEnd;

	wrnLog("dummy with '%s'", pArgs);
	dInfo("dummy with '%s'\n", pArgs);
}

void SystemCommanding::helpPrint(char *pArgs, char *pBuf, char *pBufEnd)
{
	SystemCommand cmd;
	string group = "";

	(void)pArgs;

	dInfo("\nAvailable commands\n");

	for (list<SystemCommand>::iterator iter = cmds.begin(); iter != cmds.end(); ++iter)
	{
		cmd = *iter;

		if (cmd.group != group)
		{
			dInfo("\n");

			if (cmd.group.size() and cmd.group != cInternalCmdCls)
				dInfo("%s\n", cmd.group.c_str());
			group = cmd.group;
		}

		dInfo("  ");

		if (cmd.shortcut != "")
			dInfo("%s, ", cmd.shortcut.c_str());
		else
			dInfo("   ");

		dInfo("%-*s", cSizeCmdIdMax + 2, cmd.id.c_str());

		if (cmd.desc.size())
			dInfo(".. %s", cmd.desc.c_str());

		dInfo("\n");
	}

	dInfo("\n");
}

void SystemCommanding::messageBroadcast(char *pArgs, char *pBuf, char *pBufEnd)
{
	(void)pArgs;
	(void)pBuf;
	(void)pBufEnd;

	dInfo("error: not implemented\n");
}

void SystemCommanding::memoryWrite(char *pArgs, char *pBuf, char *pBufEnd)
{
	(void)pArgs;
	(void)pBuf;
	(void)pBufEnd;

	dInfo("error: not implemented\n");
}

bool commandSort(SystemCommand &cmdFirst, SystemCommand &cmdSecond)
{
	if (cmdFirst.group == cInternalCmdCls and cmdSecond.group != cInternalCmdCls)
		return true;
	if (cmdFirst.group != cInternalCmdCls and cmdSecond.group == cInternalCmdCls)
		return false;

	if (cmdFirst.group < cmdSecond.group)
		return true;
	if (cmdFirst.group > cmdSecond.group)
		return false;

	if (cmdFirst.shortcut != "" and cmdSecond.shortcut == "")
		return true;
	if (cmdFirst.shortcut == "" and cmdSecond.shortcut != "")
		return false;

	if (cmdFirst.id < cmdSecond.id)
		return true;
	if (cmdFirst.id > cmdSecond.id)
		return false;

	return true;
}

