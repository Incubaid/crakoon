#!/bin/bash -xue

make distclean
./configure
make distcheck
