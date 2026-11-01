#!/usr/bin/env bash

# Rebuilds conan.lock files

# Run every now and then, whenever we need to adapt to new
# versions of dependencies.
# It is normal that the sdl_aa and sdl_image_aa entries
# fluctuate a bit at the end even without changes.
# Only commit if there were other changes as well.

cd "$(dirname $0)"

if test -z "$CONAN_HOME"; then
    root="$(dirname $(pwd))"
    echo "This script probably does the wrong thing without a custom CONAN_HOME, set it."
    if ls $root/build/conan_home* > /dev/null 2>&1; then
        echo "Suggestions:"
        echo
        for f in $root/build/conan_home*; do echo export CONAN_HOME=$f; done
        echo
    else
        echo
        echo Run \"CONAN=1 ./batch/test_builds.sh client\" to create a suitable home.
        echo
    fi
    exit 1
fi

for d in prerequisites/*; do
    test -d $d && conan lock create $d
done
if ! conan lock create .; then
    ./prerequisites/build.sh || exit $?
    conan lock create .
fi

