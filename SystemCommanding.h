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
#include <queue>
#include <mutex>
#include "Processing.h"
#include "TcpTransfering.h"

typedef std::string (*FuncCommand)(const std::string & /* args */);

struct SystemCommand
{
	std::string cls;
	std::string id;
	std::string shortcut;
	std::string desc;
	FuncCommand func;
};

void cmdReg(
		const std::string &cls,
		const std::string &id,
		const std::string &shortcut,
		FuncCommand cmdFunc,
		const std::string &desc);

//void procReg(const std::string &cls, const std::string &id, const std::string &shortcut, COMMANDING PROCESS CREATE FUNCTION, const std::string &desc);

void intCmdReg(
		const std::string &id,
		const std::string &shortcut,
		FuncCommand cmdFunc,
		const std::string &desc);

void unreg(const std::string &id);

class SystemCommanding : public Processing
{

public:

	static SystemCommanding *create(int fd)
	{
		return new (std::nothrow) SystemCommanding(fd);
	}

protected:

	SystemCommanding() : Processing("SystemCommanding") {}
	SystemCommanding(int fd);
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
	Success initialize();
	Success process();
	void inputsProcess();

	void inputAdd();

	void processInfo(char *pBuf, char *pBufEnd);

	/* member variables */
	uint32_t mState;
	uint32_t mStartMs;
	int mSocketFd;
	TcpTransfering *mpTrans;
	std::queue<std::string> mInputs;
	std::mutex mMtxInputs;
	std::string mLastInput;

	/* static functions */
	static std::string commandExecute(const std::string &line);

	static std::string commandRepeat(const std::string &args);
	static std::string dummyExecute(const std::string &args);
	static std::string helpPrint(const std::string &args);
	static std::string messageBroadcast(const std::string &args);
	static std::string memoryWrite(const std::string &args);

	/* static variables */
	static std::mutex mtxGlobalInit;
	static bool globalInitDone;

	/* constants */

};

#endif

