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
	echo "cppclass.sh eat"
	echo ""
	echo "Result:"
	echo "Eating.cpp"
	echo "Eating.h"
	echo ""
	echo "Input:"
	echo "cppclass.sh banana-eat"
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
filename=$classname.py
thedate=$(date +%d.%m.%Y)
theyear=$(date +%Y)

# Python file
cat > $filename << EOF
from Processing import *

class ${classname}(Processing):

	def initialize(self):

		return Positive

	def process(self):

		return Pending

EOF

# Style
#astyle --suffix=none --style=linux --indent=force-tab=4 ${filename} &> /dev/null

# Debugging
cat ${filename}
