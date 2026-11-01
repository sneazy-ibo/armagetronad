#!/usr/bin/env bash

cd $(dirname $0)

find . -name conan.lock -exec rm \{\} \;
./prerequisites/build.sh || exit $?
conan lock create prerequisites/sdl
conan lock create prerequisites/sdl_image
conan lock create .
