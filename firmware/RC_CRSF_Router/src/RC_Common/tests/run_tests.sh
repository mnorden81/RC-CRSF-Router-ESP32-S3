#!/bin/sh
# RC_Common Host-Tests (ohne ESP32): sh RC_Common/tests/run_tests.sh
# Braucht g++ und git (Unity v2.6.1 wird beim ersten Lauf geholt).
set -e
T=$(cd "$(dirname "$0")" && pwd)
R=$(dirname "$T")
# Unity und Build ausserhalb des Sketch-Ordners, sonst kompiliert die
# Arduino-IDE sie mit
U="${UNITY_DIR:-${TMPDIR:-/tmp}/rc_common_unity}"
[ -f "$U/src/unity.c" ] || git -c advice.detachedHead=false clone -q --depth 1 -b v2.6.1 https://github.com/ThrowTheSwitch/Unity "$U"
B="${BUILD_DIR:-${TMPDIR:-/tmp}/rc_common_build}"
mkdir -p "$B"
CXX="${CXX:-g++}"
FLAGS="-DRC_COMMON_HOST_TEST -std=c++17 -Wall -Wno-unused-function -I$R -I$U/src"
"${CC:-gcc}" -I"$U/src" "$U/src/unity.c" -c -o "$B/unity.o"
LIB="$R/CRSF_ESP32/crsf_esp32.cpp $R/uart_roles.cpp $R/lua_menu.cpp $R/status_led.cpp $R/settings_common.cpp $R/gps_autobaud.cpp $R/config_backup.cpp"
"$CXX" $FLAGS "$T/test_crsf.cpp"   $LIB "$B/unity.o" -o "$B/test_crsf"
"$CXX" $FLAGS "$T/test_common.cpp" $LIB "$B/unity.o" -o "$B/test_common"
"$CXX" $FLAGS "$T/test_gps.cpp"    $LIB "$B/unity.o" -o "$B/test_gps"
"$B/test_crsf"
"$B/test_common"
"$B/test_gps"
