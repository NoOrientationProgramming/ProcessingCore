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

#include <stdio.h>
#include "SystemCommanding.h"

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

const string cWelcomeMsg = "\n" dPackageName "\n" \
			"System Terminal\n\n" \
			"type 'help' or just 'h' for a list of available commands\n\n" \
			"# ";

const size_t cMaxQueueSize = 256;
const size_t cMaxCmdIdSize = 16;
const string cInternalCmdCls = "dbg";

mutex SystemCommanding::mtxGlobalInit;
bool SystemCommanding::globalInitDone = false;

static list<struct SystemCommand> cmds;
static mutex mtxCmds;
static mutex mtxCmdExec;

SystemCommanding::SystemCommanding(int fd)
	: Processing("SystemCommanding")
	, mState(StStart)
	, mStartMs(0)
	, mSocketFd(fd)
	, mpTrans(NULL)
	, mLastInput("")
{
}

/* member functions */
Success SystemCommanding::initialize()
{
	lock_guard<mutex> lock(mtxGlobalInit);

	if (mSocketFd < 0)
		return procErrLog(-1, "socket file descriptor not set");

	if (!globalInitDone)
	{
		/* register standard commands here */
		intCmdReg("dummy",		"",	dummyExecute,		"dummy command");
		intCmdReg("help",		"h",	helpPrint,		"this help screen");
		intCmdReg("broadcast",	"b",	messageBroadcast,	"broadcast message to other command terminals");
		intCmdReg("memWrite",	"w",	memoryWrite,		"write memory");

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
	//int res;
#if 0
	procWrnLog("mState = %s", ProcStateString[mState]);
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

		inputAdd();
		inputsProcess();

		break;
	case StTmp:

		break;
	default:
		break;
	}

	return Pending;
}

void SystemCommanding::inputsProcess()
{
	string lineInput, resp = "";

	while (true)
	{
		/* execution may take some time.
		 * don't lock input list while executing.
		 * let peers add new requests */
		{
			lock_guard<mutex> lock(mMtxInputs);

			if (!mInputs.size())
				break;
		}

		lineInput = mInputs.front();
		mInputs.pop();

		if (!lineInput.size())
			lineInput = mLastInput;

		resp = commandExecute(lineInput);

		procDbgLog(LOG_LVL, "sending response to: 0x%08X", mpTrans);
		resp = resp + "# ";
		mpTrans->send(resp.c_str(), resp.size());

		mLastInput = lineInput;
	}
}

void SystemCommanding::inputAdd()
{
	ssize_t lenReq, lenPlanned, lenDone;

	(void)lenReq;
	(void)lenPlanned;
	(void)lenDone;

	return;

	if (mInputs.size() >= cMaxQueueSize)
		return;

	lock_guard<mutex> lock(mMtxInputs);

	string cmd;

	if (cmd.back() == '\n')
		cmd.pop_back();

	mInputs.push(cmd); // Important: Stored WITHOUT newline

	procDbgLog(LOG_LVL, "received input: '%s'", cmd.c_str());
}

void SystemCommanding::processInfo(char *pBuf, char *pBufEnd)
{
#if 1
	dInfo("State\t\t\t%s\n", ProcStateString[mState]);
#endif
	//dInfo("Peer\t\t\t%p\n", (void *)mpTrans);
}

/* static functions */
bool cmdSort(struct SystemCommand &cmdFirst, struct SystemCommand &cmdSecond)
{
	if (cmdFirst.cls == cInternalCmdCls and cmdSecond.cls != cInternalCmdCls)
		return true;
	if (cmdFirst.cls != cInternalCmdCls and cmdSecond.cls == cInternalCmdCls)
		return false;

	if (cmdFirst.cls < cmdSecond.cls)
		return true;
	if (cmdFirst.cls > cmdSecond.cls)
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
		const string &cls,
		const string &id,
		const string &shortcut,
		FuncCommand cmdFunc,
		const string &desc)
{
	dbgLog(LOG_LVL, "registering command %s", id.c_str());
	lock_guard<mutex> lock(mtxCmds);

	struct SystemCommand cmd, newCmd = {cls, id, shortcut, desc, cmdFunc};

	for (list<struct SystemCommand>::iterator iter = cmds.begin(); iter != cmds.end(); ++iter)
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
	cmds.sort(cmdSort);

	dbgLog(LOG_LVL, "registering command %s: done", id.c_str());
}

void intCmdReg(const string &id, const string &shortcut, FuncCommand cmdFunc, const string &desc)
{
	cmdReg(cInternalCmdCls, id, shortcut, cmdFunc, desc);
}

void unreg(const string &id)
{
	dbgLog(LOG_LVL, "");
	lock_guard<mutex> lock(mtxCmds);

	(void)id;

	wrnLog("not implemented");

	dbgLog(LOG_LVL, ": done");
}

string SystemCommanding::commandExecute(const string &line)
{
	size_t pos;
	string cmd, args, resp;
	bool cmdFound = false;
	FuncCommand func;

	if (!line.size())
		return "";

	pos = line.find(' ');
	if (pos != string::npos)
	{
		cmd = line.substr(0, pos);
		args = line.substr(pos);
	}
	else
		cmd = line;

	{
		lock_guard<mutex> lock(mtxCmds);

		// Just search, do NOT execute here, because of mutex
		for (list<struct SystemCommand>::iterator iter = cmds.begin(); iter != cmds.end(); ++iter)
		{
			if (cmd != (*iter).id and cmd != (*iter).shortcut)
				continue;

			func = (*iter).func;
			cmdFound = true;
			break;
		}
	}

	if (!cmdFound)
	{
		dbgLog(LOG_LVL, "error: unknown command");
		return "error: unknown command\n";
	}

	dbgLog(LOG_LVL, "executing command: %s", line.c_str());
	{
		lock_guard<mutex> lock(mtxCmdExec);
		resp = func(args);
	}
	dbgLog(LOG_LVL, "executing command: %s: done", cmd.c_str());

	return resp;
}

string SystemCommanding::dummyExecute(const string &args)
{
	(void)args;
	return "";
}

string SystemCommanding::helpPrint(const string &args)
{
	struct SystemCommand cmd;
	string cls = "";
	string resp = "\nAvailable commands\n";

	(void)args;

	for (list<struct SystemCommand>::iterator iter = cmds.begin(); iter != cmds.end(); ++iter)
	{
		cmd = *iter;

		if (cmd.cls != cls)
		{
			resp += "\n";
			if (cmd.cls != cInternalCmdCls)
				resp += cmd.cls + "\n";
			cls = cmd.cls;
		}

		resp += "  ";

		if (cmd.shortcut != "")
			resp += cmd.shortcut.substr(0, 1) + ", ";
		else
			resp += "   ";

		resp += string(cMaxCmdIdSize + 2, ' ').replace(0, cmd.id.length(), cmd.id);
		resp += ".. " + cmd.desc + "\n";
	}

	resp += "\n";

	return resp;
}

string SystemCommanding::messageBroadcast(const string &args)
{
	(void)args;
	return "error: not implemented\n";
}

string SystemCommanding::memoryWrite(const string &args)
{
	(void)args;
	return "error: not implemented\n";
}

#if 0
void SystemCommanding::executeCommand(SystemDebuggingCmd *pCmd)
{
	TcpTransfering *pTrans = mpTrans;
	string cmd, resp;

	switch (pCmd->str[0])
	{
	case '\n':
	case 'b':
		if (pCmd->str.size() > 3)
		{
			string arg(pCmd->str, 2);

			for (PeerIter iter = mPeerList.begin(); iter != mPeerList.end(); ++iter)
			{
				SystemDebuggingPeer peer = *iter;

				if (peer.type != PeerCmd or peer.pTrans == pTrans)
					continue;

				peer.pTrans->send("\n> ", 3);
				peer.pTrans->send(arg.c_str(), arg.size());
				peer.pTrans->send("# ", 2);
			}
		}
		break;
	}

	mLastCmd = *pCmd;

}
#endif
