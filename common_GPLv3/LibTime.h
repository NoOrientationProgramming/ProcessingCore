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

#ifndef LIB_TIME_H
#define LIB_TIME_H

#include <string>
#include <chrono>

typedef std::chrono::system_clock::time_point TimePoint;
typedef std::chrono::system_clock::duration Duration;

uint32_t millis();

TimePoint nowTp();
std::string nowToStr(const char *pFmt = NULL);

std::string tpToStr(const TimePoint &tp, const char *pFmt = NULL);
TimePoint strToTp(const std::string &str, const char *pFmt = NULL);

size_t tpDiffSec(const TimePoint &tpEnd, const TimePoint &tpStart);
size_t tpDiffMs(const TimePoint &tpEnd, const TimePoint &tpStart);

#endif

