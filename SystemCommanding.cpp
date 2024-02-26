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
#include <cstdint>

#include "SystemCommanding.h"

// --------------------

#define dForEach_ProcState(gen) \
		gen(StStart) \
		gen(StSendReadyWait) \
		gen(StTelnetInit) \
		gen(StWelcomeSend) \
		gen(StMain) \
		gen(StTmp) \

#define dGenProcStateEnum(s) s,
dProcessStateEnum(ProcState);

#if 1
#define dGenProcStateString(s) #s,
dProcessStateStr(ProcState);
#endif

// --------------------

#define dForEach_KeyState(gen) \
		gen(StKeyMain) \
		gen(StKeyEscMain) \
		gen(StKeyEscBracket) \
		gen(StKeyEsc1) \
		gen(StKeyEsc2) \
		gen(StKeyEsc3) \
		gen(StKeyEscTilde) \
		gen(StKeyIac) \
		gen(StKeyIacDo) \
		gen(StKeyIacWont) \
		gen(StKeyTmp) \

#define dGenKeyStateEnum(s) s,
dProcessStateEnum(KeyState);

#if 1
#define dGenKeyStateString(s) #s,
dProcessStateStr(KeyState);
#endif

#define dByteCheckKeyCommit(b, k)	\
if (key == b) \
{ \
	mStateKey = StKeyMain; \
	\
	*pKeyOut = k; \
	return Positive; \
}

// --------------------

using namespace std;

#define LOG_LVL	0

// http://www.iana.org/assignments/telnet-options/telnet-options.xhtml#telnet-options-1
#define keyIac			0xFF // RFC854
#define keyIacDo		0xFD // RFC854
#define keyIacWont		0xFC // RFC854
#define keyEcho		0x01 // RFC857
#define keySuppGoAhd	0x03 // RFC858
#define keyStatus		0x05 // RFC859
#define keyLineMode		0x22 // RFC1184

#ifndef dPackageName
#define dPackageName "<unknown package>"
#endif

// --------------------

typedef uint16_t KeyUser;

const KeyUser keyBackspace    = 0x7F;
const KeyUser keyBackspaceWin = 0x08;
const KeyUser keyEnter        = 0x0D;
const KeyUser keyEsc          = 0x1B;
const KeyUser keyCtrlC        = 0x03;
const KeyUser keyCtrlD        = 0x04;
const KeyUser keyTab          = 0x09;
const KeyUser keyHelp         = '?';
const KeyUser keyOpt          = '=';

enum KeyExtensions
{
	keyUp = 1000,
	keyDown,
	keyLeft,
	keyRight,
	keyHome,
	keyInsert,
	keyDelete,
	keyEnd,
	keyPgUp,
	keyPgDn,
	keyF0,	keyF1,	keyF2,	keyF3,
	keyF4,	keyF5,	keyF6,	keyF7,
	keyF8,	keyF9,	keyF10,	keyF11,
	keyF12,	keyF13,	keyF14,	keyF15,
	keyF16,	keyF17,	keyF18,	keyF19,
	keyF20,
	keyShiftTab,
};

// --------------------

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

// --------------------

bool SystemCommanding::globalInitDone = false;
#if CONFIG_PROC_HAVE_DRIVERS
mutex SystemCommanding::mtxGlobalInit;
#endif

static list<SystemCommand> cmds;
#if CONFIG_PROC_HAVE_DRIVERS
static mutex mtxCmds;
static mutex mtxCmdExec;
#endif

static bool commandSort(SystemCommand &cmdFirst, SystemCommand &cmdSecond);

SystemCommanding::SystemCommanding(SOCKET fd)
	: Processing("SystemCommanding")
	, mStateKey(StKeyMain)
	, mStartMs(0)
	, mSocketFd(fd)
	, mpTrans(NULL)
	, mDone(false)
	, mpCmdLast(NULL)
	, mArgLast("")
	, mBufFragment("")
{
	mState = StStart;
}

/* member functions */

Success SystemCommanding::process()
{
	//uint32_t curTimeMs = millis();
	//uint32_t diffMs = curTimeMs - mStartMs;
	Success success;
	//bool ok;
	//int res;
	string msg;
#if 0
	dStateTrace;
#endif
	switch (mState)
	{
	case StStart:

		if (mSocketFd == INVALID_SOCKET)
			return procErrLog(-1, "socket file descriptor not set");

		mpTrans = TcpTransfering::create(mSocketFd);
		if (!mpTrans)
			return procErrLog(-1, "could not create process");

		start(mpTrans);

		globalInit();

		mState = StSendReadyWait;

		break;
	case StSendReadyWait:

		success = mpTrans->success();
		if (success != Pending)
			return success;

		if (!mpTrans->mSendReady)
			break;

		mState = StTelnetInit;

		break;
	case StTelnetInit:

		// IAC WILL ECHO
		msg += "\xFF\xFB\x01";

		// IAC WILL SUPPRESS_GO_AHEAD
		msg += "\xFF\xFB\x03";

		// IAC WONT LINEMODE
		msg += "\xFF\xFC\x22";

		// Hide cursor
		msg += "\033[?25l";

		// Set terminal title
		msg += "\033]2;SystemCommanding()\a";

		mpTrans->send(msg.c_str(), msg.size());

		mState = StWelcomeSend;

		break;
	case StWelcomeSend:

		mpTrans->send(cWelcomeMsg.c_str(), cWelcomeMsg.size());

		mState = StMain;

		break;
	case StMain:

		success = mpTrans->success();
		if (success != Pending)
			return success;

		dataReceive();

		if (!mDone)
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

Success SystemCommanding::shutdown()
{
	if (!mpTrans)
		return Positive;

	string msg = "\033[?25h"; // Show cursor

	mpTrans->send(msg.c_str(), msg.size());
	mpTrans->doneSet();
	mpTrans = NULL;

	return Positive;
}

void SystemCommanding::dataReceive()
{
	ssize_t lenReq, lenPlanned, lenDone;
	Success success;
	char buf[8];
	uint16_t key;

	buf[0] = 0;

	lenReq = sizeof(buf) - 1;
	lenPlanned = lenReq;

	lenDone = mpTrans->read(buf, lenPlanned);
	if (!lenDone)
		return;

	if (lenDone < 0)
	{
		mDone = true;
		return;
	}

	buf[lenDone] = 0;

	for (ssize_t i = 0; i < lenDone; ++i)
	{
		success = ansiFilter(buf[i], &key);
		if (success == Pending)
			continue;

		if (success != Positive)
		{
			mDone = true;
			break;
		}

		keyProcess(key);
	}
}

void SystemCommanding::keyProcess(uint16_t key)
{
	procInfLog("key received: %u, 0x%02X", key, key);
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

Success SystemCommanding::ansiFilter(uint8_t ch, uint16_t *pKeyOut)
{
	uint8_t key = ch;
#if 0
	procWrnLog("mState = %s", KeyStateString[mStateKey]);
#endif
#if 1
	procInfLog("char received: 0x%02X '%c'", ch, ch);
#endif
	switch (mStateKey)
	{
	case StKeyMain:

		if (key == keyIac)
		{
			//procWrnLog("Telnet command received");
			mStateKey = StKeyIac;
			break;
		}

		if (key == keyEsc)
		{
			mStateKey = StKeyEscMain;
			break;
		}

		// user disconnect
		if (key == keyCtrlC or key == keyCtrlD)
			return -1;

		*pKeyOut = key;
		return Positive;

		break;
	case StKeyEscMain:

		if (key == '[')
		{
			mStateKey = StKeyEscBracket;
			break;
		}

		return procErrLog(-1, "unexpected key 0x%02X '%c' in state %s",
							key, key, KeyStateString[mStateKey]);

		break;
	case StKeyEscBracket:

		dByteCheckKeyCommit('A', keyUp);
		dByteCheckKeyCommit('B', keyDown);
		dByteCheckKeyCommit('C', keyRight);
		dByteCheckKeyCommit('D', keyLeft);

		dByteCheckKeyCommit('F', keyEnd);
		dByteCheckKeyCommit('H', keyHome);

		dByteCheckKeyCommit('Z', keyShiftTab);

		if (key == '1')
		{
			mStateKey = StKeyEsc1;
			break;
		}

		if (key == '2')
		{
			mStateKey = StKeyEsc2;
			break;
		}

		if (key == '3')
		{
			mStateKey = StKeyEsc3;
			break;
		}

		if (key == '4' or key == '8')
		{
			mStateKey = StKeyEscTilde;

			*pKeyOut = keyEnd;
			return Positive;
		}

		if (key == '5')
		{
			mStateKey = StKeyEscTilde;

			*pKeyOut = keyPgUp;
			return Positive;
		}

		if (key == '6')
		{
			mStateKey = StKeyEscTilde;

			*pKeyOut = keyPgDn;
			return Positive;
		}

		if (key == '7')
		{
			mStateKey = StKeyEscTilde;

			*pKeyOut = keyHome;
			return Positive;
		}

		return procErrLog(-1, "unexpected key 0x%02X '%c' in state %s",
							key, key, KeyStateString[mStateKey]);

		break;
	case StKeyEsc1:

		dByteCheckKeyCommit('~', keyHome);

		if (key >= '0' and key <= '5')
		{
			mStateKey = StKeyEscTilde;

			*pKeyOut = keyF0 + key;
			return Positive;
		}

		if (key >= '7' and key <= '9')
		{
			mStateKey = StKeyEscTilde;

			*pKeyOut = keyF6 + (key - 7);
			return Positive;
		}

		if (key >= 'P' and key <= 'S')
		{
			mStateKey = StKeyMain;

			*pKeyOut = keyF1 + (key - 'P');
			return Positive;
		}

		return procErrLog(-1, "unexpected key 0x%02X '%c' in state %s",
							key, key, KeyStateString[mStateKey]);

		break;
	case StKeyEsc2:

		dByteCheckKeyCommit('~', keyInsert);

		if (key >= '0' and key <= '1')
		{
			mStateKey = StKeyEscTilde;

			*pKeyOut = keyF9 + key;
			return Positive;
		}

		if (key >= '3' and key <= '6')
		{
			mStateKey = StKeyEscTilde;

			*pKeyOut = keyF11 + (key - 3);
			return Positive;
		}

		if (key >= '8' and key <= '9')
		{
			mStateKey = StKeyEscTilde;

			*pKeyOut = keyF15 + (key - 8);
			return Positive;
		}

		return procErrLog(-1, "unexpected key 0x%02X '%c' in state %s",
							key, key, KeyStateString[mStateKey]);

		break;
	case StKeyEsc3:

		dByteCheckKeyCommit('~', keyDelete);

		if (key >= '1' and key <= '4')
		{
			mStateKey = StKeyEscTilde;

			*pKeyOut = keyF17 + (key - 1);
			return Positive;
		}

		return procErrLog(-1, "unexpected key 0x%02X '%c' in state %s",
							key, key, KeyStateString[mStateKey]);

		break;
	case StKeyEscTilde:

		if (key == '~')
		{
			mStateKey = StKeyMain;
			break;
		}

		return procErrLog(-1, "unexpected key 0x%02X '%c' in state %s",
							key, key, KeyStateString[mStateKey]);

		break;
	case StKeyIac:

		if (key == keyIacDo)
		{
			mStateKey = StKeyIacDo;
			break;
		}

		break;
	case StKeyIacDo:

		if (key == keyEcho)
		{
			mStateKey = StKeyMain;
			break;
		}

		if (key == keySuppGoAhd)
		{
			mStateKey = StKeyMain;
			break;
		}

		if (key == keyStatus)
		{
			mStateKey = StKeyMain;
			break;
		}

		return procErrLog(-1, "Unknown DO option: 0x%02X", key);

		break;
	case StKeyIacWont:

		if (key == keyLineMode)
		{
			mStateKey = StKeyMain;
			break;
		}

		return procErrLog(-1, "Unknown WONT option: 0x%02X", key);

		break;
	case StKeyTmp:

		break;
	default:
		break;
	}

	return Pending;
}

/* static functions */

void SystemCommanding::globalInit()
{
	lock_guard<mutex> lock(mtxGlobalInit);

	if (globalInitDone)
		return;

	/* register standard commands here */
	//intCmdReg("dummy",		dummyExecute,		"",		"dummy command");
	intCmdReg("help",		helpPrint,		"h",		"this help screen");
	//intCmdReg("broadcast",	messageBroadcast,	"b",		"broadcast message to other command terminals");
	//intCmdReg("memWrite",	memoryWrite,		"w",		"write memory");

	globalInitDone = true;
}

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

