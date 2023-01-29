#!/bin/bash

rm -f ./runner/src/*.cpp
cp "./code/$1" "./runner/src/"
cd ./runner/build
cmake ..
make
./app
