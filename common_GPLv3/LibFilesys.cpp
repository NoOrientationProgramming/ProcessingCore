/*
  This file is part of the DSP-Crowd project
  https://www.dsp-crowd.com

  Author(s):
      - Johannes Natter, office@dsp-crowd.com

  File created on 17.01.2023

  Copyright (C) 2023-now Authors and www.dsp-crowd.com

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

#include <map>
#include <cstdio>
#include <mutex>
#include <stdarg.h>
#include <unistd.h>

#include "LibFilesys.h"

using namespace std;

struct GlobalLock
{
	void *pOwner;
	const char *filename;
	const char *function;
	int line;
	string nameRes;
};

static string lockDefaultDirBase;
static int fdLockDefault = 0;
static mutex globalLocksMtx;
static map<string, GlobalLock> globalLocks;

void pipeInit(PairFd &pair)
{
	pair.fdRead = -1;
	pair.fdWrite = -1;
}

// - https://man7.org/linux/man-pages/man2/open.2.html
// - https://man7.org/linux/man-pages/man3/fopen.3.html
int fdCreate(const std::string &path, const std::string &mode)
{
	if (!path.size() or !mode.size())
		return -1;

	int flags = 0;
	bool ok, createRequested = true;

	if (mode == "r")
	{
		flags = O_RDONLY;
		createRequested = false;
	}
	else
	if (mode == "r+")
	{
		flags = O_RDWR;
		createRequested = false;
	}
	else
	if (mode == "w")
		flags = O_WRONLY | O_TRUNC;
	else
	if (mode == "w+")
		flags = O_RDWR | O_TRUNC;
	else
	if (mode == "a")
		flags = O_WRONLY | O_APPEND;
	else
	if (mode == "a+")
		flags = O_RDWR | O_APPEND;

	if (createRequested)
	{
		ok = fileExists(path);
		if (!ok)
		{
			ok = fileCreate(path);
			if (!ok)
				return -1;
		}
	}

	return open(path.c_str(), flags);
}

void pipeClose(PairFd &pair, bool deInit)
{
	fdClose(pair.fdRead, deInit);
	fdClose(pair.fdWrite, deInit);
}

void fdClose(int &fd, bool deInit)
{
	if (fd < 0)
		return;

	close(fd);

	if (!deInit)
		return;

	fd = -1;
}

bool fileExists(const string &path)
{
	FILE *pFile = fopen(path.c_str(), "r");

	if (!pFile)
		return false;

	fclose(pFile);

	return true;
}

bool fileCreate(const string &path)
{
	FILE *pFile = fopen(path.c_str(), "w");

	if (!pFile)
		return false;

	fclose(pFile);

	return true;
}

bool fileNonBlockingSet(int fd)
{
	int opt = 1;
#ifdef _WIN32
	unsigned long nonBlockMode = 1;

	opt = ioctlsocket(fd, FIONBIO, &nonBlockMode);
	if (opt == SOCKET_ERROR)
		return false;
#else
	opt = fcntl(fd, F_GETFL, 0);
	if (opt == -1)
		return false;

	opt |= O_NONBLOCK;

	opt = fcntl(fd, F_SETFL, opt);
	if (opt == -1)
		return false;
#endif
	return true;
}

bool dirExists(const string &path)
{
	struct stat sb;
	int res;

	res = stat(path.c_str(), &sb);
	if (res)
		return false;

	if ((sb.st_mode & S_IFMT) != S_IFDIR)
		return false;

	return true;
}

bool dirCreate(const string &path, mode_t mode)
{
	size_t pos = 0;
	string node;
	bool ok, endReached = false;
	int res;

	for (; !endReached; ++pos)
	{
		pos = path.find('/', pos);
		node = path.substr(0, pos);

		if (pos == string::npos)
			endReached = true;

		if (!node.size())
			return false;

		ok = dirExists(node);
		if (ok)
			continue;

		res = mkdir(node.c_str(), mode);
		if (res)
			return false;
	}

	return true;
}

bool strToFile(const string &str, const string &path)
{
	FILE *pFile = fopen(path.c_str(), "w");

	if (!pFile)
		return false;

	size_t lenReq, lenWritten;

	lenReq = str.size();
	lenWritten = fwrite(str.c_str(), sizeof(char), lenReq, pFile);

	fclose(pFile);

	if (lenWritten != lenReq)
		return false;

	return true;
}

/*
Literature
- https://linux.die.net/man/2/open
- https://linux.die.net/man/2/flock
*/
bool lockDirDefaultOpen(const string &dirBase)
{
	if (fdLockDefault)
		return false;

	if (!dirBase.size())
		return false;

	string dirBaseLocal = dirBase;

	if (dirBaseLocal.back() != '/')
		dirBaseLocal.push_back('/');

	string fileLock = dirBaseLocal + ".lock";

	fdLockDefault = open(fileLock.c_str(), O_RDONLY | O_CREAT,
					S_IRUSR | S_IWUSR,
					S_IRGRP | S_IWGRP,
					S_IROTH | S_IWOTH);

	if (fdLockDefault < 0)
		return false;

	lockDefaultDirBase = dirBaseLocal;

	return true;
}

void lockDirDefaultClose()
{
	if (fdLockDefault < 0)
		return;

	close(fdLockDefault);
}

Success sysFlagsIntLock(void *pRequester, const char *filename, const char *function, const int line, UserLocks &locks, ...)
{
	if (fdLockDefault < 0)
		return -1;

	int res;

	// Step 1: Try to get the file lock
	{
		lock_guard<mutex> lock(globalLocksMtx);
		res = flock(fdLockDefault, LOCK_EX | LOCK_NB);
	}

	if (res and errno == EWOULDBLOCK)
		return Pending;

	if (res)
		return -1;

	// Start of remote critical section
	// Guarded by function flock()

	va_list args;
	const char *pArg;
	string pathFlag;

	// Step 2: Check if all resources/flags are available
	va_start(args, locks);
	while (pArg = va_arg(args, const char *), pArg)
	{
		pathFlag = lockDefaultDirBase + pArg;
		//wrnLog("Checking %s", pathFlag.c_str());

		if (fileExists(pathFlag))
		{
			lock_guard<mutex> lock(globalLocksMtx);
			flock(fdLockDefault, LOCK_UN | LOCK_NB);
			return Pending;
		}
	}
	va_end(args);

	// Step 3: Take/create all flags
	va_start(args, locks);
	while (pArg = va_arg(args, const char *), pArg)
		fileCreate(lockDefaultDirBase + pArg);
	va_end(args);

	// Step 4: Exit the remote critical section
	{
		lock_guard<mutex> lock(globalLocksMtx);

		// End of remote critical section
		flock(fdLockDefault, LOCK_UN | LOCK_NB);

		// Step 5: Change user and global locks list
		UserLock uLock;
		GlobalLock gLock;

		uLock.type = 0;

		gLock.pOwner = pRequester;
		gLock.filename = filename;
		gLock.function = function;
		gLock.line = line;

		va_start(args, locks);
		while (pArg = va_arg(args, const char *), pArg)
		{
			uLock.nameRes = pArg;
			locks.push_back(uLock);

			gLock.nameRes = pArg;
			globalLocks[pArg] = gLock;
		}
		va_end(args);
	}

	return Positive;
}

void sysFlagsIntUnlock(void *pRequester, const char *filename, const char *function, const int line, UserLocks &locks)
{
	if (fdLockDefault < 0)
		return;

	if (!locks.size())
		return;
#if 0
	wrnLog("Process %p is unlocking", pRequester);
	wrnLog("In file %s in function %s() at line %d", filename, function, line);
#else
	(void)pRequester;
	(void)filename;
	(void)function;
	(void)line;
#endif
	UserLocks::iterator iuLock;

	{
		lock_guard<mutex> lock(globalLocksMtx);

		iuLock = locks.begin();
		for (; iuLock != locks.end(); ++iuLock)
			globalLocks.erase(iuLock->nameRes);
	}

	string pathFlag;

	iuLock = locks.begin();
	for (; iuLock != locks.end(); ++iuLock)
	{
#if 0
		wrnLog("Unlocking %s", iuLock->nameRes.c_str());
#endif
		pathFlag = lockDefaultDirBase + iuLock->nameRes;
		remove(pathFlag.c_str());
		iuLock = locks.erase(iuLock);
	}
}

