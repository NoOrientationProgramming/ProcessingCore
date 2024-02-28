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

// Temp
#define CONFIG_CMD_SIZE_HISTORY		3
#define CONFIG_CMD_SIZE_BUFFER_IN		9

//void procReg(const std::string &group, const std::string &id, const std::string &shortcut, COMMANDING PROCESS CREATE FUNCTION, const std::string &desc);

#ifndef CONFIG_CMD_SIZE_HISTORY
#define CONFIG_CMD_SIZE_HISTORY		5
#endif

#ifndef CONFIG_CMD_SIZE_BUFFER_IN
#define CONFIG_CMD_SIZE_BUFFER_IN		63
#endif

#ifndef CONFIG_CMD_SIZE_BUFFER_OUT
#define CONFIG_CMD_SIZE_BUFFER_OUT		507
#endif

const size_t cNumCmdInBuffer = 1 + CONFIG_CMD_SIZE_HISTORY;
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
	bool bufferChange(uint16_t key);
	bool chRemove(uint16_t key);

	bool keyIsInsert(uint16_t key);

	void processInfo(char *pBuf, char *pBufEnd);

	void globalInit();
	Success ansiFilter(uint8_t key, uint16_t *pKeyOut);

	/* member variables */
	SOCKET mSocketFd;
	TcpTransfering *mpTrans;
	uint32_t mStateKey;
	uint32_t mStartMs;
	bool mCursorHidden;
	bool mDone;
	char mCmdInBuf[cNumCmdInBuffer][cSizeBufCmdIn];
	int16_t mIdxLineCurrent;
	int16_t mIdxLineLast;
	uint16_t mIdxColCurrent;
	uint16_t mIdxColMax;

	/* static functions */
	static uint32_t millis();
	static void helpPrint(char *pArgs, char *pBuf, char *pBufEnd);

	/* static variables */
	static bool globalInitDone;
#if CONFIG_PROC_HAVE_DRIVERS
	static std::mutex mtxGlobalInit;
#endif
	/* constants */

};

#endif

