/*
  This file is part of the DSP-Crowd project
  https://www.dsp-crowd.com

  Author(s):
      - Johannes Natter, office@dsp-crowd.com

  File created on 28.09.2018

  Copyright (C) 2018-now Authors and www.dsp-crowd.com

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

#ifndef PIPE_H
#define PIPE_H

#define DEBUG_PIPE	0

#include <list>
#include <queue>
#include <chrono>
#if DEBUG_PIPE
#include <iostream>
#endif

#include "Processing.h"

/*
  What is Pipe?
  - It's a queue of particles and corresponding timestamps
  - With up to one parent and multiple children
  - Thread safe
  - EOF signals can be sent
  - Main functions
    - connect() / disconnect()     .. Create pipe structure
    - front()/pop()                .. Get information about-/remove an entry
    - commit()                     .. Add an entry to the queue
    - toPushTry()                  .. Try to push particles to children
*/

#define nowMs()		((uint32_t)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count())

typedef uint32_t ParticleTime;

template<typename T>
struct PipeEntry
{
	T particle;
	ParticleTime t1;
	ParticleTime t2;
};

class PipeBase
{

public:
	size_t size()
	{
#if CONFIG_PROC_HAVE_DRIVERS
		Guard lock(mEntryMtx);
#endif
		return mSize;
	}

	void sizeMaxSet(size_t size)
	{
		mSizeMax = size;
	}

	size_t sizeMax()
	{
		return mSizeMax;
	}

	bool isEmpty()
	{
#if CONFIG_PROC_HAVE_DRIVERS
		Guard lock(mEntryMtx);
#endif
		return !mSize;
	}

	bool isFull()
	{
#if CONFIG_PROC_HAVE_DRIVERS
		Guard lock(mEntryMtx);
#endif
		return mSize >= mSizeMax;
	}

	void lastSamplesSet()
	{
		mLastEntries = true;
	}

	bool noSamplesLeft()
	{
#if CONFIG_PROC_HAVE_DRIVERS
		Guard lock(mEntryMtx);
#endif
		return !mSize && mLastEntries;
	}

	void dataBlockingSet(bool block)
	{
		mDataBlocking = block;
	}

	virtual bool toPushTry() = 0;

protected:
	PipeBase(std::size_t size)
		: mSize(0)
		, mSizeMax(size)
		, mLastEntries(false)
		, mDataBlocking(true)
	{}

	virtual ~PipeBase()
	{}

#if CONFIG_PROC_HAVE_DRIVERS
	std::mutex mParentListMtx;
	std::mutex mChildListMtx;
	std::mutex mEntryMtx;
#endif
	std::size_t mSize;
	std::size_t mSizeMax;

	bool mLastEntries;
	bool mDataBlocking;

private:
	PipeBase()
	{}

};

template<typename T>
class Pipe : public PipeBase
{

public:
	typedef typename std::list<Pipe<T> *>::iterator QueueIter;
	Pipe()
		: PipeBase(defaultSizeMax)
	{
#if DEBUG_PIPE
		std::cout << "Pipe(): " << this << std::endl;
#endif
	}

	Pipe(std::size_t size)
		: PipeBase(size)
	{
#if DEBUG_PIPE
		std::cout << "Pipe(size_t size): " << this << std::endl;
#endif
	}

	virtual ~Pipe()
	{
#if DEBUG_PIPE
		std::cout << "~Pipe() 1: " << this << std::endl;
#endif
		listDelete();
#if DEBUG_PIPE
		std::cout << "~Pipe() 2: " << this << std::endl;
#endif
		listDelete(true);
#if DEBUG_PIPE
		std::cout << "~Pipe() 3: " << this << std::endl;
#endif
	}

	void connect(Pipe<T> *pChild)
	{
		if (!pChild)
		{
			errLog(-1, "Could not connect to child. No child given");
			return;
		}

		bool parentAccepted = pChild->parentAdd(this);

		if (!parentAccepted)
		{
			errLog(-2, "Could not connect to child. Another parent connected already");
			return;
		}

		childAdd(pChild);
	}

	void disconnect(Pipe<T> *pChild)
	{
		if (!pChild)
		{
			errLog(-1, "Could not disconnect child. No child given");
			return;
		}

		pChild->parentRemove(this);
		childRemove(pChild);
	}

	T front()
	{
#if CONFIG_PROC_HAVE_DRIVERS
		Guard lock(mEntryMtx);
#endif
		PipeEntry<T> entry = mEntries.front();
		return entry.particle;
	}

	ParticleTime frontT1()
	{
#if CONFIG_PROC_HAVE_DRIVERS
		Guard lock(mEntryMtx);
#endif
		PipeEntry<T> entry = mEntries.front();
		return entry.t1;
	}

	ParticleTime frontT2()
	{
#if CONFIG_PROC_HAVE_DRIVERS
		Guard lock(mEntryMtx);
#endif
		PipeEntry<T> entry = mEntries.front();
		return entry.t2;
	}

	bool pop()
	{
#if CONFIG_PROC_HAVE_DRIVERS
		Guard lock(mEntryMtx);
#endif
		if (!mSize)
			return false;

		mEntries.pop();
		--mSize;

		return true;
	}

	bool get(PipeEntry<T> &entry)
	{
#if CONFIG_PROC_HAVE_DRIVERS
		Guard lock(mEntryMtx);
#endif
		if (!mSize)
			return false;

		entry = mEntries.front();
		mEntries.pop();

		--mSize;

		return true;
	}

	bool commit(T particle, ParticleTime t1 = 0, ParticleTime t2 = 0)
	{
#if CONFIG_PROC_HAVE_DRIVERS
		Guard lock(mEntryMtx);
#endif
		if (mSize >= mSizeMax)
			return false;

		PipeEntry<T> entry;

		entry.particle = particle;
		entry.t1 = t1;
		entry.t2 = t2;

		mEntries.push(entry);
		++mSize;

		return true;
	}

	bool toPushTry()
	{
#if CONFIG_PROC_HAVE_DRIVERS
		Guard lockChildren(mChildListMtx);
#endif
		QueueIter iter;
		bool wouldBlock = false;
		bool somethingPushed = false;
		PipeEntry<T> entry;

		while (1)
		{
			{
#if CONFIG_PROC_HAVE_DRIVERS
				Guard lock(mEntryMtx);
#endif
				/* do we have something to send? */
				if (mEntries.empty())
					break;
			}

			if (!mChildList.size())
				break;

			/* are all children ready for this next entry? */
			iter = mChildList.begin();
			while (iter != mChildList.end())
			{
				if (!(*iter++)->isFull())
					continue;

				wouldBlock = true;
				break;
			}

			if (wouldBlock && mDataBlocking)
				break;

			/* this entry will be transfered => remove it */
			{
#if CONFIG_PROC_HAVE_DRIVERS
				Guard lock(mEntryMtx);
#endif
				entry = mEntries.front();
				mEntries.pop();
			}

			/* transfer entry to all children */
			iter = mChildList.begin();
			while (iter != mChildList.end())
				(*iter++)->commit(entry.particle, entry.t1, entry.t2);

			somethingPushed = true;
		}

		bool nothingLeft = noSamplesLeft();

		/* inform children that we will no longer send particles */
		iter = mChildList.begin();
		while (iter != mChildList.end())
		{
			if (nothingLeft)
				(*iter)->lastSamplesSet();

			++iter;
		}

		return somethingPushed;
	}

	static void defaultSizeMaxSet(size_t size)
	{
		defaultSizeMax = size;
	}

private:
	void listDelete(bool parent = false)
	{
#if CONFIG_PROC_HAVE_DRIVERS
		std::mutex *pMtx = &mChildListMtx;
#endif
		std::list<Pipe<T> *> *pList = &mChildList;
		void (Pipe::*pFunc)(Pipe<T> *) = &Pipe<T>::parentRemove;

		if (parent)
		{
#if CONFIG_PROC_HAVE_DRIVERS
			pMtx = &mParentListMtx;
#endif
			pList = &mParentList;
			pFunc = &Pipe<T>::childRemove;
		}
#if CONFIG_PROC_HAVE_DRIVERS
		Guard lock(*pMtx);
#endif
		QueueIter iter = pList->begin();
		while (iter != pList->end())
		{
#if DEBUG_PIPE
			std::cout << "listDelete(): " << *iter << ", " << this << std::endl;
#endif
			((*iter)->*pFunc)(this);
			iter = pList->erase(iter);
		}
	}

	void childAdd(Pipe<T> *pChild)
	{
#if CONFIG_PROC_HAVE_DRIVERS
		Guard lock(mChildListMtx);
#endif
		mChildList.remove(pChild);
		mChildList.push_front(pChild);

#if DEBUG_PIPE
		std::cout << this << "->childAdd(" << pChild << ")" << std::endl;
#endif
	}

	void childRemove(Pipe<T> *pChild)
	{
#if CONFIG_PROC_HAVE_DRIVERS
		Guard lock(mChildListMtx);
#endif
		mChildList.remove(pChild);

#if DEBUG_PIPE
		std::cout << this << "->childRemove(" << pChild << ")" << std::endl;
#endif
	}

	bool parentAdd(Pipe<T> *pParent)
	{
#if CONFIG_PROC_HAVE_DRIVERS
		Guard lock(mParentListMtx);
#endif
		if (mParentList.size())
			return false;

		mParentList.remove(pParent);
		mParentList.push_back(pParent);

#if DEBUG_PIPE
		std::cout << this << "->parentAdd(" << pParent << ")" << std::endl;
#endif

		return true;
	}

	void parentRemove(Pipe<T> *pParent)
	{
#if CONFIG_PROC_HAVE_DRIVERS
		Guard lock(mParentListMtx);
#endif
		QueueIter iter = mParentList.begin();
		while (iter != mParentList.end())
		{
#if DEBUG_PIPE
			std::cout << this << "->parentRemove(" << pParent << ") - 1: " << *iter << std::endl;
#endif
			if (*iter++ != pParent)
				continue;

			mParentList.erase(iter);
#if DEBUG_PIPE
			std::cout << this << "->parentRemove(" << pParent << ") - 2" << std::endl;
#endif
			break;
		}
	}

	std::list<Pipe<T> *> mParentList;
	std::list<Pipe<T> *> mChildList;
	std::queue<PipeEntry<T> > mEntries;

	static size_t defaultSizeMax;

};

template<typename T>
size_t Pipe<T>::defaultSizeMax = 1024;

#endif

