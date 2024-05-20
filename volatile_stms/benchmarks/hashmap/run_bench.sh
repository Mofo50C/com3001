#!/bin/bash

rm -f log.txt
for n in {0..9};
do
	./bench-"$1" "${@:2}"
done >> log.txt