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
#include <chrono>

#include "SystemCommanding.h"

// --------------------

#define dForEach_ProcState(gen) \
		gen(StStart) \
		gen(StSendReadyWait) \
		gen(StAutoModeInitWait) \
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
		gen(StKeyEscSemi) \
		gen(StKeyEscSemi5) \
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

#define dKeyIgnore(k)	\
if (key == k) \
{ \
	procInfLog("ignoring %u, 0x%02X", key, key); \
	return false; \
}

// --------------------

using namespace std;
using namespace chrono;

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
	keyJumpLeft, keyJumpRight
};

// --------------------

const string cWelcomeMsg = "\r\n" dPackageName "\r\n" \
			"System Terminal\r\n\r\n" \
			"type 'help' or just 'h' for a list of available commands\r\n\r\n";
const string cSeqCtrlC = "\xff\xf4\xff\xfd\x06";
const size_t cLenSeqCtrlC = cSeqCtrlC.size();

const uint32_t cTmoAutoModeMs = 40;
const int cSizeCmdIdMax = 16;

// --------------------

bool SystemCommanding::globalInitDone = false;
#if CONFIG_PROC_HAVE_DRIVERS
mutex SystemCommanding::mtxGlobalInit;
#endif

static list<SystemCommand> cmds;
#if CONFIG_PROC_HAVE_DRIVERS
static mutex mtxCmds;
#endif

SystemCommanding::SystemCommanding(SOCKET fd)
	: Processing("SystemCommanding")
	, mSocketFd(fd)
	, mpTrans(NULL)
	, mStateKey(StKeyMain)
	, mStartMs(0)
	, mCursorHidden(false)
	, mDone(false)
	, mIdxLineEdit(0)
	, mIdxLineView(0)
#if CONFIG_CMD_SIZE_HISTORY
	, mIdxLineLast(-1)
#endif
	, mIdxColCursor(0)
	, mIdxColLineEnd(0)
{
	mState = StStart;

	for (size_t i = 0; i < cNumCmdInBuffer; ++i)
		mCmdInBuf[i][0] = 0;
}

/* member functions */

Success SystemCommanding::process()
{
	uint32_t curTimeMs = millis();
	uint32_t diffMs = curTimeMs - mStartMs;
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

		mpTrans->procTreeDisplaySet(false);
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

		mStartMs = curTimeMs;
		mState = StTelnetInit;

		break;
	case StAutoModeInitWait:

		if (diffMs > cTmoAutoModeMs)
		{
			mState = StTelnetInit;
			break;
		}

		success = autoCommandReceive();
		if (success == Pending)
			break;

		msg += "Auto command executed";

		mpTrans->send(msg.c_str(), msg.size());

		return success;

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

		mCursorHidden = true;

		mState = StWelcomeSend;

		break;
	case StWelcomeSend:

		mpTrans->send(cWelcomeMsg.c_str(), cWelcomeMsg.size());
		promptSend();

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

	string msg = "\r\n";

	if (mCursorHidden)
		msg += "\033[?25h"; // Show cursor

	mpTrans->send(msg.c_str(), msg.size());
	mpTrans->doneSet();
	mpTrans = NULL;

	return Positive;
}

Success SystemCommanding::autoCommandReceive()
{
	ssize_t lenReq, lenPlanned, lenDone;
	char buf[8];

	buf[0] = 0;

	lenReq = sizeof(buf) - 1;
	lenPlanned = lenReq;

	lenDone = mpTrans->read(buf, lenPlanned);
	if (!lenDone)
		return Pending;

	if (lenDone < 0)
		return procErrLog(-1, "could not receive auto command");

	buf[lenDone] = 0;

	procInfLog("auto bytes received: %d", lenDone);

	for (ssize_t i = 0; i < lenDone; ++i)
		procInfLog("byte: %3u %02x '%c'", buf[i], buf[i], buf[i]);

	return Positive;
}

void SystemCommanding::dataReceive()
{
	ssize_t lenReq, lenPlanned, lenDone;
	char buf[8];
	Success success;
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

	procInfLog("bytes received: %d", lenDone);

	if (lenDone == 1 and buf[0] == keyEsc)
		return;

	bool changed = false;

	for (ssize_t i = 0; i < lenDone; ++i)
	{
		procInfLog("byte: %3u %02x '%c'", buf[i], buf[i], buf[i]);

		success = ansiFilter(buf[i], &key);
		if (success == Pending)
			continue;

		if (success != Positive)
		{
			mDone = true;
			break;
		}

		if (key == keyEnter)
		{
			lineAck();
			break;
		}

		changed |= bufferEdit(key);
	}

	// Send result

	if (!changed)
		return;

	procInfLog("buffer changed");

	promptSend();
}

void SystemCommanding::lineAck()
{
	procInfLog("line acknowledged");

	promptSend(false, false, true);

	char *pEdit= mCmdInBuf[mIdxLineEdit];

	if (*pEdit)
	{
#if CONFIG_CMD_SIZE_HISTORY
		historyInsert();
#endif
		commandExecute();
	}

	*pEdit = 0;
	mIdxColLineEnd = 0;
	mIdxColCursor = 0;

	mIdxLineView = mIdxLineEdit;

	promptSend();
}

void SystemCommanding::commandExecute()
{
	char *pEdit = mCmdInBuf[mIdxLineEdit];
#if CONFIG_CMD_SIZE_HISTORY
	if (!*pEdit)
	{
		const char *pLast = mCmdInBuf[mIdxLineLast];

		// reuse edit buffer as command buffer
		while (*pLast)
			*pEdit++ = *pLast++;

		pEdit = mCmdInBuf[mIdxLineEdit];
	}
#endif
	procInfLog("executing line: %s", pEdit);
}

#if CONFIG_CMD_SIZE_HISTORY
void SystemCommanding::historyInsert()
{
	if (cNumCmdInBuffer <= 1)
		return;

	const char *pEdit = mCmdInBuf[mIdxLineEdit];
	const char *pLast = mIdxLineLast >= 0 ? mCmdInBuf[mIdxLineLast] : NULL;

	while (pLast and *pEdit == *pLast and *pEdit)
	{
		++pEdit;
		++pLast;
	}

	// ignore duplicate
	if (pLast and *pEdit == *pLast)
		return;

	// insert

	mIdxLineLast = mIdxLineEdit;

	++mIdxLineEdit;

	if (mIdxLineEdit >= cNumCmdInBuffer)
		mIdxLineEdit = 0;

	mCmdInBuf[mIdxLineEdit][0] = 0;
}

bool SystemCommanding::historyNavigate(uint16_t key)
{
	if (key != keyUp and key != keyDown)
		return false;

	if (key == keyDown and mIdxLineView == mIdxLineEdit)
		return false;

	int16_t direction = key == keyDown ? 1 : -1;
	int16_t mIdxLineViewNew = mIdxLineView + direction;

	if (mIdxLineViewNew < 0)
		mIdxLineViewNew = cNumCmdInBuffer - 1;

	if (mIdxLineViewNew >= cNumCmdInBuffer)
		mIdxLineViewNew = 0;

	if (key == keyUp and mIdxLineViewNew == mIdxLineEdit)
		return false;

	if (!mCmdInBuf[mIdxLineViewNew][0])
		return false;

	mIdxLineView = mIdxLineViewNew;

	char *pDstBase = mCmdInBuf[mIdxLineEdit];
	char *pDst = pDstBase;

	if (mIdxLineView != mIdxLineEdit)
	{
		const char *pSrc = mCmdInBuf[mIdxLineView];

		while (*pSrc)
			*pDst++ = *pSrc++;
	}

	*pDst = 0;

	mIdxColLineEnd = pDst - pDstBase;
	mIdxColCursor = mIdxColLineEnd;

	return true;
}
#endif

bool SystemCommanding::bufferEdit(uint16_t key)
{
	// Filter

	dKeyIgnore('\0');

	if (key < 256)
		procInfLog("key received: %u, 0x%02X, '%c'", key, key, (char)key);
	else
		procInfLog("key received: %u, 0x%02X", key, key);

	// Navigation

	if (key == keyHome)
	{
		mIdxColCursor = 0;
		return true;
	}

	if (key == keyEnd)
	{
		mIdxColCursor = mIdxColLineEnd;
		return true;
	}

	if (key == keyLeft)
	{
		if (!mIdxColCursor)
			return false;

		--mIdxColCursor;
		return true;
	}

	if (key == keyRight)
	{
		if (mIdxColCursor >= mIdxColLineEnd)
			return false;

		++mIdxColCursor;
		return true;
	}

	if (cursorJump(key))
		return true;
#if CONFIG_CMD_SIZE_HISTORY
	if (historyNavigate(key))
		return true;
#endif
	// Removal

	if (chRemove(key))
		return true;

	// Insertion

	if (!keyIsInsert(key))
		return false;

	if (mIdxColLineEnd >= cIdxColMax)
		return false;

	char *pCursor = &mCmdInBuf[mIdxLineEdit][mIdxColCursor];

	char chInsert = (char)key;
	char chSave;

	while (true)
	{
		chSave = *pCursor;
		*pCursor++ = chInsert;

		if (!chInsert)
			break;

		chInsert = chSave;
	}

	++mIdxColCursor;
	++mIdxColLineEnd;

	return true;
}

bool SystemCommanding::chRemove(uint16_t key)
{
	char *pCursor = &mCmdInBuf[mIdxLineEdit][mIdxColCursor];
	bool isBackspace = key == keyBackspace or key == keyBackspaceWin;
	char *pRemove = NULL;

	if (isBackspace and mIdxColCursor)
	{
		--mIdxColCursor;
		--pCursor;

		pRemove = pCursor;
	}

	if (key == keyDelete and *pCursor)
		pRemove = pCursor;

	if (!pRemove)
		return false;

	char *pInsert = pRemove + 1;

	while (true)
	{
		*pRemove++ = *pInsert;

		if (!*pInsert)
			break;

		++pInsert;
	}

	--mIdxColLineEnd;

	return true;
}

bool SystemCommanding::cursorJump(uint16_t key)
{
	if (key != keyJumpLeft and key != keyJumpRight)
		return false;

	int direction = key == keyJumpRight ? 1 : -1;
	bool statePrev = (direction + 1) >> 1;
	bool stateCursor = not statePrev;
	uint16_t idxStop = statePrev ? mIdxColLineEnd : 0;
	bool changed = false;

	const char *pCursor = &mCmdInBuf[mIdxLineEdit][mIdxColCursor];
	const char *pPrev = pCursor - 1;

	procInfLog("jumping to the %s", statePrev ? "right" : "left");

	while (true)
	{
		if (mIdxColCursor == idxStop)
			break;

		mIdxColCursor += direction;
		changed = true;

		pPrev += direction;
		pCursor += direction;

		if (keyIsAlphaNum(*pPrev) == statePrev and
			keyIsAlphaNum(*pCursor) == stateCursor)
			break;
	}

	return changed;
}

void SystemCommanding::promptSend(bool cursor, bool preNewLine, bool postNewLine)
{
	const char *pCh = &mCmdInBuf[mIdxLineEdit][0];
	const char *pCursor = &mCmdInBuf[mIdxLineEdit][mIdxColCursor];
	const char *pEnd = &mCmdInBuf[mIdxLineEdit][mIdxColLineEnd + 1];
	string msg;
	size_t numPad;

	if (preNewLine)
		msg += "\r\n";

	msg += "\r# ";

	for (; pCh < pEnd; ++pCh)
	{
		if (cursor and pCh == pCursor)
			msg += "\033[7m";

		if (*pCh)
			msg.append(1, *pCh);
		else
			msg.append(1, ' ');

		if (pCh == pCursor)
			msg += "\033[0m";
	}

	numPad = cIdxColMax - mIdxColLineEnd;

	if (numPad)
		msg.append(numPad, ' ');

	if (postNewLine)
		msg += "\r\n";

	mpTrans->send(msg.c_str(), msg.size());
}

bool SystemCommanding::keyIsInsert(uint16_t key)
{
	if (keyIsAlphaNum(key))
		return true;

	if (key == '-' or key == '_')
		return true;

	if (key == ' ')
		return true;

	return false;
}

bool SystemCommanding::keyIsAlphaNum(uint16_t key)
{
	if (key >= 'a' and key <= 'z')
		return true;

	if (key >= 'A' and key <= 'Z')
		return true;

	if (key >= '0' and key <= '9')
		return true;

	return false;
}

void SystemCommanding::processInfo(char *pBuf, char *pBufEnd)
{
#if 1
	dInfo("State\t\t\t%s\n", ProcStateString[mState]);
#endif
#if CONFIG_CMD_SIZE_HISTORY
	const char *pLineLast = "<none>";

	if (mIdxLineLast >= 0)
		pLineLast = mCmdInBuf[mIdxLineLast];

	dInfo("Last command\t\t%s\n", pLineLast);
#endif
	bool lineDone = false;
	const char *pCh;
	const char *pCursor = &mCmdInBuf[mIdxLineEdit][mIdxColCursor];
	const char *pCursorMax = &mCmdInBuf[mIdxLineEdit][mIdxColLineEnd];

	dInfo("Buffer\n");
	for (int16_t u = 0; u < cNumCmdInBuffer; ++u)
	{
		lineDone = false;

		if (u == mIdxLineView)
			dInfo("> ");
		else
			dInfo("  ");

		if (u == mIdxLineEdit)
			dInfo("+ ");
		else
			dInfo("  ");

		dInfo("%u ", u);

		dInfo("|");
		for (size_t v = 0; v < cSizeBufCmdIn; ++v)
		{
			pCh = &mCmdInBuf[u][v];

			if (lineDone)
			{
				dInfo(".");
				dInfo("\033[0m");
				continue;
			}

			if (pCh == pCursor)
				dInfo("\033[4m");

			if (pCh == pCursorMax)
				dInfo("\033[4m");

			if (*pCh)
				dInfo("%c", *pCh);
			else
			{
				lineDone = true;
				dInfo(".");
			}

			dInfo("\033[0m");
		}
		dInfo("|\n");
	}
}

void SystemCommanding::globalInit()
{
#if CONFIG_PROC_HAVE_DRIVERS
	Guard lock(mtxGlobalInit);
#endif
	if (globalInitDone)
		return;
	globalInitDone = true;

	/* register standard commands here */
	cmdReg("help",
		helpPrint,
		"h", "this help screen",
		cInternalCmdCls);
#if 0
	cmdReg("broadcast",
		messageBroadcast,
		"b", "broadcast message to other command terminals",
		cInternalCmdCls);

	cmdReg("memWrite",
		memoryWrite,
		"w", "write memory",
		cInternalCmdCls);
#endif
}

Success SystemCommanding::ansiFilter(uint8_t ch, uint16_t *pKeyOut)
{
	uint8_t key = ch;
#if 0
	procWrnLog("mState = %s", KeyStateString[mStateKey]);
#endif
#if 0
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

		if (key == ';')
		{
			mStateKey = StKeyEscSemi;
			break;
		}

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
	case StKeyEscSemi:

		if (key == '5')
		{
			mStateKey = StKeyEscSemi5;
			break;
		}

		return procErrLog(-1, "unexpected key 0x%02X '%c' in state %s",
							key, key, KeyStateString[mStateKey]);

		break;
	case StKeyEscSemi5:

		if (key == 'C')
		{
			mStateKey = StKeyMain;

			*pKeyOut = keyJumpRight;
			return Positive;
		}

		if (key == 'D')
		{
			mStateKey = StKeyMain;

			*pKeyOut = keyJumpLeft;
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

uint32_t SystemCommanding::millis()
{
	auto now = steady_clock::now();
	auto nowMs = time_point_cast<milliseconds>(now);
	return (uint32_t)nowMs.time_since_epoch().count();
}

void SystemCommanding::helpPrint(char *pArgs, char *pBuf, char *pBufEnd)
{
	list<SystemCommand>::iterator iter;
	SystemCommand cmd;
	string group = "";

	(void)pArgs;

	dInfo("\nAvailable commands\n");

	iter = cmds.begin();
	for (; iter != cmds.end(); ++iter)
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

static bool commandSort(SystemCommand &cmdFirst, SystemCommand &cmdSecond)
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
	}

	cmds.push_back(newCmd);
	cmds.sort(commandSort);

	dbgLog(LOG_LVL, "registering command %s: done", id.c_str());
}

