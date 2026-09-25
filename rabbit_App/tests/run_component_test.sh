#!/bin/bash
# Lightweight build+run for the component tests (Stream family + SeriWrapComponent).
#
# A full rabbit_App build (xmake, every app source, in parallel) needs more memory
# than this machine has and took the session harness down with it.  This compiles
# ONLY the component library and the test, one file at a time, under nice, and
# reuses objects across runs.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../.." && pwd)
APP=$ROOT/rabbit_App
QT=${QT_DIR:-/home/camel/tools/qt/Qt/6.5.3/gcc_64}
MOC=$QT/libexec/moc
BUILD=${1:-$HERE/.component_build}
mkdir -p "$BUILD/moc"

INC="-I$APP/include -I$QT/include -I$QT/include/QtCore -I$QT/include/QtGui -I$QT/include/QtWidgets"
FLAGS="-std=c++17 -O1 -fPIC -Wall -Wno-unused-parameter"
objs=""
compile() {
    local f=$1 o
    o="$BUILD/$(basename "$1" .cpp).o"
    if [ -f "$o" ] && [ "$o" -nt "$f" ]; then objs="$objs $o"; return 0; fi
    echo "  cc $(basename "$f")"
    nice -n 10 g++ $FLAGS $INC -c "$f" -o "$o"
    objs="$objs $o"
}

echo "[1/3] moc"
for h in "$APP"/include/Components/*.h "$APP"/include/*.h; do
    [ -f "$h" ] || continue
    grep -q Q_OBJECT "$h" || continue
    b=$(basename "$h" .h)
    o="$BUILD/moc/moc_$b.o"
    if [ -f "$o" ] && [ "$o" -nt "$h" ]; then objs="$objs $o"; continue; fi
    nice -n 10 "$MOC" $INC "$h" -o "$BUILD/moc/moc_$b.cpp"
    nice -n 10 g++ $FLAGS $INC -c "$BUILD/moc/moc_$b.cpp" -o "$o"
    objs="$objs $o"
done

echo "[2/3] compile component library + test"
for f in "$APP"/src/Components/*.cpp; do compile "$f"; done
for f in "$APP"/src/Ports/*.cpp; do [ -f "$f" ] && compile "$f"; done
compile "$HERE/component_test.cpp"

echo "[3/3] link + run"
nice -n 10 g++ $FLAGS $objs -o "$BUILD/component_test" \
    -L"$QT/lib" -lQt6Widgets -lQt6Gui -lQt6Core -Wl,-rpath,"$QT/lib"
QT_QPA_PLATFORM=offscreen "$BUILD/component_test"
