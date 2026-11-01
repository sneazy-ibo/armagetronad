#/usr/bin/env bash

# builds prerequisites and puts them into your local conan repository
# all arguments passed to conan commands

export CXXFLAGS=""
export CPPFLAGS=""
export CFLAGS=""

cd `dirname $0`

conan install sdl "$@" || exit $?
conan create sdl "$@" || exit $?

conan install sdl_image "$@" || exit $?
conan create sdl_image "$@" || exit $?
