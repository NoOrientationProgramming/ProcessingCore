/*
  This file is part of the DSP-Crowd project
  https://www.dsp-crowd.com

  Author(s):
      - Johannes Natter, office@dsp-crowd.com

  File created on 14.09.2018

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

#include "Processing.h"

#define coreLog(m, ...)					(genericLog(5, 0, "%-41s " m, __PROC_FILENAME__, ##__VA_ARGS__))
#define procCoreLog(m, ...)				(genericLog(5, 0, "%p %-26s " m, this, this->procName(), ##__VA_ARGS__))

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

#if CONFIG_PROC_HAVE_LIB_STD_CPP
typedef list<Processing *>::iterator ChildIter;
#endif

uint8_t Processing::showAddressInId = CONFIG_PROC_SHOW_ADDRESS_IN_ID;
uint8_t Processing::disableTreeDefault = CONFIG_PROC_DISABLE_TREE_DEFAULT;

#if CONFIG_PROC_HAVE_GLOBAL_DESTRUCTORS
#if CONFIG_PROC_HAVE_LIB_STD_CPP
list<FuncGlobDestruct> Processing::globalDestructors;
#else
FuncGlobDestruct *Processing::pGlobalDestructors = NULL;
#endif
#endif

#if CONFIG_PROC_HAVE_DRIVERS
size_t Processing::sleepInternalDriveUs = 2000;
size_t Processing::numBurstInternalDrive = 13;
FuncInternalDrive Processing::pFctInternalDrive = Processing::internalDrive;
FuncDriverInternalCreate Processing::pFctDriverInternalCreate = Processing::driverInternalCreate;
FuncDriverInternalCleanUp Processing::pFctDriverInternalCleanUp = Processing::driverInternalCleanUp;
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
	while (pChildListElem && *pChildListElem)
	{
		pChild = *pChildListElem;
#endif
		parentalDrive(pChild);

		childCanBeRemoved = pChild->mStatDrv & PsbDrvUndriven &&
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

		procCoreLog("removing %s from child list", childId);
		{
#if CONFIG_PROC_HAVE_DRIVERS
			procCoreLog("Locking mChildListMtx");
			Guard lock(mChildListMtx);
			procCoreLog("Locking mChildListMtx: done");
#endif
#if CONFIG_PROC_HAVE_LIB_STD_CPP
			iter = mChildList.erase(iter);
			--mNumChildren;
#else
			pChildListElem = childElemErase(pChildListElem);
#endif
		}
		procCoreLog("removing %s from child list: done", childId);

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
			procCoreLog("process canceled during state existent");
			mStateAbstract = PsFinishedPrepare;
			break;
		}

		procCoreLog("initializing()");
		mStateAbstract = PsInitializing;

		break;
	case PsInitializing:

		if (mStatParent & PsbParCanceled)
		{
			procCoreLog("process canceled during initializing");
			procCoreLog("downShutting()");
			mStateAbstract = PsDownShutting;
			break;
		}

		success = initialize(); // child list may be changed here

		if (success == Pending)
			break;

		if (success != Positive)
		{
			mSuccess = success;
			procCoreLog("initializing(): failed. success = %d", int(mSuccess));
			procCoreLog("downShutting()");
			mStateAbstract = PsDownShutting;
			break;
		}

		procCoreLog("initializing(): done");
		mStatDrv |= PsbDrvInitDone;

		procCoreLog("processing()");
		mStateAbstract = PsProcessing;

		break;
	case PsProcessing:

		if (mStatParent & PsbParCanceled)
		{
			procCoreLog("process canceled during processing");
			procCoreLog("downShutting()");
			mStateAbstract = PsDownShutting;
			break;
		}

		success = process(); // child list may be changed here

		if (success == Pending)
			break;

		mSuccess = success;

		procCoreLog("processing(): done. success = %d", int(mSuccess));
		mStatDrv |= PsbDrvProcessDone;

		procCoreLog("downShutting()");
		mStateAbstract = PsDownShutting;

		break;
	case PsDownShutting:

		success = shutdown(); // child list may be changed here

		if (success == Pending)
			break;

		procCoreLog("downShutting(): done");
		mStatDrv |= PsbDrvShutdownDone;

		mStateAbstract = PsChildrenUnusedSet;

		break;
	case PsChildrenUnusedSet:

		procCoreLog("marking children as unused");
#if CONFIG_PROC_HAVE_LIB_STD_CPP
		iter = mChildList.begin();
		while (iter != mChildList.end())
		{
			pChild = *iter++;
#else
		pChildListElem = mpChildList;
		while (pChildListElem && *pChildListElem)
		{
			pChild = *pChildListElem++;
#endif
			pChild->unusedSet();
		}
		procCoreLog("marking children as unused: done");

		mStateAbstract = PsFinishedPrepare;

		break;
	case PsFinishedPrepare:

		procCoreLog("preparing finish");

		if (mStatParent & PsbParWhenFinishedUnused)
		{
			procCoreLog("set process as unused when finished");
			unusedSet();
		}

		procCoreLog("preparing finish: done -> finished");

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
	return mStateAbstract != PsFinished || mNumChildren;
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

	if (!pBuf || !(pBufEnd - pBuf))
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
	if (colored && !mLevelDriver)
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

	if (detailed && mStateAbstract != PsFinished)
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

			if (*pBufIter && *pBufIter != '\n')
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
		while (pChildListElem && *pChildListElem)
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

	coreLog("child %s destroy()", childId);

	if (pChild->mNumChildren)
		errLog(-1, "destroying child with grand children");

#if !CONFIG_PROC_HAVE_LIB_STD_CPP
	if (pChild->mpChildList)
	{
		coreLog("child %s deleting child list", childId);
		delete[] pChild->mpChildList;
		pChild->mpChildList = NULL;
		coreLog("child %s deleting child list: done", childId);
	}
#endif
#if CONFIG_PROC_HAVE_DRIVERS
	if (pChild->mpDriver)
	{
		coreLog("driver cleanup");
		pFctDriverInternalCleanUp(pChild->mpDriver);
		pChild->mpDriver = NULL;
		coreLog("driver cleanup: done");
	}
#endif
	coreLog("child %s delete()", childId);
	delete pChild;
	coreLog("child %s delete(): done", childId);

	coreLog("child %s destroy(): done", childId);
}

void Processing::applicationClose()
{
	coreLog("closing application");

#if CONFIG_PROC_HAVE_GLOBAL_DESTRUCTORS
	coreLog("executing global destructors");
#if CONFIG_PROC_HAVE_LIB_STD_CPP
	list<FuncGlobDestruct>::iterator iter = globalDestructors.begin();

	while (iter != globalDestructors.end())
		(*iter++)();

	globalDestructors.clear();
#else
	FuncGlobDestruct *pGlobDestrListElem = pGlobalDestructors;

	while (pGlobDestrListElem && *pGlobDestrListElem)
		(*pGlobDestrListElem++)();

	if (pGlobalDestructors)
		delete[] pGlobalDestructors;
#endif
	coreLog("executing global destructors: done");
#else
	coreLog("global destructors disabled");
#endif

	coreLog("closing application: done");
}

void Processing::globalDestructorRegister(FuncGlobDestruct globDestr)
{
#if CONFIG_PROC_HAVE_GLOBAL_DESTRUCTORS
	coreLog("");
#if CONFIG_PROC_HAVE_LIB_STD_CPP
	globalDestructors.push_front(globDestr);
	globalDestructors.unique();
	coreLog(": done");
#else
	FuncGlobDestruct *pGlobDestrListElem = NULL;

	if (!pGlobalDestructors)
	{
		size_t numDestrElements = CONFIG_PROC_NUM_MAX_GLOBAL_DESTRUCTORS + 1;

		pGlobalDestructors = new dNoThrow FuncGlobDestruct[numDestrElements];
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
			coreLog(": done");
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

#if CONFIG_PROC_HAVE_DRIVERS
void Processing::sleepUsInternalDriveSet(size_t delayUs)
{
#if defined(__linux__) || defined(__FreeBSD__) || defined(_WIN32)
	if (!delayUs)
		wrnLog("Sleep time for internal drivers set to zero => Busy loop");
#endif
	sleepInternalDriveUs = delayUs;
}

void Processing::sleepInternalDriveSet(chrono::microseconds delay)
{
	sleepUsInternalDriveSet(delay.count());
}

void Processing::sleepInternalDriveSet(chrono::milliseconds delay)
{
	sleepUsInternalDriveSet(delay.count() * 1000);
}

void Processing::numBurstInternalDriveSet(size_t numBurst)
{
	if (!numBurst)
		return;

	numBurstInternalDrive = numBurst;
}

void Processing::internalDriveSet(FuncInternalDrive pFctDrive)
{
	if (!pFctDrive)
		return;

	pFctInternalDrive = pFctDrive;
}

void Processing::driverInternalCreateAndCleanUpSet(
			FuncDriverInternalCreate pFctCreate,
			FuncDriverInternalCleanUp pFctCleanUp)
{
	if (!pFctCreate || !pFctCleanUp)
		return;

	pFctDriverInternalCreate = pFctCreate;
	pFctDriverInternalCleanUp = pFctCleanUp;
}
#endif

// This area is used by the concrete processes

Processing::Processing(const char *name)
	: mState(0)
	, mStateOld(0)
	, mLevelTree(0)
	, mLevelDriver(0)
	, mName(name)
#if CONFIG_PROC_HAVE_LIB_STD_CPP
	, mChildList()
#else
	, mpChildList(NULL)
#endif
#if CONFIG_PROC_HAVE_DRIVERS
	, mChildListMtx()
	, mpDriver(NULL)
	, mpConfigDriver(NULL)
#endif
	, mSuccess(Pending)
	, mNumChildren(0)
	, mStateAbstract(PsExistent)
	, mStatParent(0)
	, mDriver(DrivenByExternalDriver)
#if !CONFIG_PROC_HAVE_LIB_STD_CPP
	, mNumChildrenMax(CONFIG_PROC_NUM_MAX_CHILDREN_DEFAULT)
#endif
	//, mStatDrv(0) <- Initialized below
{
	procCoreLog("Processing()");

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
	procCoreLog("~Processing()");
#if CONFIG_PROC_HAVE_DRIVERS
	procCoreLog("mpDriver = 0x%08X", mpDriver);
#endif
}

Processing *Processing::start(Processing *pChild, DriverMode driver)
{
	if (!pChild)
	{
		procCoreLog("could not start child. NULL pointer");
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

	procCoreLog("starting %s", childId);

	pChild->mDriver = driver;
	pChild->mLevelTree = mLevelTree + 1;
	pChild->mLevelDriver = mLevelDriver;
	pChild->mStatParent |= PsbParStarted;

	// Add process to child list
	procCoreLog("adding %s to child list", childId);
	{
#if CONFIG_PROC_HAVE_DRIVERS
		procCoreLog("Locking mChildListMtx");
		Guard lock(mChildListMtx);
		procCoreLog("Locking mChildListMtx: done");
#endif
#if CONFIG_PROC_HAVE_LIB_STD_CPP
		mChildList.push_back(pChild);
		++mNumChildren;
#else
		childElemAdd(pChild);
#endif
	}
	procCoreLog("adding %s to child list: done", childId);

	// Optionally: Create and start new driver
	if (driver == DrivenByNewInternalDriver)
	{
#if CONFIG_PROC_HAVE_DRIVERS
		procCoreLog("using new internal driver for %s", childId);
		++pChild->mLevelDriver;

		procCoreLog("creating new internal driver");
		pChild->mpDriver = pFctDriverInternalCreate(pFctInternalDrive, pChild, pChild->mpConfigDriver);
		pChild->mpConfigDriver = NULL;

		if (!pChild->mpDriver)
		{
			procWrnLog("could not create internal driver. switching back to parental drive");

			pChild->mDriver = DrivenByParent;
			--pChild->mLevelDriver;
		} else
			procCoreLog("creating new internal driver: done");
#else
		procWrnLog("system does not have internal drivers. switching back to parental drive");
		pChild->mDriver = DrivenByParent;
#endif
	}
	else if (driver == DrivenByExternalDriver)
	{
		procCoreLog("using external driver for %s", childId);
		++pChild->mLevelDriver;
	} else
		procCoreLog("using parent as driver for %s", childId);

	procCoreLog("starting %s: done", childId);

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

	procCoreLog("canceling %s", childId);
	pChild->mStatParent |= PsbParCanceled;
	procCoreLog("canceling %s: done", childId);

	return pChild;
}

Processing *Processing::repel(Processing *pChild)
{
	procCoreLog("trying to repel child");

	if (!cancel(pChild))
		return NULL;

	procCoreLog("setting child unused");
	pChild->unusedSet();
	procCoreLog("setting child unused: done");

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

	procCoreLog("repelling %s when finished", childId);
	pChild->mStatParent |= PsbParWhenFinishedUnused;
	procCoreLog("repelling %s when finished: done", childId);

	return NULL;
}

Success Processing::initialize()
{
	procCoreLog("initializing() not used");
	return Positive;
}

Success Processing::shutdown()
{
	procCoreLog("shutdown() not used");
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
	while (pChildListElem && *pChildListElem)
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

DriverMode Processing::driver()	const { return mDriver;		}
uint8_t Processing::levelDriver()	const { return mLevelDriver;	}

#if CONFIG_PROC_HAVE_DRIVERS
void Processing::configDriverSet(void *pConfigDriver)
{
	mpConfigDriver = pConfigDriver;
}
#endif

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
		maxValCpy = (int)(((float)maxValCpy) * 0.1f);

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

		mpChildList = new dNoThrow Processing *[numChildElements];
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
void Processing::internalDrive(void *pProc)
{
	Processing *pChild = (Processing *)pProc;
	size_t i;

	while (1)
	{
		for (i = 0; i < numBurstInternalDrive; ++i)
			pChild->treeTick();

		if (sleepInternalDriveUs)
			this_thread::sleep_for(chrono::microseconds(sleepInternalDriveUs));

		if (pChild->progress())
			continue;

		undrivenSet(pChild);
		break;
	}
}

void *Processing::driverInternalCreate(FuncInternalDrive pFctDrive, void *pProc, void *pConfigDriver)
{
	(void)pConfigDriver;
	return new dNoThrow thread(pFctDrive, pProc);
}

void Processing::driverInternalCleanUp(void *pDriver)
{
	thread *pThread = (thread *)pDriver;

	coreLog("thread join()");
	if (pThread->joinable())
		pThread->join();
	coreLog("thread join(): done");

	coreLog("thread delete()");
	delete pThread;
	pThread = NULL;
	coreLog("thread delete(): done");
}
#endif

