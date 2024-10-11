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

using namespace std;

extern UART_HandleTypeDef huart1;

uint8_t SingleWireTransfering::buffRx[2];
uint8_t SingleWireTransfering::buffRxIdxIrq = 0; // used by IRQ only
uint8_t SingleWireTransfering::buffRxIdxWritten = 0; // set by IRQ, cleared by main
uint8_t SingleWireTransfering::buffTxPending = 0;

enum SingleWireState
{
	FlowControlByteRcv = 0,
	ContentIdOutSend,
	ContentIdOutSendWait,
	DataSend,
	DataSendDoneWait,
	ContentIdInRcv,
	CmdRcv
};

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
	, state(FlowControlByteRcv)
{
}

SingleWireTransfering::~SingleWireTransfering()
{
}

/* member functions */
Success SingleWireTransfering::initialize()
{
	HAL_UART_Receive_IT(&huart1, SingleWireTransfering::buffRx, 1);

	return Positive;
}

Success SingleWireTransfering::process()
{
	uint8_t data;

	switch (state)
	{
	case FlowControlByteRcv:

		if (!byteReceived(&data))
			return Pending;

		if (data == FlowMasterSlave)
			state = ContentIdInRcv;

		if (data == FlowSlaveMaster)
			state = ContentIdOutSend;

		break;
	case ContentIdOutSend:

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

		state = ContentIdOutSendWait;

		break;
	case ContentIdOutSendWait:

		if (SingleWireTransfering::buffTxPending)
			return Pending;

		if (contentTx == ContentOutNone)
			state = FlowControlByteRcv;
		else
			state = DataSend;

		break;
	case DataSend:

		SingleWireTransfering::buffTxPending = 1;
		HAL_UART_Transmit_IT(&huart1, (uint8_t *)pDataTx, strlen(pDataTx) + 1);

		state = DataSendDoneWait;

		break;
	case DataSendDoneWait:

		if (SingleWireTransfering::buffTxPending)
			return Pending;

		pEnv->buffValid &= ~validIdTx;

		state = FlowControlByteRcv;

		break;
	case ContentIdInRcv:

		if (!byteReceived(&data))
			return Pending;

		idxRx = 0;

		if (data == ContentInCmd)
			state = CmdRcv;
		else
			state = FlowControlByteRcv;

		break;
	case CmdRcv:

		if (!byteReceived(&data))
			return Pending;

		if (pEnv->buffValid & dBuffValidInCmd)
		{
			// Consumer not finished. Discard command
			state = FlowControlByteRcv;
			return Pending;
		}

		if (idxRx == sizeof(pEnv->buffInCmd) - 1)
			data = 0;

		pEnv->buffInCmd[idxRx] = data;
		++idxRx;

		if (!data)
		{
			pEnv->buffValid |= dBuffValidInCmd;
			state = FlowControlByteRcv;
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
	(void)pBuf;
	(void)pBufEnd;
}

/* static functions */
extern "C" void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	SingleWireTransfering::buffRxIdxWritten = SingleWireTransfering::buffRxIdxIrq + 1;
	SingleWireTransfering::buffRxIdxIrq ^= 1;
	HAL_UART_Receive_IT(&huart1, &SingleWireTransfering::buffRx[SingleWireTransfering::buffRxIdxIrq], 1);

	//HAL_GPIO_WritePin(LED_Blue_GPIO_Port, LED_Blue_Pin, dLedSet);
	//HAL_GPIO_WritePin(LED_Blue_GPIO_Port, LED_Blue_Pin, dLedClear);
}

extern "C" void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
	(void)huart;

	SingleWireTransfering::buffTxPending = 0;

	//HAL_GPIO_WritePin(LED_Blue_GPIO_Port, LED_Blue_Pin, dLedSet);
	//HAL_GPIO_WritePin(LED_Blue_GPIO_Port, LED_Blue_Pin, dLedClear);

	//HAL_GPIO_WritePin(LED_Blue_GPIO_Port, LED_Blue_Pin, dLedSet);
	//HAL_GPIO_WritePin(LED_Blue_GPIO_Port, LED_Blue_Pin, dLedClear);
}
