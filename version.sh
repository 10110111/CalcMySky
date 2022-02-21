#!/bin/sh -e

if [ $# -ne 3 ]; then
	echo "Usage: $0 staticVersion inputFile outputFile" >&2
	exit 1
fi

staticVersion="$1"
inputFile="$2"
outputFile="$3"

cd $(dirname "$0")
ver="$(git describe --always --dirty)"
if [ -z "$ver" ]; then
    ver="$staticVersion"
fi
cd -
if ! [ -e "$outputFile" ] || ! grep -q "\"$ver\"" "$outputFile"; then
	sed -e "s@%\<PROJECT_VERSION\>%@\"${ver}\"@" "$inputFile" > "$outputFile"
fi
