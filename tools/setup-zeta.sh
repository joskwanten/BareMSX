#!/bin/sh
# Fetch the Zeta Z80 CPU core (by Manuel Sainz de Baranda y Goñi) into zeta/.
# The core is not bundled here; this pulls it from upstream. See README.
set -e
cd "$(dirname "$0")/.."

TMP=$(mktemp -d)
git clone --depth 1 https://github.com/redcode/Zeta.git "$TMP/Zeta"
git clone --depth 1 https://github.com/redcode/Z80.git   "$TMP/Z80"

mkdir -p zeta
cp    "$TMP/Z80/sources/Z80.c" zeta/Z80.c
cp    "$TMP/Z80/API/Z80.h"     zeta/Z80.h
cp -R "$TMP/Zeta/API/Z"        zeta/Z

rm -rf "$TMP"
echo "Zeta Z80 core installed in zeta/"
