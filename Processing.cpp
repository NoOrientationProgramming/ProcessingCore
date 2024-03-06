/*
  This file is part of the DSP-Crowd project
  https://www.dsp-crowd.com

  Author(s):
      - Johannes Natter, office@dsp-crowd.com

  File created on 14.09.2018

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

#include "Processing.h"

#if CONFIG_PROC_HAVE_DRIVERS
#define CONFIG_PROC_TITLE_NEW_DRIVER
#if defined(__linux__)
#include <sys/prctl.h>
#elif defined(__FreeBSD__)
#include <unistd.h>
#else
/* not implemented */
#undef CONFIG_PROC_TITLE_NEW_DRIVER
#endif
#endif

enum ProcessState
{
	PsExistent = 0,
	PsInitializing,
	PsProcessing,
	PsDownShutting,
	PsChildrenUnusedSet,
	PsFinishedPrepare,
	PsFinished,
};

enum ProcStatBitParent
{
	PsbParStarted = 1,
	PsbParCanceled = 2,
	PsbParUnused = 4,
	PsbParWhenFinishedUnused = 8,
};

enum ProcStatBitDriver
{
	PsbDrvInitDone = 1,
	PsbDrvProcessDone = 2,
	PsbDrvShutdownDone = 4,
	PsbDrvUndriven = 8,
	PsbDrvPrTreeDisable = 16,
};

#if CONFIG_PROC_HAVE_LIB_STD_CPP || CONFIG_PROC_HAVE_DRIVERS
using namespace std;
#endif

#define LOG_LVL	1

#if CONFIG_PROC_HAVE_LIB_STD_CPP
typedef list<Processing *>::iterator ChildIter;
#endif

uint8_t Processing::showAddressInId = CONFIG_PROC_SHOW_ADDRESS_IN_ID;
uint8_t Processing::disableTreeDefault = CONFIG_PROC_DISABLE_TREE_DEFAULT;

#if CONFIG_PROC_HAVE_GLOBAL_DESTRUCTORS
#if CONFIG_PROC_HAVE_LIB_STD_CPP
list<GlobDestructorFunc> Processing::globalDestructors;
#else
GlobDestructorFunc *Processing::pGlobalDestructors = NULL;
#endif
#endif

/* Literature
 * - http://man7.org/linux/man-pages/man5/proc.5.html
 * - https://stackoverflow.com/questions/6261201/how-to-find-memory-leak-in-a-c-code-project
 */

// This area is used by the client

void Processing::treeTick()
{
	// No need to lock child list here

	Processing *pChild = NULL;
	Success success;
	bool childCanBeRemoved;

#if CONFIG_PROC_HAVE_LIB_STD_CPP
	ChildIter iter = mChildList.begin();
	while (iter != mChildList.end())
	{
		pChild = *iter;
#else
	Processing **pChildListElem = mpChildList;
	while (pChildListElem and *pChildListElem)
	{
		pChild = *pChildListElem;
#endif
		parentalDrive(pChild);

		childCanBeRemoved = pChild->mStatDrv & PsbDrvUndriven and
						pChild->mStatParent & PsbParUnused;

		if (!childCanBeRemoved)
		{
#if CONFIG_PROC_HAVE_LIB_STD_CPP
			++iter;
#else
			++pChildListElem;
#endif
			continue;
		}

		char childId[CONFIG_PROC_ID_BUFFER_SIZE];
		procId(childId, childId + sizeof(childId), pChild);

		procDbgLog(LOG_LVL, "removing %s from child list", childId);
		{
#if CONFIG_PROC_HAVE_DRIVERS
			procDbgLog(LOG_LVL, "Locking mChildListMtx");
			Guard lock(mChildListMtx);
			procDbgLog(LOG_LVL, "Locking mChildListMtx: done");
#endif
#if CONFIG_PROC_HAVE_LIB_STD_CPP
			iter = mChildList.erase(iter);
			--mNumChildren;
#else
			pChildListElem = childElemErase(pChildListElem);
#endif
		}
		procDbgLog(LOG_LVL, "removing %s from child list: done", childId);

		destroy(pChild);
	}

	// Only after this point children can be created or destroyed
	// and therefore added or removed from the child list

	switch (mStateAbstract)
	{
	case PsExistent:

#if CONFIG_PROC_HAVE_DRIVERS
// cpp -dM /dev/null
#if defined(CONFIG_PROC_TITLE_NEW_DRIVER)
		// Only for worker threads
		if (mDriver == DrivenByNewInternalDriver)
		{
			char buf[32];
			char *pBuf = buf;
			char *pBufEnd = pBuf + sizeof(buf);

			dInfo("%p", (const void *)this);
#if defined(__linux__)
			int res;
			res = prctl(PR_SET_NAME, buf, 0, 0, 0);
			if (res < 0)
				procWrnLog("could not set driver name via prctl()");
#elif defined(__FreeBSD__)
			setproctitle("%s", buf);
#endif
		}
#endif
#endif
		if (mStatParent & PsbParCanceled)
		{
			procDbgLog(LOG_LVL, "process canceled during state existent");
			mStateAbstract = PsFinishedPrepare;
			break;
		}

		procDbgLog(LOG_LVL, "initializing()");
		mStateAbstract = PsInitializing;

		break;
	case PsInitializing:

		if (mStatParent & PsbParCanceled)
		{
			procDbgLog(LOG_LVL, "process canceled during initializing");
			procDbgLog(LOG_LVL, "downShutting()");
			mStateAbstract = PsDownShutting;
			break;
		}

		success = initialize(); // child list may be changed here

		if (success == Pending)
			break;

		if (success != Positive)
		{
			mSuccess = success;
			procDbgLog(LOG_LVL, "initializing(): failed. success = %d", (int)mSuccess);
			procDbgLog(LOG_LVL, "downShutting()");
			mStateAbstract = PsDownShutting;
			break;
		}

		procDbgLog(LOG_LVL, "initializing(): done");
		mStatDrv |= PsbDrvInitDone;

		procDbgLog(LOG_LVL, "processing()");
		mStateAbstract = PsProcessing;

		break;
	case PsProcessing:

		if (mStatParent & PsbParCanceled)
		{
			procDbgLog(LOG_LVL, "process canceled during processing");
			procDbgLog(LOG_LVL, "downShutting()");
			mStateAbstract = PsDownShutting;
			break;
		}

		success = process(); // child list may be changed here

		if (success == Pending)
			break;

		mSuccess = success;

		procDbgLog(LOG_LVL, "processing(): done. success = %d", (int)mSuccess);
		mStatDrv |= PsbDrvProcessDone;

		procDbgLog(LOG_LVL, "downShutting()");
		mStateAbstract = PsDownShutting;

		break;
	case PsDownShutting:

		success = shutdown(); // child list may be changed here

		if (success == Pending)
			break;

		procDbgLog(LOG_LVL, "downShutting(): done");
		mStatDrv |= PsbDrvShutdownDone;

		mStateAbstract = PsChildrenUnusedSet;

		break;
	case PsChildrenUnusedSet:

		procDbgLog(LOG_LVL, "marking children as unused");
#if CONFIG_PROC_HAVE_LIB_STD_CPP
		iter = mChildList.begin();
		while (iter != mChildList.end())
		{
			pChild = *iter++;
#else
		pChildListElem = mpChildList;
		while (pChildListElem and *pChildListElem)
		{
			pChild = *pChildListElem++;
#endif
			pChild->unusedSet();
		}
		procDbgLog(LOG_LVL, "marking children as unused: done");

		mStateAbstract = PsFinishedPrepare;

		break;
	case PsFinishedPrepare:

		procDbgLog(LOG_LVL, "preparing finish");

		if (mStatParent & PsbParWhenFinishedUnused)
		{
			procDbgLog(LOG_LVL, "set process as unused when finished");
			unusedSet();
		}

		procDbgLog(LOG_LVL, "preparing finish: done -> finished");

		mStateAbstract = PsFinished;

		break;
	case PsFinished:

		break;
	default:
		break;
	}
}

bool Processing::progress() const
{
	return mStateAbstract != PsFinished or mNumChildren;
}

Success Processing::success() const
{
	return mSuccess;
}

void Processing::unusedSet()
{
	uint8_t flags = PsbParCanceled | PsbParUnused;
	mStatParent |= flags;
}

void Processing::procTreeDisplaySet(bool display)
{
	if (display)
		mStatDrv &= ~PsbDrvPrTreeDisable;
	else
		mStatDrv |= PsbDrvPrTreeDisable;
}

bool Processing::initDone() const		{ return mStatDrv & PsbDrvInitDone;	}
bool Processing::processDone() const	{ return mStatDrv & PsbDrvProcessDone;	}
bool Processing::shutdownDone() const	{ return mStatDrv & PsbDrvShutdownDone;	}

size_t Processing::processTreeStr(char *pBuf, char *pBufEnd, bool detailed, bool colored)
{
	Processing *pChild = NULL;
	static char bufInfo[CONFIG_PROC_INFO_BUFFER_SIZE];
	char *pBufStart = pBuf;
	char *pBufLineStart, *pBufIter;
	int8_t n, lastChildInfoLine;
	int8_t cntChildDrawn;

	if (mStatDrv & PsbDrvPrTreeDisable)
		return 0;

	if (!pBuf or !(pBufEnd - pBuf))
		return 0;

	for (n = 0; n < 2 * mLevelTree; ++n)
		dInfo(" ");

	if (mSuccess == Pending)
		dInfo("-");
	else if (mSuccess == Positive)
		dInfo("+");
	else
		dInfo("x");

	dInfo(" ");

	if (mDriver == DrivenByExternalDriver)
	{
#if CONFIG_PROC_USE_DRIVER_COLOR
		if (colored)
			dInfo("\033[95m");
		else
#endif
			dInfo("### ");
	}

#if CONFIG_PROC_USE_DRIVER_COLOR
	if (colored and !mLevelDriver)
		dInfo("\033[32m");
#endif

	if (mDriver == DrivenByNewInternalDriver)
	{
#if CONFIG_PROC_USE_DRIVER_COLOR
		if (colored)
			dInfo("\033[36m");
		else
#endif
			dInfo("*** ");
	}

	pBuf += procId(pBuf, pBufEnd, this);
	dInfo("()\r\n");

#if CONFIG_PROC_USE_DRIVER_COLOR
	if (colored)
		dInfo("\033[37m");
#endif

	if (detailed and mStateAbstract != PsFinished)
	{
		bufInfo[0] = 0;
		processInfo(bufInfo, bufInfo + sizeof(bufInfo));
		bufInfo[sizeof(bufInfo) - 1] = 0;

		pBufLineStart = pBufIter = bufInfo;
		lastChildInfoLine = 0;

		while (1)
		{
			if (pBufIter >= bufInfo + sizeof(bufInfo))
				break;

			if (!bufInfo[0])
				break;

			if (*pBufIter and *pBufIter != '\n')
			{
				++pBufIter;
				continue;
			}

			if (!*pBufIter)
			{
				if (!*(pBufIter - 1)) // last line drawn already
					break;

				lastChildInfoLine = 1;
			}

			for (n = 0; n < 2 * mLevelTree + 2; ++n)
				dInfo(" ");

			*pBufIter = 0; // terminate current line starting at pBufLineStart

			dInfo("%s\r\n", pBufLineStart);

			++pBufIter;
			pBufLineStart = pBufIter;

			if (lastChildInfoLine)
				break;
		}
	}

	cntChildDrawn = 0;

	{
#if CONFIG_PROC_HAVE_DRIVERS
		Guard lock(mChildListMtx);
#endif
#if CONFIG_PROC_HAVE_LIB_STD_CPP
		ChildIter iter = mChildList.begin();
		while (iter != mChildList.end())
		{
			pChild = *iter++;
#else
		Processing **pChildListElem = mpChildList;
		while (pChildListElem and *pChildListElem)
		{
			pChild = *pChildListElem++;
#endif
			pBuf += pChild->processTreeStr(pBuf, pBufEnd, detailed, colored);

			++cntChildDrawn;

			if (cntChildDrawn < 11)
				continue;

			for (n = 0; n < 2 * mLevelTree + 2; ++n)
				dInfo(" ");

			dInfo("..\r\n");

			break;
		}
	}

	return pBuf - pBufStart;
}

void Processing::undrivenSet(Processing *pChild)
{
	pChild->mStatDrv |= PsbDrvUndriven;
}

void Processing::destroy(Processing *pChild)
{
	char childId[CONFIG_PROC_ID_BUFFER_SIZE];
	procId(childId, childId + sizeof(childId), pChild);

	dbgLog(LOG_LVL, "child %s destroy()", childId);

	if (pChild->mNumChildren)
		errLog(-1, "destroying child with grand children");

#if !CONFIG_PROC_HAVE_LIB_STD_CPP
	if (pChild->mpChildList)
	{
		dbgLog(LOG_LVL, "child %s deleting child list", childId);
		delete[] pChild->mpChildList;
		pChild->mpChildList = NULL;
		dbgLog(LOG_LVL, "child %s deleting child list: done", childId);
	}
#endif

#if CONFIG_PROC_HAVE_DRIVERS
	if (pChild->mpThread)
	{
		dbgLog(LOG_LVL, "thread join()");
		if (pChild->mpThread->joinable())
			pChild->mpThread->join();
		dbgLog(LOG_LVL, "thread join(): done");

		dbgLog(LOG_LVL, "thread delete()");
		delete pChild->mpThread;
		pChild->mpThread = NULL;
		dbgLog(LOG_LVL, "thread delete(): done");
	}
#endif

	dbgLog(LOG_LVL, "child %s delete()", childId);
	delete pChild;
	dbgLog(LOG_LVL, "child %s delete(): done", childId);

	dbgLog(LOG_LVL, "child %s destroy(): done", childId);
}

void Processing::applicationClose()
{
	dbgLog(LOG_LVL, "closing application");

#if CONFIG_PROC_HAVE_GLOBAL_DESTRUCTORS
	dbgLog(LOG_LVL, "executing global destructors");
#if CONFIG_PROC_HAVE_LIB_STD_CPP
	list<GlobDestructorFunc>::iterator iter = globalDestructors.begin();

	while (iter != globalDestructors.end())
		(*iter++)();

	globalDestructors.clear();
#else
	GlobDestructorFunc *pGlobDestrListElem = pGlobalDestructors;

	while (pGlobDestrListElem and *pGlobDestrListElem)
		(*pGlobDestrListElem++)();

	if (pGlobalDestructors)
		delete[] pGlobalDestructors;
#endif
	dbgLog(LOG_LVL, "executing global destructors: done");
#else
	dbgLog(LOG_LVL, "global destructors disabled");
#endif

	dbgLog(LOG_LVL, "closing application: done");
}

void Processing::globalDestructorRegister(GlobDestructorFunc globDestr)
{
#if CONFIG_PROC_HAVE_GLOBAL_DESTRUCTORS
	dbgLog(LOG_LVL, "");
#if CONFIG_PROC_HAVE_LIB_STD_CPP
	globalDestructors.push_front(globDestr);
	globalDestructors.unique();
	dbgLog(LOG_LVL, ": done");
#else
	GlobDestructorFunc *pGlobDestrListElem = NULL;

	if (!pGlobalDestructors)
	{
		size_t numDestrElements = CONFIG_PROC_NUM_MAX_GLOBAL_DESTRUCTORS + 1;

		pGlobalDestructors = new (std::nothrow) GlobDestructorFunc[numDestrElements];

		if (!pGlobalDestructors)
		{
			errLog(-1, "could not allocate global destructor list");
			return;
		}

		pGlobDestrListElem = pGlobalDestructors;
		for (size_t i = 0; i < numDestrElements; ++i)
			*pGlobDestrListElem++ = NULL;
	}

	pGlobDestrListElem = pGlobalDestructors;
	for (size_t i = 0; i < CONFIG_PROC_NUM_MAX_GLOBAL_DESTRUCTORS; ++i)
	{
		if (!*pGlobDestrListElem)
		{
			*pGlobDestrListElem = globDestr;
			dbgLog(LOG_LVL, ": done");
			return;
		}

		++pGlobDestrListElem;
	}

	errLog(-2, "could not register global destructor. no free slot available");
#endif
#else
	(void)globDestr;
	errLog(-1, "can't register global destructor. function disabled");
#endif
}

#if !CONFIG_PROC_HAVE_LIB_STD_C
const char *Processing::strrchr(const char *x, char y)
{
	if (!x)
		return NULL;

	for (; *x; ++x)
	{
		if (*x == y)
			return x;
	}

	return NULL;
}

void *Processing::memcpy(void *to, const void *from, size_t cnt)
{
	const char *pFrom = (const char *)from;
	char *pTo = (char *)to;

	for (; cnt; --cnt)
		*pTo++ = *pFrom++;

	return to;
}
#endif

// This area is used by the concrete processes

Processing::Processing(const char *name)
	: mState(0)
	, mStateOld(0)
	, mName(name)
#if !CONFIG_PROC_HAVE_LIB_STD_CPP
	, mpChildList(NULL)
#endif
#if CONFIG_PROC_HAVE_DRIVERS
	, mpThread(NULL)
#endif
	, mSuccess(Pending)
	, mNumChildren(0)
#if !CONFIG_PROC_HAVE_LIB_STD_CPP
	, mNumChildrenMax(CONFIG_PROC_NUM_MAX_CHILDREN_DEFAULT)
#endif
	, mStateAbstract(PsExistent)
	, mDriver(DrivenByExternalDriver)
	, mStatParent(0)
	, mLevelTree(0)
	, mLevelDriver(0)
{
	procDbgLog(LOG_LVL, "Processing()");

	mStatDrv = 0;
	if (disableTreeDefault)
		mStatDrv = PsbDrvPrTreeDisable;
}

/*
 * Protected destructor because of two reasons
 * - Processes MUST be allocated on heap to be
 *   able to maintain a process structure
 * - Destruction of processes include stopping them
 *   first. If a process has a new internal driver
 *   it must be joined first. This can't be done
 *   in a destructor because at this point the
 *   processing function of the concrete process
 *   doesn't exist anymore. Therefore an explicit
 *   function repel() must be used by the concrete
 *   processes to delete a child
 */

Processing::~Processing()
{
	procDbgLog(LOG_LVL, "~Processing()");
#if CONFIG_PROC_HAVE_DRIVERS
	procDbgLog(LOG_LVL, "mpThread = 0x%08X", mpThread);
#endif
}

Processing *Processing::start(Processing *pChild, DriverMode driver)
{
	if (!pChild)
	{
		procErrLog(-1, "could not start child. NULL pointer");
		return NULL;
	}

	if (pChild == this)
	{
		procErrLog(-1, "could not start child. pointer to child is me");
		return NULL;
	}
#if !CONFIG_PROC_HAVE_LIB_STD_CPP
	if (mNumChildren >= mNumChildrenMax)
	{
		procErrLog(-2, "can't add child. maximum number of children reached");
		return NULL;
	}
#endif

	char childId[CONFIG_PROC_ID_BUFFER_SIZE];
	procId(childId, childId + sizeof(childId), pChild);

	procDbgLog(LOG_LVL, "starting %s", childId);

	pChild->mDriver = driver;
	pChild->mLevelTree = mLevelTree + 1;
	pChild->mLevelDriver = mLevelDriver;
	pChild->mStatParent |= PsbParStarted;

	procDbgLog(LOG_LVL, "adding %s to child list", childId);
	{
#if CONFIG_PROC_HAVE_DRIVERS
		procDbgLog(LOG_LVL, "Locking mChildListMtx");
		Guard lock(mChildListMtx);
		procDbgLog(LOG_LVL, "Locking mChildListMtx: done");
#endif
#if CONFIG_PROC_HAVE_LIB_STD_CPP
		mChildList.push_back(pChild);
		++mNumChildren;
#else
		childElemAdd(pChild);
#endif
	}
	procDbgLog(LOG_LVL, "adding %s to child list: done", childId);

	if (driver == DrivenByNewInternalDriver)
	{
		procDbgLog(LOG_LVL, "creating new internal driver for %s", childId);
#if CONFIG_PROC_HAVE_DRIVERS
		++pChild->mLevelDriver;

		pChild->mpThread = new (std::nothrow) std::thread(&Processing::internalDrive, pChild);
		if (!pChild->mpThread)
		{
			procErrLog(-3, "could not allocate internal driver for child");
			return NULL;
		}

		procDbgLog(LOG_LVL, "creating new internal driver for %s: done", childId);
#else
		procWrnLog("can't create new internal driver because system has no drivers");
#endif
	}
	else if (driver == DrivenByExternalDriver)
	{
		procDbgLog(0, "external driver is used for %s", childId);
		++pChild->mLevelDriver;
	} else
		procDbgLog(LOG_LVL, "using parent as driver for %s", childId);

	procDbgLog(LOG_LVL, "starting %s: done", childId);

	return pChild;
}

Processing *Processing::cancel(Processing *pChild)
{
	if (!pChild)
	{
		procErrLog(-1, "could not cancel child. NULL pointer");
		return NULL;
	}

	if (pChild == this)
	{
		procErrLog(-1, "could not cancel child. pointer to child is me");
		return NULL;
	}

	if (!(pChild->mStatParent & PsbParStarted))
	{
		procErrLog(-2, "tried to cancel orphan");
		return NULL;
	}

	char childId[CONFIG_PROC_ID_BUFFER_SIZE];
	procId(childId, childId + sizeof(childId), pChild);

	procDbgLog(LOG_LVL, "canceling %s", childId);
	pChild->mStatParent |= PsbParCanceled;
	procDbgLog(LOG_LVL, "canceling %s: done", childId);

	return pChild;
}

Processing *Processing::repel(Processing *pChild)
{
	procDbgLog(LOG_LVL, "trying to repel child");

	if (!cancel(pChild))
		return NULL;

	procDbgLog(LOG_LVL, "setting child unused");
	pChild->unusedSet();
	procDbgLog(LOG_LVL, "setting child unused: done");

	return NULL;
}

Processing *Processing::whenFinishedRepel(Processing *pChild)
{
	if (!pChild)
	{
		procErrLog(-1, "can't repel child when finished. NULL pointer");
		return NULL;
	}

	if (pChild == this)
	{
		procErrLog(-1, "can't repel child when finished. pointer to child is me");
		return NULL;
	}

	char childId[CONFIG_PROC_ID_BUFFER_SIZE];
	procId(childId, childId + sizeof(childId), pChild);

	procDbgLog(LOG_LVL, "repelling %s when finished", childId);
	pChild->mStatParent |= PsbParWhenFinishedUnused;
	procDbgLog(LOG_LVL, "repelling %s when finished: done", childId);

	return NULL;
}

Success Processing::initialize()
{
	procDbgLog(LOG_LVL, "initializing() not used");
	return Positive;
}

Success Processing::shutdown()
{
	procDbgLog(LOG_LVL, "shutdown() not used");
	return Positive;
}

void Processing::processInfo(char *pBuf, char *pBufEnd)
{
	(void)pBuf;
	(void)pBufEnd;
}

size_t Processing::processTrace(char *pBuf, char *pBufEnd)
{
	(void)pBuf;
	(void)pBufEnd;
	return 0;
}

// Return: Sorted by priority
// - Negative .. At least one child is Negative. Error number of first err child
// - Pending  .. At least one child is Pending
// - Positive .. All children finished Positive
Success Processing::childrenSuccess()
{
	if (!mNumChildren)
		return Positive;

	Processing *pChild = NULL;
	Success success;
	bool oneIsPending = false;

#if CONFIG_PROC_HAVE_LIB_STD_CPP
	ChildIter iter = mChildList.begin();
	while (iter != mChildList.end())
	{
		pChild = *iter++;
#else
	Processing **pChildListElem = mpChildList;
	while (pChildListElem and *pChildListElem)
	{
		pChild = *pChildListElem++;
#endif
		if (pChild->mStatParent & PsbParUnused)
			continue;

		success = pChild->success();

		if (success < Pending)
			return success;

		if (success == Pending)
			oneIsPending = true;
	}

	if (oneIsPending)
		return Pending;

	return Positive;
}

size_t Processing::mncpy(void *dest, size_t destSize, const void *src, size_t srcSize)
{
	if (destSize < srcSize)
		return 0;

	memcpy(dest, src, srcSize);

	return srcSize;
}

#if !CONFIG_PROC_HAVE_LIB_STD_CPP
void Processing::maxChildrenSet(uint16_t cnt)
{
	if (mpChildList)
	{
		procErrLog(-1, "can't change max number of children. List created already");
		return;
	}

	mNumChildrenMax = cnt;
}
#endif

DriverMode Processing::driver() const	{ return mDriver; }

size_t Processing::procId(char *pBuf, char *pBufEnd, const Processing *pProc)
{
	char *pBufStart = pBuf;

	if (!pProc)
		return 0;

	if (showAddressInId)
		dInfo("%p ", (const void *)pProc);

	dInfo("%s", pProc->mName);

	return pBuf - pBufStart;
}

size_t Processing::progressStr(char *pBuf, char *pBufEnd, const int val, const int maxVal)
{
	char *pBufStart = pBuf;

	int valCapped = val < maxVal ? val : maxVal;
	valCapped = valCapped > 0 ? valCapped : 0;

	int percent;
	if (maxVal)
		percent = (valCapped * 100) / maxVal;
	else
		percent = 0;

	const int percentageStep = 5;
	const int maxPercentageSteps = 100 / percentageStep;
	int barCnt = percent / percentageStep;

	dInfo("|");
	for (int i = 0; i < barCnt; ++i)
		dInfo("=");
	dInfo("%*s", maxPercentageSteps - barCnt, "");
	dInfo("|");

	dInfo(" %3d%%", percent);

	int maxValLen = 0;
	int maxValCpy = maxVal;

	for (; maxValCpy; ++maxValLen)
		maxValCpy *= (float)0.1;

	dInfo(" %*d / %d", maxValLen, val, maxVal);

	return pBuf - pBufStart;
}

// This area is used by the abstract process

#if !CONFIG_PROC_HAVE_LIB_STD_CPP
Processing **Processing::childElemAdd(Processing *pChild)
{
	if (mNumChildren >= mNumChildrenMax)
	{
		procErrLog(-1, "can't add child. maximum number of children reached");
		return NULL;
	}

	if (!mpChildList)
	{
		size_t numChildElements = mNumChildrenMax + 1;

		mpChildList = new (std::nothrow) Processing *[numChildElements];

		if (!mpChildList)
		{
			procErrLog(-2, "could not allocate child list");
			return NULL;
		}

		Processing **pChildListElem = mpChildList;
		for (size_t i = 0; i < numChildElements; ++i)
			*pChildListElem++ = NULL;
	}

	Processing **pEnd = &mpChildList[mNumChildren];

	*pEnd = pChild;
	++mNumChildren;

	return pEnd;
}

Processing **Processing::childElemErase(Processing **pChildListElem)
{
	if (!mNumChildren)
	{
		procErrLog(-1, "can't remove child. no children");
		return NULL;
	}

	Processing **pMid = pChildListElem;
	Processing **pLast = &mpChildList[mNumChildren - 1];

	while (pChildListElem < pLast)
	{
		*pChildListElem = *(pChildListElem + 1);
		++pChildListElem;
	}

	*pChildListElem = NULL;
	--mNumChildren;

	return pMid;
}
#endif

void Processing::parentalDrive(Processing *pChild)
{
	if (pChild->mDriver != DrivenByParent)
		return;

	if (pChild->mStatDrv & PsbDrvUndriven)
		return;

	pChild->treeTick();

	if (pChild->progress())
		return;

	undrivenSet(pChild);
}

#if CONFIG_PROC_HAVE_DRIVERS
void Processing::internalDrive(Processing *pChild)
{
	while (1)
	{
		pChild->treeTick();
		this_thread::sleep_for(chrono::milliseconds(2));

		if (pChild->progress())
			continue;

		undrivenSet(pChild);
		break;
	}
}
#endif

