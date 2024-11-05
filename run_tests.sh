#!/bin/bash

for infile in tests/*.in; do
  base_name=$(basename "$infile" .in)
  echo "Running test: $base_name"

  ./grader ./engine < "$infile"
done
