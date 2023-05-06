/*
  This file is part of the DSP-Crowd project
  https://www.dsp-crowd.com

  Author(s):
      - Johannes Natter, office@dsp-crowd.com

  File created on 16.09.2022

  Copyright (C) 2022-now Authors and www.dsp-crowd.com

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef TRANSFERING_H
#define TRANSFERING_H

#include <string>
#include <vector>

#include "Processing.h"

typedef std::vector<uint8_t> VecByte;
typedef std::vector<uint8_t>::iterator VecByteIter;

class Transfering : public Processing
{

public:

	virtual ssize_t send(const void *pData, size_t lenReq) = 0;

	virtual ssize_t send(const std::string &str)
	{ return send(str.c_str(), str.size()); }

	virtual ssize_t send(VecByte &pkt)
	{ return send(pkt.data(), pkt.size()); }

	/*
	 * Return value
	 *   > 0 number of bytes read
	 *   = 0 no data at the moment, but data can be expected in the future
	 *   < 0 no data can be expected in the future
	 */
	virtual ssize_t read(void *pBuf, size_t lenReq) = 0;
	Success exactRead(void *pBuf, size_t lenReq)
	{
		if (!lenReq)
			return Positive;

		if (!pBuf)
			return procErrLog(-1, "buffer not set");

		ssize_t lenRead = read(pBuf, lenReq);

		if (!lenRead)
			return Pending;

		if (lenRead < 0)
			return -2;

		if ((size_t)lenRead != lenReq)
			return procErrLog(-3, "read data len does not match. Requested %d, got %d", lenReq, lenRead);

		return Positive;
	}

	void doneSet()
	{
		mDone = true;
	}

	bool mReadReady;
	bool mSendReady;

	virtual const std::string &addrRemote() const
	{
		return mAddrRemote;
	}

protected:

	Transfering(const char *name)
		: Processing(name)
		, mReadReady(false)
		, mSendReady(false)
		, mAddrLocal("")
		, mPortLocal(0)
		, mAddrRemote("")
		, mPortRemote(0)
		, mDone(false)
	{}
	virtual ~Transfering() {}

	std::string mAddrLocal;
	uint16_t mPortLocal;
	std::string mAddrRemote;
	uint16_t mPortRemote;

	bool mDone;

private:

	Transfering() : Processing("") {}
	Transfering(const Transfering &) : Processing("") {}
	Transfering &operator=(const Transfering &) { return *this; }

	/*
	 * Naming of functions:  objectVerb()
	 * Example:              peerAdd()
	 */

	/* member functions */

	/* member variables */

	/* static functions */

	/* static variables */

	/* constants */

};

#endif

