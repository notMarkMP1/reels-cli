#!/bin/bash

# Reels CLI Launcher Script
# This script starts both the Python client and C video player

set -e 

# resolve symlinks
SCRIPT_PATH="$(readlink -f "${BASH_SOURCE[0]}")"
SCRIPT_DIR="$(dirname "$SCRIPT_PATH")"

cd "$SCRIPT_DIR"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}🎬 Starting Reels CLI...${NC}"
echo -e "${YELLOW}📁 Working from: $SCRIPT_DIR${NC}"

# C
if [ ! -f "build/video_player" ]; then
    echo -e "${RED}❌ Error: build/video_player not found!${NC}"
    echo -e "${YELLOW}💡 Please build the C application first by running:${NC}"
    echo "   cd c && make"
    exit 1
fi

# PY
if [ ! -f "python/main.py" ]; then
    echo -e "${RED}❌ Error: python/main.py not found!${NC}"
    exit 1
fi

cleanup() {
    echo -e "\n${YELLOW}🧹 Cleaning up processes...${NC}"
    if [ ! -z "$PYTHON_PID" ]; then
        kill $PYTHON_PID 2>/dev/null || true
        echo -e "${GREEN}✅ Python client stopped${NC}"
    fi
    if [ ! -z "$C_PID" ]; then
        kill $C_PID 2>/dev/null || true
        echo -e "${GREEN}✅ C video player stopped${NC}"
    fi
    # remove socket file
    rm -f /tmp/uds_socket
    echo -e "${GREEN}🎉 Cleanup complete!${NC}"
}

trap cleanup EXIT INT TERM

echo -e "${GREEN}🐍 Starting Python client...${NC}"
cd python
if [ -f ".venv/bin/activate" ]; then
    . .venv/bin/activate
fi
python main.py &    
PYTHON_PID=$!
cd ..

# give python client a moment to start
sleep 2

# start C video player
echo -e "${GREEN}🎮 Starting C video player...${NC}"
./build/video_player
