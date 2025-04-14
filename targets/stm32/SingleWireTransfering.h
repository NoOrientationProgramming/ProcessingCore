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

#ifndef SINGLE_WIRE_TRANSFERING_H
#define SINGLE_WIRE_TRANSFERING_H

#include "Processing.h"

const uint8_t cStartedTrans = 1 << 0;
const uint8_t cStartedDbg = 1 << 1;

const uint8_t cBufValidInCmd = 1 << 0;
const uint8_t cBufValidOutCmd = 1 << 2;
const uint8_t cBufValidOutLog = 1 << 4;
const uint8_t cBufValidOutProc = 1 << 6;

typedef void (*FuncDataSend)(char *pData, size_t len, void *pUser);

const size_t cSzBufInCmd = 64;
const size_t cSzBufOutProc = 1024;
const size_t cSzBufOutLog = 256;
const size_t cSzBufOutCmd = 128;

class SingleWireTransfering : public Processing
{

public:

	static SingleWireTransfering *create()
	{
		return new dNoThrow SingleWireTransfering;
	}

	uint8_t mModeDebug;

	void fctDataSendSet(FuncDataSend pFct, void *pUser);
	void dataReceived(char *pData, size_t len);
	void dataSent();

	bool mSendReady;

	char mBufInCmd[cSzBufInCmd];
	char mBufOutProc[cSzBufOutProc];
	char mBufOutLog[cSzBufOutLog];
	char mBufOutCmd[cSzBufOutCmd];
	uint8_t mValidBuf;

	static uint8_t idStarted;

protected:

	SingleWireTransfering();
	virtual ~SingleWireTransfering() {}

private:

	SingleWireTransfering(const SingleWireTransfering &) = delete;
	SingleWireTransfering &operator=(const SingleWireTransfering &) = delete;

	/*
	 * Naming of functions:  objectVerb()
	 * Example:              peerAdd()
	 */

	/* member functions */
	Success process();
	void processInfo(char *pBuf, char *pBufEnd);

	uint8_t byteReceived(char *pData);

	/* member variables */
	FuncDataSend mpSend;
	void *mpUser;
	char mContentTx;
	uint8_t mValidIdTx;
	char *mpDataTx;
	uint8_t mIdxRx;
	uint16_t mLenSend;

	/* static functions */

	/* static variables */

	/* constants */

};

#endif

