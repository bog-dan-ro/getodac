#!/bin/bash

cd `dirname $0`
rm -fr build
mkdir build || exit 1
cd build
cmake -G Ninja .. || exit 1
ninja all || exit 1
ninja test || exit 1
rm -fr build
exit 0
