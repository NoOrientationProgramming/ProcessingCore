#!/bin/bash

if [ -z "$1" ]; then
	echo ""
	echo "Error: No classname specified"
	echo ""
	echo "Usage: namesegment(-namesegment)*"
	echo "  Name segments must be separated with dashes '-'"
	echo "  Ending 'ing' is added automatically to classname"
	echo ""
	echo "Examples"
	echo ""
	echo "Input:"
	echo "cppprocessing.sh eat"
	echo ""
	echo "Result:"
	echo "Eating.cpp"
	echo "Eating.h"
	echo ""
	echo "Input:"
	echo "cppprocessing.sh banana-eat"
	echo ""
	echo "Result:"
	echo "BananaEating.cpp"
	echo "BananaEating.h"
	echo ""
	exit 1
fi

# Parameters
inputname=$(echo $1 | tr '[:upper:]' '[:lower:]')
classname=$(echo "$inputname" | sed -e "s:\b\(.\):\u\1:g" -e "s:-\| ::g")
classname="${classname}ing"
inputname="${inputname}ing"
headerdef=$(echo "${inputname^^}" | sed -e "s:-:_:g")
cppfilename=$classname.cpp
hppfilename=$classname.h
thedate=$(date +%d.%m.%Y)
theyear=$(date +%Y)

# CPP file
cat > $cppfilename << EOF

#include "$hppfilename"

using namespace std;

${classname}::${classname}()
	: Processing("${classname}")
{}

/* member functions */

Success ${classname}::process()
{
	return Pending;
}

void ${classname}::processInfo(char *pBuf, char *pBufEnd)
{
	(void)pBuf;
	(void)pBufEnd;
}

/* static functions */

EOF

# Header file
cat > $hppfilename << EOF

#ifndef ${headerdef}_H
#define ${headerdef}_H

#include "Processing.h"

class ${classname} : public Processing
{

public:

	static ${classname} *create()
	{
		return new dNoThrow ${classname};
	}

protected:

	${classname}();
	virtual ~${classname}() {}

private:

	${classname}(const ${classname} &) = delete;
	${classname} &operator=(const ${classname} &) = delete;

	/*
	 * Naming of functions:  objectVerb()
	 * Example:              peerAdd()
	 */

	/* member functions */
	Success process();
	void processInfo(char *pBuf, char *pBufEnd);

	/* member variables */

	/* static functions */

	/* static variables */

	/* constants */

};

#endif

EOF

# Style
astyle --suffix=none --style=linux --indent=force-tab=4 ${cppfilename} ${hppfilename} &> /dev/null

# Debugging
cat ${cppfilename}
cat ${hppfilename}
