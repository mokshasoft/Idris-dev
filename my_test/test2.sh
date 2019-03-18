#!/bin/sh

echo "building..."
idris -o single single.idr
echo "running..."
./single
