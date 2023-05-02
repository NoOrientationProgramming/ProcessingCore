/*
  This file is part of the DSP-Crowd project
  https://www.dsp-crowd.com

  Author(s):
      - Johannes Natter, office@dsp-crowd.com

  File created on 27.04.2019

  Copyright (C) 2019-now Authors and www.dsp-crowd.com

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

#ifndef JSON_PROCESSING_H
#define JSON_PROCESSING_H

#include "Processing.h"
#include <jsoncpp/json/json.h>
#include "Pipe.h"

class JsonProcessing : public Processing
{

public:

	void setArguments(Json::Value &args)
	{
		mArgs = args;
	}
	const Json::Value &result() const
	{
		return mResult;
	}

	Pipe<Json::Value> ppJsonIn;
	Pipe<Json::Value> ppJsonOut;

	Json::Value mArgs;
	Json::Value mResult;

protected:

	JsonProcessing(const char *name)
		: Processing(name)
	{}
	virtual ~JsonProcessing() {}

private:

	JsonProcessing() : Processing("") {}
	JsonProcessing(const JsonProcessing &) : Processing("") {}
	JsonProcessing &operator=(const JsonProcessing &)
	{
		return *this;
	}

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

#define JS(x) (x.isString() ? x.asString().c_str() : "")

#if CONFIG_PROC_USE_STD_LISTS
typedef std::list<JsonProcessing *> JsonProcs;
typedef JsonProcs::iterator JsonProcsIter;
#endif

#endif

