#!/usr/bin/env bash

for i in *.c
do
    g++ -Wall "$i" -o "${i%.cpp}.out"
done