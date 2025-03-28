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

#include <string.h>

#include "SingleWireTransfering.h"

#define dForEach_ProcState(gen) \
		gen(StStart) \
		gen(StFlowControlByteRcv) \
		gen(StContentIdOutSend) \
		gen(StContentIdOutSendWait) \
		gen(StDataSend) \
		gen(StDataSendDoneWait) \
		gen(StContentIdInRcv) \
		gen(StCmdRcv) \

#define dGenProcStateEnum(s) s,
dProcessStateEnum(ProcState);

#if 1
#define dGenProcStateString(s) #s,
dProcessStateStr(ProcState);
#endif

using namespace std;

char SingleWireTransfering::bufRx[2];
uint8_t SingleWireTransfering::bufRxIdxIrq = 0; // used by IRQ only
uint8_t SingleWireTransfering::bufRxIdxWritten = 0; // set by IRQ, cleared by main
uint8_t SingleWireTransfering::bufTxPending = 0; // set by main, cleared by IRQ

enum SwtFlowControlBytes
{
	FlowMasterSlave = 0xF0,
	FlowSlaveMaster
};

enum SwtContentIdOutBytes
{
	ContentOutNone = 0x00,
	ContentOutLog = 0xC0,
	ContentOutCmd,
	ContentOutProc,
};

enum SwtContentIdInBytes
{
	ContentInCmd = 0xC0,
};

SingleWireTransfering::SingleWireTransfering()
	: Processing("SingleWireTransfering")
	, mSendReady(false)
	, mValidBuf(0)
	, mpSend(NULL)
	, mContentTx(ContentOutNone)
	, mValidIdTx(0)
	, mpDataTx(NULL)
	, mIdxRx(0)
{
	mState = StStart;

	mBufInCmd[0] = 0;
	mBufOutProc[0] = 0;
	mBufOutLog[0] = 0;
	mBufOutCmd[0] = 0;
}

/* member functions */

void SingleWireTransfering::fctDataSendSet(FuncDataSend pFct)
{
	mpSend = pFct;
}

void SingleWireTransfering::dataReceived(char *pData, size_t len)
{
	char *pDest = &bufRx[bufRxIdxIrq];

	*pDest = *pData;
	(void)len;

	bufRxIdxWritten = bufRxIdxIrq + 1;
	bufRxIdxIrq ^= 1;
}

void SingleWireTransfering::dataSent()
{
	bufTxPending = 0;
}

Success SingleWireTransfering::process()
{
	char data;

	switch (mState)
	{
	case StStart:

		if (!mpSend)
			return procErrLog(-1, "send function not set");

		mSendReady = true;

		mState = StFlowControlByteRcv;

		break;
	case StFlowControlByteRcv:

		if (!byteReceived(&data))
			return Pending;

		if (data == FlowMasterSlave)
			mState = StContentIdInRcv;

		if (data == FlowSlaveMaster)
			mState = StContentIdOutSend;

		break;
	case StContentIdOutSend:

		if (mValidBuf & dBufValidOutCmd)
		{
			mValidIdTx = dBufValidOutCmd;
			mpDataTx = mBufOutCmd;
			mContentTx = ContentOutCmd;
		}
		else if (mValidBuf & dBufValidOutLog)
		{
			mValidIdTx = dBufValidOutLog;
			mpDataTx = mBufOutLog;
			mContentTx = ContentOutLog;
		}
		else if (mValidBuf & dBufValidOutProc)
		{
			mValidIdTx = dBufValidOutProc;
			mpDataTx = mBufOutProc;
			mContentTx = ContentOutProc;
		}
		else
			mContentTx = ContentOutNone;

		bufTxPending = 1;
		mpSend(&mContentTx, sizeof(mContentTx));

		mState = StContentIdOutSendWait;

		break;
	case StContentIdOutSendWait:

		if (bufTxPending)
			return Pending;

		if (mContentTx == ContentOutNone)
			mState = StFlowControlByteRcv;
		else
			mState = StDataSend;

		break;
	case StDataSend:

		bufTxPending = 1;
		mpSend(mpDataTx, strlen(mpDataTx) + 1);

		mState = StDataSendDoneWait;

		break;
	case StDataSendDoneWait:

		if (bufTxPending)
			return Pending;

		mValidBuf &= ~mValidIdTx;

		mState = StFlowControlByteRcv;

		break;
	case StContentIdInRcv:

		if (!byteReceived(&data))
			return Pending;

		mIdxRx = 0;

		if (data == ContentInCmd)
			mState = StCmdRcv;
		else
			mState = StFlowControlByteRcv;

		break;
	case StCmdRcv:

		if (!byteReceived(&data))
			return Pending;

		if (mValidBuf & dBufValidInCmd)
		{
			// Consumer not finished. Discard command
			mState = StFlowControlByteRcv;
			return Pending;
		}

		if (mIdxRx == sizeof(mBufInCmd) - 1)
			data = 0;

		mBufInCmd[mIdxRx] = data;
		++mIdxRx;

		if (!data)
		{
			mValidBuf |= dBufValidInCmd;
			mState = StFlowControlByteRcv;
		}

		break;
	default:
		break;
	}

	return Pending;
}

uint8_t SingleWireTransfering::byteReceived(char *pData)
{
	uint8_t idxWr = bufRxIdxWritten;

	if (!idxWr)
		return 0;

	--idxWr;
	*pData = bufRx[idxWr];

	bufRxIdxWritten = 0;

	return 1;
}

void SingleWireTransfering::processInfo(char *pBuf, char *pBufEnd)
{
#if 1
	dInfo("State\t\t%s\n", ProcStateString[mState]);
#else
	(void)pBuf;
	(void)pBufEnd;
#endif
}

/* static functions */

