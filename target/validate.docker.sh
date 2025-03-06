#!/bin/bash
# Define color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

output1=$(eval /opt/llvm-project/build/bin/lli exec/orig.ll $@)
output2=$(eval /opt/llvm-project/build/bin/lli exec/obf.ll $@)

# Compare the results
if [ "$output1" == "$output2" ]; then
    echo -e "${GREEN}* Case #$((i + 1)) - OK${NC}\n${BLUE}Input:${NC} $@\n${BLUE}Output:${NC} $output1"
else
    echo -e "${RED}* Case #$((i + 1)) - ERROR${NC}\n${BLUE}Input:${NC} $@\n${BLUE}Expected output:${NC} $output1\n${BLUE}Received output:${NC} $output2"
fi