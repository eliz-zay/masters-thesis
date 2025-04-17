#!/bin/bash
set -e

BLUE='\033[0;34m'
NC='\033[0m' # No Color

if [ "$#" -ne 2 ]; then
  echo -e "${BLUE}Usage: $0 <obfuscation_modes> <output_file.out>${NC}"
  exit 1
fi

OBFUSCATION_MODES=$1
OUT_FILE="/app/out/$2"

mkdir -p build/src

touch build/includes.h

python3 annotate.py /app/in/target.c build/src/target.c $OBFUSCATION_MODES
./obfuscate.sh build/src/target.c "$OUT_FILE"
