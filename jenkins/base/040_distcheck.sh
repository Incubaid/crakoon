#!/bin/bash -xue

make distclean
./configure
MAKE_DISTCHECK=1 make distcheck
