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

#ifndef PROCESSING_H
#define PROCESSING_H

#ifndef CONFIG_PROC_HAVE_LIB_C_CUSTOM
#define CONFIG_PROC_HAVE_LIB_C_CUSTOM			0
#endif

#if CONFIG_PROC_HAVE_LIB_C_CUSTOM
#include "LibcCustom.h"
#endif

#ifndef CONFIG_PROC_HAVE_LOG
#define CONFIG_PROC_HAVE_LOG					0
#endif

#ifndef CONFIG_PROC_HAVE_DRIVERS
#if defined(__STDCPP_THREADS__)
#define CONFIG_PROC_HAVE_DRIVERS				1
#else
#define CONFIG_PROC_HAVE_DRIVERS				0
#endif
#endif

#ifndef CONFIG_PROC_HAVE_GLOBAL_DESTRUCTORS
#define CONFIG_PROC_HAVE_GLOBAL_DESTRUCTORS		1
#endif

#ifndef CONFIG_PROC_NUM_MAX_GLOBAL_DESTRUCTORS
#define CONFIG_PROC_NUM_MAX_GLOBAL_DESTRUCTORS	20
#endif

#ifndef CONFIG_PROC_HAVE_LIB_STD_C
#define CONFIG_PROC_HAVE_LIB_STD_C				1
#endif

#ifndef CONFIG_PROC_HAVE_LIB_STD_CPP
#if defined(__unix__) || defined(_WIN32)
#define CONFIG_PROC_HAVE_LIB_STD_CPP			1
#else
#define CONFIG_PROC_HAVE_LIB_STD_CPP			0
#endif
#endif

#ifndef CONFIG_PROC_USE_DRIVER_COLOR
#define CONFIG_PROC_USE_DRIVER_COLOR			1
#endif

#ifndef CONFIG_PROC_NUM_MAX_CHILDREN_DEFAULT
#define CONFIG_PROC_NUM_MAX_CHILDREN_DEFAULT		20
#endif

#ifndef CONFIG_PROC_SHOW_ADDRESS_IN_ID
#define CONFIG_PROC_SHOW_ADDRESS_IN_ID			0
#endif

#ifndef CONFIG_PROC_ID_BUFFER_SIZE
#define CONFIG_PROC_ID_BUFFER_SIZE				64
#endif

#ifndef CONFIG_PROC_INFO_BUFFER_SIZE
#define CONFIG_PROC_INFO_BUFFER_SIZE			256
#endif

#ifndef CONFIG_PROC_DISABLE_TREE_DEFAULT
#define CONFIG_PROC_DISABLE_TREE_DEFAULT		0
#endif

#if CONFIG_PROC_HAVE_LIB_STD_C
#include <cstdint>
#include <cstring>
#include <cstdio>
#endif

#if CONFIG_PROC_HAVE_LIB_STD_CPP
#include <new>
#include <list>
#define dNoThrow (std::nothrow)
#endif

#ifndef dNoThrow
#define dNoThrow
#endif

#if CONFIG_PROC_HAVE_DRIVERS
#include <thread>
#include <mutex>
typedef std::lock_guard<std::mutex> Guard;
#endif

enum DriverMode
{
	DrivenByParent = 0,
	DrivenByNewInternalDriver,
	DrivenByExternalDriver
};

typedef int16_t Success;

enum SuccessState
{
	Pending = 0,
	Positive = 1
};

typedef void (*FuncGlobDestruct)();
typedef void (*FuncInternalDrive)(void *pProc);
typedef void * /* pDriver */ (*FuncDriverInternalCreate)(FuncInternalDrive pFctDrive, void *pProc, void *pConfigDriver);
typedef void (*FuncDriverInternalCleanUp)(void *pDriver);

class Processing
{

public:
	// This area is used by the client

	void treeTick();
	bool progress() const;
	Success success() const;
	void unusedSet();
	void procTreeDisplaySet(bool display);

	bool initDone() const;
	bool processDone() const;
	bool shutdownDone() const;

	size_t processTreeStr(char *pBuf, char *pBufEnd, bool detailed = true, bool colored = false);
#if CONFIG_PROC_HAVE_DRIVERS
	void configDriverSet(void *pConfigDriver);
#endif
	static void undrivenSet(Processing *pChild);
	static void destroy(Processing *pChild);
	static void applicationClose();
	static void globalDestructorRegister(FuncGlobDestruct globDestr);
#if !CONFIG_PROC_HAVE_LIB_STD_C
	static const char *strrchr(const char *x, char y);
	static void *memcpy(void *to, const void *from, size_t cnt);
#endif
	static void showAddressInIdSet(uint8_t val) { showAddressInId = val; }
	static void disableTreeDefaultSet(uint8_t val) { disableTreeDefault = val; }
#if CONFIG_PROC_HAVE_DRIVERS
	static void sleepUsInternalDriveSet(size_t delayUs);
	static void sleepInternalDriveSet(std::chrono::microseconds delay);
	static void sleepInternalDriveSet(std::chrono::milliseconds delay);
	static void numBurstInternalDriveSet(size_t numBurst);
	static void internalDriveSet(FuncInternalDrive pFctDrive);
	static void driverInternalCreateAndCleanUpSet(
			FuncDriverInternalCreate pFctCreate,
			FuncDriverInternalCleanUp pFctCleanUp);
#endif

protected:
	// This area is used by the concrete processes

	Processing(const char *name);
	virtual ~Processing();

	const char *procName() const { return mName; }

	Processing *start(Processing *pChild, DriverMode driver = DrivenByParent);
	Processing *cancel(Processing *pChild);
	Processing *repel(Processing *pChild);
	Processing *whenFinishedRepel(Processing *pChild);

	virtual Success initialize();
	virtual Success process() = 0;
	virtual Success shutdown();

	virtual void processInfo(char *pBuf, char *pBufEnd);
	virtual size_t processTrace(char *pBuf, char *pBufEnd);

	Success childrenSuccess();
	size_t mncpy(void *dest, size_t destSize, const void *src, size_t srcSize);
#if !CONFIG_PROC_HAVE_LIB_STD_CPP
	void maxChildrenSet(uint16_t cnt);
#endif
	DriverMode driver() const;
	uint8_t levelDriver() const;

	static size_t procId(char *pBuf, char *pBufEnd, const Processing *pProc);
	static size_t progressStr(char *pBuf, char *pBufEnd, const int val, const int maxVal);

	uint8_t mState;
	uint8_t mStateOld;

private:
	// This area is used by the abstract process

	Processing() {}
	Processing(const Processing &) {}
	Processing &operator=(const Processing &)
	{
		return *this;
	}

	/* member functions */

	/* member variables */
	uint8_t mLevelTree;
	uint8_t mLevelDriver;

	const char *mName;

#if CONFIG_PROC_HAVE_LIB_STD_CPP
	std::list<Processing *> mChildList;
#else
	Processing **childElemAdd(Processing *pChild);
	Processing **childElemErase(Processing **pChildListElem);
	Processing **mpChildList;
#endif
#if CONFIG_PROC_HAVE_DRIVERS
	std::mutex mChildListMtx;
	void *mpDriver;
	void *mpConfigDriver;
#endif
	Success mSuccess;
	uint16_t mNumChildren;
	uint8_t mStateAbstract;
	uint8_t mStatParent;
	DriverMode mDriver;
#if !CONFIG_PROC_HAVE_LIB_STD_CPP
	uint16_t mNumChildrenMax;
#endif
	uint8_t mStatDrv;

	/* static functions */
	static void parentalDrive(Processing *pChild);
#if CONFIG_PROC_HAVE_DRIVERS
	static void internalDrive(void *pProc);
	static void *driverInternalCreate(FuncInternalDrive pFctDrive, void *pProc, void *pConfigDriver);
	static void driverInternalCleanUp(void *pDriver);

	/* static variables */
	static size_t sleepInternalDriveUs;
	static size_t numBurstInternalDrive;
	static FuncInternalDrive pFctInternalDrive;
	static FuncDriverInternalCreate pFctDriverInternalCreate;
	static FuncDriverInternalCleanUp pFctDriverInternalCleanUp;
#endif
	static uint8_t showAddressInId;
	static uint8_t disableTreeDefault;

#if CONFIG_PROC_HAVE_GLOBAL_DESTRUCTORS
#if CONFIG_PROC_HAVE_LIB_STD_CPP
	static std::list<FuncGlobDestruct> globalDestructors;
#else
	static FuncGlobDestruct *pGlobalDestructors;
#endif
#endif
};

#if CONFIG_PROC_HAVE_LIB_STD_C
#define procStrrChr(s, c)		strrchr(s, c)
#else
#define procStrrChr(s, c)		Processing::strrchr(s, c)
#endif
#define __PROC_FILENAME__ (procStrrChr(__FILE__, '/') ? procStrrChr(__FILE__, '/') + 1 : __FILE__)

#if CONFIG_PROC_HAVE_LOG
typedef void (*FuncEntryLogCreate)(
			const int severity,
			const char *filename,
			const char *function,
			const int line,
			const int16_t code,
			const char *msg,
			const size_t len);

void levelLogSet(int lvl);
void entryLogCreateSet(FuncEntryLogCreate pFct);
int16_t logEntryCreate(
				const int severity,
				const char *filename,
				const char *function,
				const int line,
				const int16_t code,
				const char *msg, ...);
#define genericLog(l, c, m, ...)			(logEntryCreate(l, __PROC_FILENAME__, __func__, __LINE__, c, m, ##__VA_ARGS__))
#else
inline void levelLogSet(int lvl)
{
	(void)lvl;
}
#define entryLogCreateSet(pFct)
inline int16_t logEntryCreateDummy(
				const int severity,
				const char *filename,
				const char *function,
				const int line,
				const int16_t code,
				const char *msg, ...)
{
	(void)severity;
	(void)filename;
	(void)function;
	(void)line;
	(void)msg;
	return code;
}
#define genericLog(l, c, m, ...)	(logEntryCreateDummy(l, __PROC_FILENAME__, __func__, __LINE__, c, m, ##__VA_ARGS__))
#endif

#define errLog(c, m, ...)				(c < 0 ? genericLog(1, c, "%-41s " m, __PROC_FILENAME__, ##__VA_ARGS__) : c)
#define wrnLog(m, ...)					(genericLog(2, 0, "%-41s " m, __PROC_FILENAME__, ##__VA_ARGS__))
#define infLog(m, ...)					(genericLog(3, 0, "%-41s " m, __PROC_FILENAME__, ##__VA_ARGS__))
#define dbgLog(m, ...)					(genericLog(4, 0, "%-41s " m, __PROC_FILENAME__, ##__VA_ARGS__))

#define procErrLog(c, m, ...)				(c < 0 ? genericLog(1, c, "%p %-26s " m, this, this->procName(), ##__VA_ARGS__) : c)
#define procWrnLog(m, ...)				(genericLog(2, 0, "%p %-26s " m, this, this->procName(), ##__VA_ARGS__))
#define procInfLog(m, ...)				(genericLog(3, 0, "%p %-26s " m, this, this->procName(), ##__VA_ARGS__))
#define procDbgLog(m, ...)				(genericLog(4, 0, "%p %-26s " m, this, this->procName(), ##__VA_ARGS__))
#define procCoreLog(m, ...)				(genericLog(5, 0, "%p %-26s " m, this, this->procName(), ##__VA_ARGS__))

#if CONFIG_PROC_HAVE_LIB_STD_C
#define dInfoDebugPrefix
#define dInfo(m, ...)					dInfoDebugPrefix pBuf = (pBuf += snprintf(pBuf, pBufEnd - pBuf, m, ##__VA_ARGS__), pBuf > pBufEnd ? pBufEnd : pBuf)
#else
inline void dInfoDummy(char *pBuf, char *pBufEnd, const char *msg, ...)
{
	if (!pBuf)
		return;

	*pBuf = 0;

	(void)pBufEnd;
	(void)msg;
}
#define dInfo(m, ...)					dInfoDummy(pBuf, pBufEnd, m, ##__VA_ARGS__)
#endif
#define dTrace(x)						pBuf += mncpy(pBuf, pBufEnd - pBuf, (char *)&x, sizeof(x))

#define dProcessStateEnum(StateName) \
enum StateName \
{ \
	dForEach_ ## StateName(dGen ## StateName ## Enum) \
}

#define dProcessStateStr(StateName) \
static const char *StateName ## String[] = \
{ \
	dForEach_ ## StateName(dGen ## StateName ## String) \
}

// pst .. process state transition
#define dStateTrace \
if (mState != mStateOld) \
{ \
	procDbgLog("pst: %s > %s", \
			ProcStateString[mStateOld], \
			ProcStateString[mState]); \
	mStateOld = mState; \
}

template <typename T>
T MIN(T a, T b)
{
	return a < b ? a : b;
}

template <typename T>
T MAX(T a, T b)
{
	return a > b ? a : b;
}

#ifdef _MSC_VER
#ifndef SSIZE_TYPE_DEFINE
#define SSIZE_TYPE_DEFINE
#define ssize_t SSIZE_T
#endif
#endif

#endif

