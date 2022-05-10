#!/bin/sh -e

if [ $# -ne 3 ]; then
	echo "Usage: $0 staticVersion inputFile outputFile" >&2
	exit 1
fi

staticVersion="$1"
inputFile="$2"
outputFile="$3"

cd $(dirname "$0")
if ! [ -d .git ]; then
    ver=
elif [ "`whoami`" = root ]; then
    # Prevent git paranoid "unsafe directory" error when
    # running as root (e.g. while doing `sudo make install`)
    owner=$(stat -c %U .)
    ver="$(su -c 'git describe --always --dirty' "$owner")"
else
    ver="$(git describe --always --dirty)"
fi
if [ -z "$ver" ]; then
    ver="$staticVersion"
fi
cd - >/dev/null
if ! [ -e "$outputFile" ] || ! grep -q "\"$ver\"" "$outputFile"; then
	sed -e "s@%\<PROJECT_VERSION\>%@\"${ver}\"@" "$inputFile" > "$outputFile"
fi
