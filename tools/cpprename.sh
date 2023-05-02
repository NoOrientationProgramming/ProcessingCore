#!/bin/bash

helpPrint()
{
	if [ ! -z "$1" ]; then
		echo
		echo "Error: $1"
	fi
	echo
	echo "Usage: <old filename> <new classname> [options]"
	echo "  Used to rename a cpp class"
	echo
	echo "Examples:"
	echo "$(basename $0) ExampleClass.cpp app-config"
	echo "$(basename $0) ExampleClass.h app-config"
	echo
}

if [ -z "$1" ]; then
	helpPrint "No filename specified"
	exit 1
fi

if [ -z "$2" ]; then
	helpPrint "No classname specified"
	exit 1
fi

# Parameters
classNameOld=${1%.*}
splitName=$(echo ${classNameOld} | sed 's/\(.\)\([[:upper:]]\)/\1-\2/g')
lowerName=${splitName,,}
amNameOld=$(echo "${lowerName}" | sed -e "s:-:_:g")

cppFileOld="./${classNameOld}.cpp"
hppFileOld="./${classNameOld}.h"
headerDefOld="$(echo "${lowerName^^}" | sed -e "s:-:_:g")_H"

inputName=$2
className=$(echo "$inputName" | sed -e "s:\b\(.\):\u\1:g" -e "s:-\| ::g")
amName=$(echo "${inputName}" | sed -e "s:-:_:g")
headerDef="${amName^^}_H"
cppFile="./${className}.cpp"
hppFile="./${className}.h"
theDate=$(date +%d.%m.%Y)
theYear=$(date +%Y)

# Class name
find . -type f -regex ".*/.*\.\(cpp\|h\|am\|json\)" -exec sed -i -e "s:\b${classNameOld}\b:${className}:gI" {} \;

# App name
find . -type f -regex ".*/.*\.\(cpp\|am\)" -exec sed -i -e "s:\b${lowerName}\b:${inputName}:gI" {} \;

# Header define
find . -type f -regex ".*/.*\.\(h\)" -exec sed -i -e "s:\b${headerDefOld}\b:${headerDef}:gI" {} \;

# Makefile.am
find . -type f -regex ".*/.*\.\(am\)" -exec sed -i -e "s:\b${amNameOld}\b:${amName}:gI" {} \;

# meson.build
find . -type f -regex ".*/\(meson\)\.\(build\)" -exec sed -i -e "s:\b${classNameOld}\b:${className}:gI" {} \;

# Finally rename files
mv ${cppFileOld} ${cppFile}
mv ${hppFileOld} ${hppFile}

# Add renamed files zu staging area
git add ${cppFileOld}
git add ${cppFile}
git add ${hppFileOld}
git add ${hppFile}
