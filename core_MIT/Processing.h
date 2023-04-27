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

#include <new>

#ifndef PROCESSING_H
#define PROCESSING_H

#ifndef CONFIG_PROC_HAVE_LOG
#define CONFIG_PROC_HAVE_LOG					0
#endif

#ifndef CONFIG_PROC_HAVE_DRIVERS
#define CONFIG_PROC_HAVE_DRIVERS				0
#endif

#ifndef CONFIG_PROC_HAVE_GLOBAL_DESTRUCTORS
#define CONFIG_PROC_HAVE_GLOBAL_DESTRUCTORS		1
#endif

#ifndef CONFIG_PROC_NUM_MAX_GLOBAL_DESTRUCTORS
#define CONFIG_PROC_NUM_MAX_GLOBAL_DESTRUCTORS	20
#endif

#ifndef CONFIG_PROC_USE_STD_LISTS
#define CONFIG_PROC_USE_STD_LISTS				0
#endif

#ifndef CONFIG_PROC_USE_DRIVER_COLOR
#define CONFIG_PROC_USE_DRIVER_COLOR			1
#endif

#ifndef CONFIG_PROC_NUM_MAX_CHILDREN_DEFAULT
#define CONFIG_PROC_NUM_MAX_CHILDREN_DEFAULT		20
#endif

#ifndef CONFIG_PROC_SHOW_ADDRESS_IN_ID
#define CONFIG_PROC_SHOW_ADDRESS_IN_ID			1
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

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#if CONFIG_PROC_USE_STD_LISTS
#include <list>
#include <algorithm>
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

#if CONFIG_PROC_HAVE_GLOBAL_DESTRUCTORS
typedef void (*GlobDestructorFunc)();
#endif

class Processing
{

public:
	// This area is used by the client

	void treeTick();
	bool progress() const;
	Success success() const;
	void unusedSet();
	void procTreeDisplaySet(bool display);

	int processTreeStr(char *pBuf, char *pBufEnd, bool detailed = true, bool colored = false);

	static void undrivenSet(Processing *pChild);
	static void destroy(Processing *pChild);
	static void applicationClose();
#if CONFIG_PROC_HAVE_GLOBAL_DESTRUCTORS
	static void globalDestructorRegister(GlobDestructorFunc globDestr);
#endif

protected:
	// This area is used by the concrete processes

	Processing(const char *name);
	virtual ~Processing();

	Processing *start(Processing *pChild, DriverMode driver = DrivenByParent);
	Processing *repel(Processing *pChild);
	Processing *whenFinishedRepel(Processing *pChild);

	virtual Success initialize();
	virtual Success process() = 0;
	virtual Success shutdown();

	virtual void processInfo(char *pBuf, char *pBufEnd);
	virtual size_t processTrace(char *pBuf, char *pBufEnd);

	Success childrenSuccess();
	size_t mncpy(void *dest, size_t destSize, const void *src, size_t srcSize);
#if !CONFIG_PROC_USE_STD_LISTS
	void maxChildrenSet(size_t cnt);
#endif
	bool initDone() const;
	DriverMode driver() const;

	static int procId(char *pBuf, char *pBufEnd, const Processing *pProc);
	static int progressStr(char *pBuf, char *pBufEnd, const int val, const int maxVal);

	const char *mName;

private:
	// This area is used by the abstract process

	Processing() {}
	Processing(const Processing &) {}
	Processing &operator=(const Processing &)
	{
		return *this;
	}

#if CONFIG_PROC_USE_STD_LISTS
	std::list<Processing *> mChildList;
#else
	Processing **childElemAdd(Processing *pChild);
	Processing **childElemErase(Processing **pChildListElem);
	Processing **mpChildList;
#endif

#if CONFIG_PROC_HAVE_DRIVERS
	std::mutex mChildListMtx;
	std::thread *mpThread;
#endif

	Success mSuccess;
	uint16_t mNumChildren;
#if !CONFIG_PROC_USE_STD_LISTS
	uint16_t mNumChildrenMax;
#endif
	uint8_t mProcState;
	DriverMode mDriver;
	uint8_t mStatParent;
	uint8_t mStatDrv;
	uint8_t mLevel;
	uint8_t mDriverLevel;

	static void parentalDrive(Processing *pChild);
#if CONFIG_PROC_HAVE_DRIVERS
	static void internalDrive(Processing *pChild);
#endif

#if CONFIG_PROC_HAVE_GLOBAL_DESTRUCTORS
#if CONFIG_PROC_USE_STD_LISTS
	static std::list<GlobDestructorFunc> globalDestructors;
#else
	static GlobDestructorFunc *pGlobalDestructors;
#endif
#endif
};

#define blockUntilNotified(x) \
{ \
	if (!haveNotificationsOrBlock(x)) \
		return Pending; \
}

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#if CONFIG_PROC_HAVE_LOG
int16_t logEntryCreate(const int severity, const char *filename, const char *function, const int line, const int16_t code, const char *msg, ...);
#else
inline int16_t logEntryCreate(const int severity, const char *filename, const char *function, const int line, const int16_t code, const char *msg, ...)
{
	(void)severity;
	(void)filename;
	(void)function;
	(void)line;
	(void)msg;

	return code;
}
#endif

#define genericLog(l, c, m, ...)			(logEntryCreate(l, __FILENAME__, __FUNCTION__, __LINE__, c, m, ##__VA_ARGS__))
#define errLog(c, m, ...)				(c < 0 ? genericLog(1, c, m, ##__VA_ARGS__) : c)
#define wrnLog(m, ...)					(genericLog(2, 0, m, ##__VA_ARGS__))
#define infLog(m, ...)					(genericLog(3, 0, m, ##__VA_ARGS__))
#define dbgLog(l, m, ...)				(genericLog(4 + l, 0, m, ##__VA_ARGS__))

#define GLOBAL_PROC_LOG_LEVEL_OFFSET		0
#define procErrLog(c, m, ...)				(errLog(c, "%p %-35s" m, this, this->mName, ##__VA_ARGS__))
#define procWrnLog(m, ...)				(wrnLog("%p %-35s" m, this, this->mName, ##__VA_ARGS__))
#define procInfLog(m, ...)				(infLog("%p %-35s" m, this, this->mName, ##__VA_ARGS__))
#define procDbgLog(l, m, ...)				(dbgLog(GLOBAL_PROC_LOG_LEVEL_OFFSET + l, "%p %-35s" m, this, this->mName, ##__VA_ARGS__))

//#define dInfoDebugPrefix				printf("%-20s (%3d): %p %p '%c'%s\n", __FILENAME__, __LINE__, pBuf, pBufEnd, *(pBuf - 1), pBuf > pBufEnd ? " -> FAIL" : ""),
#define dInfoDebugPrefix
#define dInfo(m, ...)					dInfoDebugPrefix pBuf = (pBuf += snprintf(pBuf, pBufEnd - pBuf, m, ##__VA_ARGS__), pBuf > pBufEnd ? pBufEnd : pBuf)
#define dTrace(x)						pBuf += mncpy(pBuf, pBufEnd - pBuf, (char *)&x, sizeof(x))

#define dProcessStateEnum(StateName) \
enum StateName \
{ \
	dForEach_ ## StateName(dGen ## StateName ## Enum) \
};

#define dProcessStateStr(StateName) \
static const char *StateName ## String[] = \
{ \
	dForEach_ ## StateName(dGen ## StateName ## String) \
};

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

#endif

