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

#ifndef SYSTEM_COMMANDING_H
#define SYSTEM_COMMANDING_H

#include <string>
#include <list>
#include <functional>

#include "Processing.h"
#include "TcpTransfering.h"

// Banana optimization
using FuncCommand = std::function<void (char *pArgs, char *pBuf, char *pBufEnd)>;
#define BIND_MEMBER_FN(fn) [this](auto&&... args) -> decltype(auto) { return this->fn(std::forward<decltype(args)>(args)...); }

struct SystemCommand
{
	std::string id;
	FuncCommand func;
	std::string shortcut;
	std::string desc;
	std::string group;
};

const std::string cInternalCmdCls = "dbg";

void cmdReg(
		const std::string &id,
		FuncCommand cmdFunc,
		const std::string &shortcut = "",
		const std::string &desc = "",
		const std::string &group = "");

//void procReg(const std::string &group, const std::string &id, const std::string &shortcut, COMMANDING PROCESS CREATE FUNCTION, const std::string &desc);

#ifndef CONFIG_CMD_SIZE_HISTORY
#define CONFIG_CMD_SIZE_HISTORY		5
#endif

#ifndef CONFIG_CMD_SIZE_BUFFER_IN
#define CONFIG_CMD_SIZE_BUFFER_IN		29
#endif

#ifndef CONFIG_CMD_SIZE_BUFFER_OUT
#define CONFIG_CMD_SIZE_BUFFER_OUT		507
#endif

const int16_t cNumCmdInBuffer = 1 + CONFIG_CMD_SIZE_HISTORY;
const size_t cSizeBufCmdIn = CONFIG_CMD_SIZE_BUFFER_IN;
const size_t cSizeBufCmdOut = CONFIG_CMD_SIZE_BUFFER_OUT;

const size_t cIdxColMax = cSizeBufCmdIn - 1;

class SystemCommanding : public Processing
{

public:

	static SystemCommanding *create(SOCKET fd)
	{
		return new (std::nothrow) SystemCommanding(fd);
	}

	void modeAutoSet() { mModeAuto = true; }

protected:

	SystemCommanding() : Processing("SystemCommanding") {}
	SystemCommanding(SOCKET fd);
	virtual ~SystemCommanding() {}

private:

	SystemCommanding(const SystemCommanding &) : Processing("") {}
	SystemCommanding &operator=(const SystemCommanding &)
	{
		return *this;
	}

	/*
	 * Naming of functions:  objectVerb()
	 * Example:              peerAdd()
	 */

	/* member functions */
	Success process();
	Success shutdown();

	Success autoCommandReceive();
	void dataReceive();
	void tabProcess();
	void cmdAutoComplete();
	void cmdCandidatesShow();
	void cmdCandidatesGet(std::list<const char *> &listCandidates);
	void lineAck();
	void commandExecute();
#if CONFIG_CMD_SIZE_HISTORY
	void historyInsert();
	bool historyNavigate(uint16_t key);
#endif
	bool bufferEdit(uint16_t key);
	bool chRemove(uint16_t key);
	bool chInsert(uint16_t key);
	bool cursorJump(uint16_t key);
	void promptSend(bool cursor = true, bool preNewLine = false, bool postNewLine = false);

	bool keyIsInsert(uint16_t key);
	bool keyIsAlphaNum(uint16_t key);
	void lfToCrLf(char *pBuf, std::string &str);

	void processInfo(char *pBuf, char *pBufEnd);

	void globalInit();
	Success ansiFilter(uint8_t key, uint16_t *pKeyOut);

	/* member variables */
	SOCKET mSocketFd;
	TcpTransfering *mpTrans;
	uint32_t mStateKey;
	uint32_t mStartMs;
	bool mModeAuto;
	bool mTermChanged;
	bool mDone;
	bool mLastKeyWasTab;
	char mCmdInBuf[cNumCmdInBuffer][cSizeBufCmdIn];
	int16_t mIdxLineEdit;
	int16_t mIdxLineView;
#if CONFIG_CMD_SIZE_HISTORY
	int16_t mIdxLineLast;
#endif
	uint16_t mIdxColCursor;
	uint16_t mIdxColLineEnd;
	char mBufOut[cSizeBufCmdOut];

	/* static functions */
	static uint32_t millis();
	static void cmdHelpPrint(char *pArgs, char *pBuf, char *pBufEnd);
	static void cmdHexDump(char *pArgs, char *pBuf, char *pBufEnd);
	static size_t hexDumpPrint(char *pBuf, char *pBufEnd,
					const void *pData, size_t len,
					const char *pName = NULL, size_t colWidth = 0x10);

	/* static variables */
	static bool globalInitDone;
#if CONFIG_PROC_HAVE_DRIVERS
	static std::mutex mtxGlobalInit;
#endif
	/* constants */

};

#endif

