#!/bin/bash
# Build and run the SeriWrap protocol unit test (no Qt, no board needed).
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../.." && pwd)
OUT=${1:-/tmp/seriwrap_protocol_test}
g++ -std=c++17 -Wall -Wextra -I "$ROOT/rabbit_App/include" \
    "$HERE/seriwrap_protocol_test.cpp" \
    "$ROOT/rabbit_App/src/Components/SeriWrapProtocol.cpp" \
    -o "$OUT"
"$OUT"
