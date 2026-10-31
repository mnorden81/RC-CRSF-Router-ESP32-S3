#!/bin/sh
# Schreibt MANIFEST.sha256 neu (nach jeder Aenderung an RC_Common).
# Pruefen:  cd RC_Common && sha256sum -c MANIFEST.sha256
cd "$(dirname "$0")"
find . -type f ! -name MANIFEST.sha256 \
    | LC_ALL=C sort | xargs sha256sum > MANIFEST.sha256
