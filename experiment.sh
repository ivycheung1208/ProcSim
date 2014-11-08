#!/bin/bash

for r in {1..4}; do
for j in {1..2}; do
for k in {1..2}; do
for l in {1..2}; do
./procsim -r $r -j $j -k $k -l $l -f 8 <mcf.100k.trace
done
done
done
done

