#!/bin/sh

echo "building..."
idris -o coop coop.idr
echo "running..."
./coop
