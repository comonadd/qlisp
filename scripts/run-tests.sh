#!/usr/bin/env bash

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
TESTS_DIR="$SCRIPT_DIR/../test"
source "$SCRIPT_DIR/build-debug.sh" && python "$TESTS_DIR/test.py"
