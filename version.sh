#!/bin/sh -e

usage()
{
    echo "Usage: $0 staticVersion inputFile outputFile [--always-overwrite]" >&2
    exit 1
}

if [ $# -ne 3 ] && [ $# -ne 4 ]; then
    usage
fi

staticVersion="$1"
inputFile="$2"
outputFile="$3"
if [ "$4" = "--always-overwrite" ]; then
    alwaysOverwrite=true
elif [ -z "$4" ]; then
    alwaysOverwrite=false
else
    usage
fi

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
if $alwaysOverwrite || ! [ -e "$outputFile" ] || ! grep -q "\"$ver\"" "$outputFile"; then
	sed -e "s@%\<PROJECT_VERSION\>%@\"${ver}\"@" "$inputFile" > "$outputFile"
fi
