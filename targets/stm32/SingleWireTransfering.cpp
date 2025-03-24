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

uint8_t SingleWireTransfering::buffRx[2];
uint8_t SingleWireTransfering::buffRxIdxIrq = 0; // used by IRQ only
uint8_t SingleWireTransfering::buffRxIdxWritten = 0; // set by IRQ, cleared by main
uint8_t SingleWireTransfering::buffTxPending = 0;

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
{
	mState = StStart;
}

/* member functions */

Success SingleWireTransfering::process()
{
	uint8_t data;

	switch (mState)
	{
	case StStart:

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

		if (pEnv->buffValid & dBuffValidOutCmd)
		{
			validIdTx = dBuffValidOutCmd;
			pDataTx = pEnv->buffOutCmd;
			contentTx = ContentOutCmd;
		}
		else if (pEnv->buffValid & dBuffValidOutLog)
		{
			validIdTx = dBuffValidOutLog;
			pDataTx = pEnv->buffOutLog;
			contentTx = ContentOutLog;
		}
		else if (pEnv->buffValid & dBuffValidOutProc)
		{
			validIdTx = dBuffValidOutProc;
			pDataTx = pEnv->buffOutProc;
			contentTx = ContentOutProc;
		}
		else
			contentTx = ContentOutNone;

		SingleWireTransfering::buffTxPending = 1;
		HAL_UART_Transmit_IT(&huart1, &contentTx, 1);

		mState = StContentIdOutSendWait;

		break;
	case StContentIdOutSendWait:

		if (SingleWireTransfering::buffTxPending)
			return Pending;

		if (contentTx == ContentOutNone)
			mState = StFlowControlByteRcv;
		else
			mState = StDataSend;

		break;
	case StDataSend:

		SingleWireTransfering::buffTxPending = 1;
		HAL_UART_Transmit_IT(&huart1, (uint8_t *)pDataTx, strlen(pDataTx) + 1);

		mState = StDataSendDoneWait;

		break;
	case StDataSendDoneWait:

		if (SingleWireTransfering::buffTxPending)
			return Pending;

		pEnv->buffValid &= ~validIdTx;

		mState = StFlowControlByteRcv;

		break;
	case StContentIdInRcv:

		if (!byteReceived(&data))
			return Pending;

		idxRx = 0;

		if (data == ContentInCmd)
			mState = StCmdRcv;
		else
			mState = StFlowControlByteRcv;

		break;
	case StCmdRcv:

		if (!byteReceived(&data))
			return Pending;

		if (pEnv->buffValid & dBuffValidInCmd)
		{
			// Consumer not finished. Discard command
			mState = StFlowControlByteRcv;
			return Pending;
		}

		if (idxRx == sizeof(pEnv->buffInCmd) - 1)
			data = 0;

		pEnv->buffInCmd[idxRx] = data;
		++idxRx;

		if (!data)
		{
			pEnv->buffValid |= dBuffValidInCmd;
			mState = StFlowControlByteRcv;
		}

		break;
	default:
		break;
	}

	return Pending;
}

uint8_t SingleWireTransfering::byteReceived(uint8_t *pData)
{
	uint8_t idxWr = SingleWireTransfering::buffRxIdxWritten;

	if (!idxWr)
		return 0;

	--idxWr;
	*pData = SingleWireTransfering::buffRx[idxWr];

	SingleWireTransfering::buffRxIdxWritten = 0;

	return 1;
}

void SingleWireTransfering::processInfo(char *pBuf, char *pBufEnd)
{
#if 1
	dInfo("State\t\t\t%s\n", ProcStateString[mState]);
#else
	(void)pBuf;
	(void)pBufEnd;
#endif
}

/* static functions */

