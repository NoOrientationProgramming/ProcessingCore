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

enum SwtContentIdIn
{
	ContentInCmd = 0x90,
};

enum SwtContentId
{
	ContentNone = 0xA0,
	ContentProc,
	ContentLog,
	ContentCmd,
};

enum SwtContentEnd
{
	ContentCut = 0x0F,
	ContentEnd = 0x17,
};

SingleWireTransfering::SingleWireTransfering()
	: Processing("SingleWireTransfering")
	, mModeDebug(0)
	, mSendReady(false)
	, mValidBuf(0)
	, mpSend(NULL)
	, mpUser(NULL)
	, mContentTx(ContentNone)
	, mValidIdTx(0)
	, mpDataTx(NULL)
	, mIdxRx(0)
	, mLenSend(0)
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
			break;

		if (data == FlowCtrlToTarget)
			mState = StContentIdInRcvdWait;

		if (!mModeDebug)
			break;

		if (data == FlowTargetToCtrl)
			mState = StContentIdOutSend;

		break;
	case StContentIdOutSend:

		if (mValidBuf & dBufValidOutCmd) // highest prio
		{
			mValidIdTx = dBufValidOutCmd;
			mContentTx = ContentCmd;
			mpDataTx = mBufOutCmd;
			mLenSend = sizeof(mBufOutCmd);
		}
		else if (mValidBuf & dBufValidOutLog)
		{
			mValidIdTx = dBufValidOutLog;
			mContentTx = ContentLog;
			mpDataTx = mBufOutLog;
			mLenSend = sizeof(mBufOutLog);
		}
		else if (mValidBuf & dBufValidOutProc) // lowest prio
		{
			mValidIdTx = dBufValidOutProc;
			mContentTx = ContentProc;
			mpDataTx = mBufOutProc;
			mLenSend = sizeof(mBufOutProc);
		}
		else
			mLenSend = 0;

		if (mLenSend < 2)
			mContentTx = ContentNone;

		bufTxPending = 1;
		mpSend(&mContentTx, sizeof(mContentTx), mpUser);

		mState = StContentIdOutSentWait;

		break;
	case StContentIdOutSentWait:

		if (bufTxPending)
			break;

		if (mContentTx == ContentNone)
		{
			mState = StFlowControlRcvdWait;
			break;
		}

		// protect strlen(). Zero byte and 'message end' identifier byte must be stored at least
		mLenSend -= 2;
		mpDataTx[mLenSend] = 0;

		mLenSend = strlen(mpDataTx);

		mpDataTx[mLenSend] = 0;
		++mLenSend;

		mpDataTx[mLenSend] = ContentEnd;
		++mLenSend;

		mState = StDataSend;

		break;
	case StDataSend:

		bufTxPending = 1;
		mpSend(mpDataTx, mLenSend, mpUser);

		mState = StDataSentWait;

		break;
	case StDataSentWait:

		if (bufTxPending)
			break;

		mValidBuf &= ~mValidIdTx;

		mState = StFlowControlRcvdWait;

		break;
	case StContentIdInRcvdWait:

		if (!byteReceived(&data))
			break;

		if (data == ContentInCmd && !(mValidBuf & dBufValidInCmd))
		{
			mIdxRx = 0;
			mBufInCmd[mIdxRx] = 0;

			mState = StCmdRcvdWait;
			break;
		}

		mState = StFlowControlRcvdWait;

		break;
	case StCmdRcvdWait:

		if (!byteReceived(&data))
			break;

		if (data == FlowTargetToCtrl)
		{
			mBufInCmd[0] = 0;
			mState = StContentIdOutSend;
			break;
		}

		if (data == ContentEnd)
		{
			mBufInCmd[mIdxRx] = 0;
			mValidBuf |= dBufValidInCmd;

			mState = StFlowControlRcvdWait;
			break;
		}

		if (mIdxRx >= sizeof(mBufInCmd) - 1)
			break;

		mBufInCmd[mIdxRx] = data;
		++mIdxRx;

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

