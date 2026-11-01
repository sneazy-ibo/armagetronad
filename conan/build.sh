#!/usr/bin/env bash

# builds conan libraries
# all arguments passed to 'conan build'

cd $(dirname $0)

# naive attempt at building conan
if ! CFLAGS="" CXXFLAGS="" CPPFLAGS="" conan build ./conanfile.py "$@"; then
    # we are probably missing prerequisites
    ./prerequisites/build.sh "$@" || exit $?
    # try again
    CFLAGS="" CXXFLAGS="" CPPFLAGS="" conan build ./conanfile.py "$@" || exit $?
fi
