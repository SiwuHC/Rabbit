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
# Qt location: QT_DIR if given, otherwise the usual install roots.  No absolute
# path of one particular machine is baked in.
QT=${QT_DIR:-}
if [ -z "$QT" ]; then
    for c in "$HOME/tools/qt/Qt/6.5.3/gcc_64" /opt/Qt/6.5.3/gcc_64 \
             /opt/qt/6.5.3/gcc_64 /usr/lib/x86_64-linux-gnu/qt6; do
        [ -d "$c" ] && QT=$c && break
    done
fi
if [ -z "$QT" ] || [ ! -d "$QT" ]; then
    echo "cannot find Qt: set QT_DIR=/path/to/Qt/6.x/gcc_64" >&2
    exit 1
fi
MOC=$QT/libexec/moc
BUILD=${1:-$HERE/.component_build}
mkdir -p "$BUILD/moc"

# 3rdparty: MainTabToolBar.h pulls in TabToolbar/Builder.h, which lives in the
# vendored TabToolbar tree.
INC="-I$APP/include -I$APP/3rdparty/TabToolbar/include -I$ROOT/vlfd-ffi -I$QT/include -I$QT/include/QtCore \
     -I$QT/include/QtGui -I$QT/include/QtWidgets"
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
# Only the headers whose Q_OBJECT classes this binary links (same set as the
# xmake target): moc-ing the window/FPGA/waveform headers would pull in
# classes that are deliberately not compiled here.
for h in "$APP"/include/Components/*.h "$APP"/include/ThreadTimer.h \
         "$APP"/include/Ports/*.h; do
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
# Utils/ThreadTimer are referenced by AbstractComponent and the value-update
# controller (xmake compiles them for this target too).
for f in "$APP"/src/Utils.cpp "$APP"/src/ThreadTimer.cpp; do [ -f "$f" ] && compile "$f"; done
compile "$HERE/component_test.cpp"

echo "[3/3] link + run"
nice -n 10 g++ $FLAGS $objs -o "$BUILD/component_test" \
    -L"$QT/lib" -lQt6Widgets -lQt6Gui -lQt6Core -Wl,-rpath,"$QT/lib"
QT_QPA_PLATFORM=offscreen "$BUILD/component_test"
