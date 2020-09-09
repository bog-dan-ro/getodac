#!/bin/bash

function quit {
    killall GETodac
    exit 1
}

cd `dirname $0`
git clean -d -x -f .
git submodule update --init --recursive
git submodule foreach --recursive git clean -d -x -f .

function build_test {
    rm -fr build
    mkdir build || quit
    cd build
    cmake -G Ninja $1 .. || quit
    ninja all || quit
    killall GETodac
    ninja test || quit
    killall GETodac
    cd ..
    rm -fr build
}

# "" "-DSANITIZE_ADDRESS=ON -DSANITIZE_UNDEFINED=ON" "-DSANITIZE_THREAD=ON -DSANITIZE_UNDEFINED=ON"
# Disable ADDRESS SANITIZE until we find a way to address:
# - https://github.com/boostorg/coroutine2/issues/12
# - https://github.com/google/sanitizers/issues/189#issuecomment-312914329

for cp in "" "-DSANITIZE_UNDEFINED=ON"
do
    build_test "$cp"
done

exit 0
