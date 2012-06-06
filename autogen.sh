#!/bin/sh -xe

autoreconf -fiv
./configure "$@"
