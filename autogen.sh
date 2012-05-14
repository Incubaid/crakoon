#!/bin/sh -xe

autoreconf -f
./configure "$@"
