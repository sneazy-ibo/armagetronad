#/usr/bin/env bash

# builds prerequisites and puts them into your local conan repository

export CXXFLAGS=""
export CPPFLAGS=""
export CFLAGS=""

cd `dirname $0`

conan install ftgl --build=missing || exit $?
conan create ftgl --build=missing || exit $?

