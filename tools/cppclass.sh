#!/bin/bash

if [ -z "$1" ]; then
	echo "Error: No filename specified"
	echo ""
	echo "Usage: <name> [options]"
	echo "  name segments must be separated with dashes '-'"
	echo ""
	echo "Examples:"
	echo "cppclass.sh app-config"
	echo "cppclass.sh example-app app"
	exit 1
fi

# Parameters
inputname=$1
classname=$(echo "$inputname" | sed -e "s:\b\(.\):\u\1:g" -e "s:-\| ::g")
headerdef=$(echo "${inputname^^}" | sed -e "s:-:_:g")
cppfilename=$classname.cpp
hppfilename=$classname.h
thedate=$(date +%d.%m.%Y)
theyear=$(date +%Y)

# CPP file
cat > $cppfilename << EOF
/*
  This file is part of the DSP-Crowd project
  https://www.dsp-crowd.com

  Author(s):
      - Johannes Natter, office@dsp-crowd.com

  File created on ${thedate}

  Copyright (C) ${theyear}, Johannes Natter

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

#include "$hppfilename"

using namespace std;

${classname}::${classname}()
{

}

${classname}::~${classname}()
{

}

EOF

if [ ! -z "$2" ]; then

cat >> $cppfilename << EOF
int ${classname}::exec(int argc, char *argv[]) {

	return init();
}

int ${classname}::init() {

	return 0;
}

EOF

fi

# Header file
cat > $hppfilename << EOF
/*
  This file is part of the DSP-Crowd project
  https://www.dsp-crowd.com

  Author(s):
      - Johannes Natter, office@dsp-crowd.com

  File created on ${thedate}

  Copyright (C) ${theyear}, Johannes Natter

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

#ifndef ${headerdef}_H
#define ${headerdef}_H

#include "Object.h"

class ${classname} : public Object
{

public:
	${classname}();
	virtual ~${classname}();

EOF

if [ ! -z "$2" ]; then

cat >> $hppfilename << EOF
	int exec(int argc, char *argv[]);

EOF

fi

cat >> $hppfilename << EOF
private:
EOF

if [ ! -z "$2" ]; then

cat >> $hppfilename << EOF
	int init();
EOF

fi

cat >> $hppfilename << EOF

};

#endif

EOF

# Style
astyle --suffix=none --style=linux --indent=force-tab=4 ${cppfilename} ${hppfilename} &> /dev/null

# Debugging
cat ${cppfilename}
cat ${hppfilename}
