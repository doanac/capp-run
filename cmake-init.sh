#!/bin/sh -e

## Just an easy way to get cmake/ninja set up so you don't have to remember

HERE=$(dirname $(readlink -f $0))

if [ "$HERE" = "$(pwd)" ] ; then
	echo "ERROR: Don't run this from source directory."
	exit 1
fi

CMAKE_DEF_ARGS="-GNinja -DDOCKER_ARCH=amd64"
CMAKE_ARGS="${CMAKE_DEF_ARGS} $@"
echo "Building capp-run with the following arguments: ${CMAKE_ARGS}"
cmake ${CMAKE_ARGS} $HERE
