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

#define dBuffValidInCmd			(1 << 0)
#define dBuffValidOutCmd			(1 << 2)
#define dBuffValidOutLog			(1 << 4)
#define dBuffValidOutProc		(1 << 6)

class SingleWireTransfering : public Processing
{

public:

	static SingleWireTransfering *create()
	{
		return new dNoThrow SingleWireTransfering;
	}

	static void dataReceived(uint8_t *pData, size_t len);
	static void dataTransmitted();

	bool mSendReady;

	uint8_t mBufValid;
	char mBufInCmd[64];
	char mBufOutLog[256];
	char mBufOutCmd[128];
	char mBufOutProc[1024];

protected:

	SingleWireTransfering();
	virtual ~SingleWireTransfering() {}

private:

	SingleWireTransfering(const SingleWireTransfering &) : Processing("") {}
	SingleWireTransfering &operator=(const SingleWireTransfering &)
	{
		return *this;
	}

	/*
	 * Naming of functions:  objectVerb()
	 * Example:              peerAdd()
	 */

	/* member functions */
	Success process();
	void processInfo(char *pBuf, char *pBufEnd);

	uint8_t byteReceived(uint8_t *pData);

	/* member variables */
	uint8_t mContentTx;
	uint8_t mValidIdTx;
	char *mpDataTx;
	uint8_t mIdxRx;

	/* static functions */

	/* static variables */
	static uint8_t bufRx[2];
	static uint8_t bufRxIdxIrq;
	static uint8_t bufRxIdxWritten;
	static uint8_t bufTxPending;

	/* constants */

};

#endif

