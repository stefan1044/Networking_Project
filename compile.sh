#!/usr/bin/env bash

for i in *.cpp
do
    g++ -Wall "$i" -o "${i%.cpp}.out"
done