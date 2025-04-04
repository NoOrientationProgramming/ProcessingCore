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
		gen(StFlowControlRcvdWait) \
		gen(StContentIdOutSend) \
		gen(StContentIdOutSentWait) \
		gen(StDataSend) \
		gen(StDataSentWait) \
		gen(StContentIdInRcvdWait) \
		gen(StCmdRcvdWait) \

#define dGenProcStateEnum(s) s,
dProcessStateEnum(ProcState);

#if 0
#define dGenProcStateString(s) #s,
dProcessStateStr(ProcState);
#endif

using namespace std;

char SingleWireTransfering::bufRx[2];
uint8_t SingleWireTransfering::bufRxIdxIrq = 0; // used by IRQ only
uint8_t SingleWireTransfering::bufRxIdxWritten = 0; // set by IRQ, cleared by main
uint8_t SingleWireTransfering::bufTxPending = 0; // set by main, cleared by IRQ

enum SwtFlowDirection
{
	FlowCtrlToTarget = 0xF1,
	FlowTargetToCtrl
};

enum SwtContentId
{
	ContentNone = 0x00,
	ContentLog = 0xA0,
	ContentCmd,
	ContentProc,
};

enum SwtContentIdIn
{
	ContentInCmd = 0x90,
};

SingleWireTransfering::SingleWireTransfering()
	: Processing("SingleWireTransfering")
	, mSendReady(false)
	, mValidBuf(0)
	, mpSend(NULL)
	, mpUser(NULL)
	, mContentTx(ContentNone)
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

void SingleWireTransfering::fctDataSendSet(FuncDataSend pFct, void *pUser)
{
	mpSend = pFct;
	mpUser = pUser;
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

		mState = StFlowControlRcvdWait;

		break;
	case StFlowControlRcvdWait:

		if (!byteReceived(&data))
			return Pending;

		if (data == FlowCtrlToTarget)
			mState = StContentIdInRcvdWait;

		if (data == FlowTargetToCtrl)
			mState = StContentIdOutSend;

		break;
	case StContentIdOutSend:

		if (mValidBuf & dBufValidOutCmd)
		{
			mValidIdTx = dBufValidOutCmd;
			mpDataTx = mBufOutCmd;
			mContentTx = ContentCmd;
		}
		else if (mValidBuf & dBufValidOutLog)
		{
			mValidIdTx = dBufValidOutLog;
			mpDataTx = mBufOutLog;
			mContentTx = ContentLog;
		}
		else if (mValidBuf & dBufValidOutProc)
		{
			mValidIdTx = dBufValidOutProc;
			mpDataTx = mBufOutProc;
			mContentTx = ContentProc;
		}
		else
			mContentTx = ContentNone;

		bufTxPending = 1;
		mpSend(&mContentTx, sizeof(mContentTx), mpUser);

		mState = StContentIdOutSentWait;

		break;
	case StContentIdOutSentWait:

		if (bufTxPending)
			return Pending;

		if (mContentTx == ContentNone)
			mState = StFlowControlRcvdWait;
		else
			mState = StDataSend;

		break;
	case StDataSend:

		bufTxPending = 1;
		mpSend(mpDataTx, strlen(mpDataTx) + 1, mpUser);

		mState = StDataSentWait;

		break;
	case StDataSentWait:

		if (bufTxPending)
			return Pending;

		mValidBuf &= ~mValidIdTx;

		mState = StFlowControlRcvdWait;

		break;
	case StContentIdInRcvdWait:

		if (!byteReceived(&data))
			return Pending;

		mIdxRx = 0;

		if (data == ContentInCmd)
			mState = StCmdRcvdWait;
		else
			mState = StFlowControlRcvdWait;

		break;
	case StCmdRcvdWait:

		if (!byteReceived(&data))
			return Pending;

		if (mValidBuf & dBufValidInCmd)
		{
			// Consumer not finished. Discard command
			mState = StFlowControlRcvdWait;
			return Pending;
		}

		if (mIdxRx == sizeof(mBufInCmd) - 1)
			data = 0;

		mBufInCmd[mIdxRx] = data;
		++mIdxRx;

		if (!data)
		{
			mValidBuf |= dBufValidInCmd;
			mState = StFlowControlRcvdWait;
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
#if 0
	dInfo("State\t\t%s\n", ProcStateString[mState]);
#else
	(void)pBuf;
	(void)pBufEnd;
#endif
}

/* static functions */

