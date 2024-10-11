#!/bin/bash

#  This file is part of the DSP-Crowd project
#  https://www.dsp-crowd.com
#
#  Author(s):
#      - Johannes Natter, office@dsp-crowd.com
#
#  File created on 09.03.2023
#
#  Copyright (C) 2023, Johannes Natter

modLineSet()
{
	s="$1"
	category="0"

	if [ "$s" -lt "100" ]; then
		category="0"
		mod=""
	elif [ "$s" -gt "600" ]; then
		category="2"
		mod="\033[91;1m"
	elif [ "$s" -lt "200" ]; then
		category="0"
		mod="\033[93;1m"
	elif [ "$s" -gt "500" ]; then
		category="2"
		mod="\033[93;1m"
	else
		category="1"
		mod="\033[32;1m"
	fi

	return "$category"
}

modGreenSet()
{
	s="$1"

	if [ "$s" -lt "35" ]; then
		mod="\033[91;1m"
	elif [ "$s" -lt "60" ]; then
		mod="\033[93;1m"
	else
		mod="\033[32;1m"
	fi
}

linesPrint()
{
	mod=""

	numLinesSum=0
	numFiles=0
	numSmall=0
	numGreen=0
	numBig=0
	numProcs=0
	numLibs=0

	fType="cpp"
	if [ -n "$1" ]; then
		fType="$1"
	fi

	srcs="$(find . -name "*.${fType}" | xargs wc -l | grep -v resources | grep -v total | grep -v build)"
	srcs="$(echo "$srcs" | sort -n | tr -s ' ' | sed 's:^[ \t]*::g' | sed 's: :|:g')"
	#echo "$srcs" | hexdump -n 64 -C
	#echo "$srcs" | head -n 10

	echo
	echo "----------------------------------"

	for s in $srcs; do

		#echo "~${s}~"

		numLines="$(echo "$s" | cut -d '|' -f 1)"
		f="$(echo "$s" | cut -d '|' -f 2)"
		#echo "~${f}~"
		bName="$(basename $f .$fType)"
		dName="$(dirname $f)"
		#f="${f:2}"

		if [ "$bName" == "main" ]; then
			continue
		fi

		if [[ "$bName" == *Supervising ]]; then
			continue
		fi

		modLineSet "$numLines"
		category="$?"
		if [ "$category" == "0" ]; then
			numSmall=$((numSmall + 1))
		elif [ "$category" == "1" ]; then
			numGreen=$((numGreen + 1))
		else
			numBig=$((numBig + 1))
		fi

		lic="-"

		found="$(grep "free of charge" "$f")"
		if [ -n "$found" ]; then
			lic="MIT"
		fi

		found="$(grep "version 3 of the License" "$f")"
		if [ -n "$found" ]; then
			lic="GPLv3"
		fi

		printf "$mod%25s\033[0m   %-8s%-40s%s\n" "$numLines" "$lic" "$bName" "$dName"

		if [ "${bName: -3}" == "ing" ]; then
			numProcs=$((numProcs + 1))
		fi

		if [ "${bName:0:3}" == "Lib" ]; then
			numLibs=$((numLibs + 1))
		fi

		numLinesSum=$((numLinesSum + numLines))
		numFiles=$((numFiles + 1))
	done

	echo "----------------------------------"
	printf "Lines\033[96;1m%20s\033[0m\n" "$numLinesSum"

	printf "\n"

	d=$(($numLinesSum / $numFiles))
	modLineSet "$d"
	printf "Average$mod%18s\033[0m\n\n" "$d"

	echo "----------------------------------"
	printf "Files%20s (%s)\n" "$numFiles" "$fType"

	printf "\n"

	numOther=$(($numFiles - $numProcs - $numLibs))
	printf "Processes%16s\n" "$numProcs"
	printf "Libraries%16s\n" "$numLibs"
	printf "Other%20s\n" "$numOther"

	printf "\n"

	c1=$(($numSmall * 100 / $numFiles))
	printf "Small%20s (%d%%\033[0m)\n" "$numSmall" "$c1"

	c2=$(($numGreen * 100 / $numFiles))
	modGreenSet "$c2"
	printf "Green%20s ($mod%d%%\033[0m)\n" "$numGreen" "$c2"

	c3=$((100 - $c1 - $c2))
	printf "Big%22s (%d%%\033[0m)\n" "$numBig" "$c3"

	printf "\n"

	echo "----------------------------------"
	g=$(git slog 2> /dev/null | wc -l)
	printf "Commits%18s\n\n" "$g"
}

linesPrint $* | less -R +G

