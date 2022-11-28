#!/usr/bin/env bash

for i in *.cpp
do
    g++ -pthread -Wall "$i" -o "${i%.cpp}.out"
done