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

typedef void (*CmdFunc)(const char *pArg, char *pBuf, char *pBufEnd);

struct Command
{
	const char *id;
	CmdFunc func;
};

class SystemDebugging : public Processing
{

public:

	static SystemDebugging *create()
	{
		return new (std::nothrow) SystemDebugging;
	}

	void treeRootSet(Processing *pTreeRoot);

	static bool cmdReg(const char *pId, CmdFunc pFunc);

protected:

	SystemDebugging();
	virtual ~SystemDebugging();

private:

	SystemDebugging(const SystemDebugging &) : Processing("") {}
	SystemDebugging &operator=(const SystemDebugging &)
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
	void commandInterpret();
	void procTreeSend();
	void processInfo(char *pBuf, char *pBufEnd);

	/* member variables */
	Processing *mpTreeRoot;
	uint8_t state;

	/* static functions */
	static Command *freeCmdStructGet();

	/* static variables */

	/* constants */

};

#endif

