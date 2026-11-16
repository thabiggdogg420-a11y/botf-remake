#!/usr/bin/env bash
cd "$(dirname "$0")/raw"
cat ../$1 | tr -d '\r' | xargs -P 3 -d '\n' -I{} bash -c 'IFS="|" read n s <<< "{}"; ../gen.sh "$n" "$s"'
