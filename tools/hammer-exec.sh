#!/bin/bash

# Use by:
#  ./hammer-exec.sh <path to exec>
#
# For example:
# ./hamer-exec.sh ./out/Release/run-nub-tests

if [ ! -f $1 ]; then
  echo "File does not exist"
  exit
fi

for i in `seq 100000`; do
  if [ `expr $i % 50` -eq 0 ]; then
    echo $i
  fi
  $1
  if [ $? -ne 0 ]; then
    break
  fi
done
