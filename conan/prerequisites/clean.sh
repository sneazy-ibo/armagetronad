#/usr/bin/env bash

# cleans prerequisites

cd `dirname $0`

yes | conan remove ftgl_aa

true
