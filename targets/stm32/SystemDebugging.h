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

#ifndef SYSTEM_DEBUGGING_H
#define SYSTEM_DEBUGGING_H

#include "Processing.h"
#include "SingleWireTransfering.h"

typedef void (*FuncCommand)(char *pArg, char *pBuf, char *pBufEnd);

struct Command
{
	const char *pId;
	FuncCommand pFctExec;
	const char *pShortcut;
	const char *pDesc;
	const char *pGroup;
};

bool cmdReg(
		const char *pId,
		FuncCommand pFct,
		const char *pShortcut = "",
		const char *pDesc = "",
		const char *pGroup = "");

class SystemDebugging : public Processing
{

public:

	static SystemDebugging *create(Processing *pTreeRoot)
	{
		return new dNoThrow SystemDebugging(pTreeRoot);
	}

	void fctDataSendSet(FuncDataSend pFct, void *pUser);
	void dataReceived(char *pData, size_t len);
	void dataSent();

	bool ready();
	bool logOverflowed();

	static void levelLogSet(int lvl);

protected:

	virtual ~SystemDebugging() {}

private:

	SystemDebugging() = delete;
	SystemDebugging(Processing *pTreeRoot);
	SystemDebugging(const SystemDebugging &) = delete;
	SystemDebugging &operator=(const SystemDebugging &) = delete;

	/*
	 * Naming of functions:  objectVerb()
	 * Example:              peerAdd()
	 */

	/* member functions */
	Success process();
	void commandInterpret();
	void procTreeSend();
	void processInfo(char *pBuf, char *pBufEnd);

	/* member variables */
	Processing *mpTreeRoot;
	FuncDataSend mpSend;
	void *mpUser;
	bool mReady;
	uint8_t mStateCmd;
	uint16_t mCntDelay;

	/* static functions */
	static void cmdInfoHelp(char *pArgs, char *pBuf, char *pBufEnd);
	static void entryLogCreate(
			const int severity,
			const char *filename,
			const char *function,
			const int line,
			const int16_t code,
			const char *msg,
			const size_t len);

	/* static variables */

	/* constants */

};

#endif

