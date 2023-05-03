/*
  This file is part of the DSP-Crowd project
  https://www.dsp-crowd.com

  Author(s):
      - Johannes Natter, office@dsp-crowd.com

  File created on 03.01.2023

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

#ifndef LIB_DSPC_H
#define LIB_DSPC_H

#include <string>
#include <mutex>
#include <jsoncpp/json/json.h>
#include <cryptopp/secblock.h>
#include <cryptopp/sha.h>
#include <curl/curl.h>

#include "Processing.h"
#include "Res.h"
#include "LibTime.h"
#include "LibFilesys.h"

typedef std::vector<std::string> VecStr;
typedef VecStr::iterator VecStrIter;
typedef VecStr::const_iterator VecStrConstIter;

typedef std::list<std::string> ListStr;
typedef ListStr::iterator ListStrIter;
typedef ListStr::const_iterator ListStrConstIter;

void curlGlobalInit();
void curlGlobalDeInit();

// Debugging
std::string appVersion();
void hexDump(const void *pData, size_t len, size_t colWidth = 0x10);
std::string toHexStr(const std::string &strIn);
size_t strReplace(std::string &strIn, const std::string &strFind, const std::string &strReplacement);
void jsonPrint(const Json::Value &val);

// Cryptography
std::string sha256(const std::string &msg, const std::string &prefix = "");
std::string sha256(const CryptoPP::SecByteBlock &msg, const std::string &prefix = "");
bool isValidSha256(const std::string &digest);

// Internet
bool isValidEmail(const std::string &mail);
bool isValidIp4(const std::string &ip);
std::string remoteAddr(int socketFd);

// Strings
void strToVecStr(const std::string &str, VecStr &vStr, char delim = '\n');

#endif

