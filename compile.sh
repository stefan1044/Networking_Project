#!/usr/bin/env bash

for i in *.c
do
    gcc -Wall "$i" -o "${i%.c}.out"
done