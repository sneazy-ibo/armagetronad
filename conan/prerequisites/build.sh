#/usr/bin/env bash

# builds prerequisites and puts them into your local conan repository
# all arguments passed to conan commands

export CXXFLAGS=""
export CPPFLAGS=""
export CFLAGS=""

cd `dirname $0`

set -x

# -of <output dir> does not apply here (and callers put it first)
if test "$1" == "-of"; then
    shift
    shift
fi

conan install sdl "$@" || exit $?
conan create sdl "$@" || exit $?

conan install sdl_image "$@" || exit $?
conan create sdl_image "$@" || exit $?
