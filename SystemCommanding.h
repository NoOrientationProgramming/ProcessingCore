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

// TODO: Make customizeable
const size_t cNumHistory = 5;
const size_t cSizeBufCmdIn = 63;
const size_t cSizeBufCmdOut = 507;

const size_t cNumCmdInBuffer = 1 + cNumHistory;

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

	void dataReceive();
	void keyProcess(uint16_t key);

	void processInfo(char *pBuf, char *pBufEnd);

	void helpPrint(char *pArgs, char *pBuf, char *pBufEnd);
	void globalInit();
	Success ansiFilter(uint8_t key, uint16_t *pKeyOut);

	/* member variables */
	SOCKET mSocketFd;
	TcpTransfering *mpTrans;
	uint32_t mStateKey;
	uint32_t mStartMs;
	const char *mpLineLast;
	bool mDone;
	char mCmdInBuf[cNumCmdInBuffer][cSizeBufCmdIn];

	/* static functions */

	/* static variables */
	static bool globalInitDone;
#if CONFIG_PROC_HAVE_DRIVERS
	static std::mutex mtxGlobalInit;
#endif
	/* constants */

};

#endif

