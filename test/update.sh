#!/bin/bash
set -e  # stop on first failure

cd ..
./build.sh
mv build/novac test/novac
cd test
./novac test.nl
clang test.cpp test.ll -o nova_program -lraylib
./nova_program