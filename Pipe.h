/*
  This file is part of the DSP-Crowd project
  https://www.dsp-crowd.com

  Author(s):
      - Johannes Natter, office@dsp-crowd.com

  File created on 28.09.2018

  Copyright (C) 2018, Johannes Natter

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
    - Sender:   sourceDoneSet()
    - Receiver: sinkDoneSet()
  - Main functions
    - connect() / disconnect()     .. Create pipe structure
    - commit()                     .. Add an entry to the queue
    - get()                        .. Get an entry from the queue
    - toPushTry()                  .. Try to push particles to children
*/

#define nowMs()		((uint32_t)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count())

typedef uint32_t ParticleTime;

/* Literature
 * - https://en.cppreference.com/w/cpp/language/rule_of_three
 */
template<typename T>
struct PipeEntry
{
	T particle;
	ParticleTime t1;
	ParticleTime t2;

	// construct / destruct

	PipeEntry()
		: particle()
		, t1()
		, t2()
	{}

	PipeEntry(T p, ParticleTime pt1, ParticleTime pt2)
		: particle(std::move(p))
		, t1(pt1)
		, t2(pt2)
	{}

	~PipeEntry()
	{}

	// copy

	PipeEntry(const PipeEntry& other)
		: particle(other.particle)
		, t1(other.t1)
		, t2(other.t2)
	{}

	PipeEntry& operator=(const PipeEntry& other)
	{
		if (this == &other)
			return *this;

		// delete own data

		particle = other.particle;
		t1 = other.t1;
		t2 = other.t2;

		return *this;
	}

	// move

	PipeEntry(PipeEntry&& other) noexcept
		: particle(std::move(other.particle))
		, t1(other.t1)
		, t2(other.t1)
	{
		other.t1 = 0;
		other.t2 = 0;
	}

	PipeEntry& operator=(PipeEntry&& other) noexcept
	{
		if (this == &other)
			return *this;

		// delete own data

		particle = std::move(other.particle);
		t1 = other.t1;
		t2 = other.t2;

		other.t1 = 0;
		other.t2 = 0;

		return *this;
	}

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

	void dataBlockingSet(bool block)
	{
		mDataBlocking = block;
	}

	virtual bool toPushTry() = 0;

	// optional
	bool sourceDone() const
	{
		return mSourceDone;
	}

	// used by sender
	void sourceDoneSet()
	{
#if CONFIG_PROC_HAVE_DRIVERS
		Guard lock(mEntryMtx);
#endif
		mSourceDone = true;
	}

	bool sinkDone() const
	{
		return mSinkDone;
	}

	// used by receiver
	void sinkDoneSet()
	{
#if CONFIG_PROC_HAVE_DRIVERS
		Guard lock(mEntryMtx);
#endif
		mSinkDone = true;
	}

	bool entriesLeft()
	{
#if CONFIG_PROC_HAVE_DRIVERS
		Guard lock(mEntryMtx);
#endif
		return mSize || !mSourceDone;
	}

protected:
	PipeBase(std::size_t size)
		: mSize(0)
		, mSizeMax(size)
		, mSourceDone(false)
		, mSinkDone(false)
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

	bool mSourceDone;
	bool mSinkDone;
	bool mDataBlocking;

private:
	PipeBase()
	{}

};

template<typename T>
class Pipe : public PipeBase
{

public:
	typedef typename std::list<Pipe<T> *>::iterator PipeListIter;
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

	void parentDisconnect()
	{
#if CONFIG_PROC_HAVE_DRIVERS
		Guard lock(mParentListMtx);
#endif
		PipeListIter iter = mParentList.begin();
		while (iter != mParentList.end())
		{
			(*iter)->childRemove(this);
#if DEBUG_PIPE
			std::cout << this << "->parentDisconnect(" << pParent << ") - 1: " << *iter << std::endl;
#endif
			iter = mParentList.erase(iter);
#if DEBUG_PIPE
			std::cout << this << "->parentDisconnect(" << pParent << ") - 2" << std::endl;
#endif
		}
	}

	ssize_t get(PipeEntry<T> &entry)
	{
#if CONFIG_PROC_HAVE_DRIVERS
		Guard lock(mEntryMtx);
#endif
		if (!mSize && mSourceDone)
			return -1;

		if (!mSize)
			return 0;

		entry = std::move(mEntries.front());
		mEntries.pop();
		--mSize;

		return 1;
	}

	ssize_t commit(T particle, ParticleTime t1 = 0, ParticleTime t2 = 0)
	{
#if CONFIG_PROC_HAVE_DRIVERS
		Guard lock(mEntryMtx);
#endif
		if (mSourceDone || mSinkDone)
			return -1;

		if (mSize >= mSizeMax)
			return 0;

		mEntries.emplace(std::move(particle), t1, t2);
		++mSize;

		return 1;
	}

	bool toPushTry()
	{
#if CONFIG_PROC_HAVE_DRIVERS
		Guard lockChildren(mChildListMtx);
#endif
		PipeListIter iter;
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
			for (; iter != mChildList.end(); ++iter)
			{
				if (!(*iter)->isFull())
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
				--mSize;
			}

			/* transfer entry to all children */
			iter = mChildList.begin();
			for (; iter != mChildList.end(); ++iter)
				(*iter)->commit(entry.particle, entry.t1, entry.t2);

			somethingPushed = true;
		}

		bool nothingLeft = !entriesLeft();

		/* inform children that we will no longer send particles */
		iter = mChildList.begin();
		for (; iter != mChildList.end(); ++iter)
		{
			if (nothingLeft)
				(*iter)->sourceDoneSet();
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
		PipeListIter iter = pList->begin();
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
		PipeListIter iter = mParentList.begin();
		while (iter != mParentList.end())
		{
#if DEBUG_PIPE
			std::cout << this << "->parentRemove(" << pParent << ") - 1: " << *iter << std::endl;
#endif
			if (*iter != pParent)
			{
				++iter;
				continue;
			}

			iter = mParentList.erase(iter);
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

