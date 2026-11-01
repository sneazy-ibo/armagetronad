#!/usr/bin/env bash

# Rebuilds conan.lock files

# Run every now and then, whenever we need to adapt to new
# versions of dependencies.
# It is normal that the sdl_aa and sdl_image_aa entries
# fluctuate a bit at the end even without changes.
# Only commit if there were other changes as well.

cd $(dirname $0)

find . -name conan.lock -exec rm \{\} \;
./prerequisites/build.sh || exit $?
conan lock create prerequisites/sdl
conan lock create prerequisites/sdl_image
conan lock create .
