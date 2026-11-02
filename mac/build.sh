#!/bin/bash
# Baut "AGS Starter.app" auf einem Mac (wird von der GitHub Action aufgerufen).
# Erwartet SDL2 (statisch) im Ordner ./sdl
set -euo pipefail
cd "$(dirname "$0")/.."
SDL="$PWD/sdl"
rm -rf build dist; mkdir -p build dist

# 1) Programm uebersetzen (fuer Apple-Chips und Intel)
clang -std=gnu99 -O2 -Wall -Wno-unused-result \
  -arch arm64 -arch x86_64 -mmacosx-version-min=11.0 \
  $("$SDL/bin/sdl2-config" --cflags) \
  src/main.c -o build/agsstarter \
  $("$SDL/bin/sdl2-config" --static-libs)

# 2) App-Paket zusammenstellen
APP="dist/AGS Starter.app"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"
cp build/agsstarter      "$APP/Contents/MacOS/AGS Starter"
cp mac/Info.plist        "$APP/Contents/Info.plist"
cp mac/icon.icns         "$APP/Contents/Resources/icon.icns"
cp mac/ags               "$APP/Contents/Resources/ags"      # eingebaute AGS-Engine
cp mac/header.bmp        "$APP/Contents/Resources/header.bmp" # Header-Bild
chmod 755 "$APP/Contents/MacOS/AGS Starter" "$APP/Contents/Resources/ags"

# 3) "Ad-hoc"-Signatur (ohne Apple-Konto; noetig, damit Apple-Chips das Programm ausfuehren)
codesign --force --deep --sign - "$APP"

# 4) Als ZIP verpacken (ditto behaelt alle Rechte)
cp README.txt LICENSE THIRD-PARTY-NOTICES.md dist/
(cd dist && ditto -c -k --keepParent "AGS Starter.app" "AGS_Starter.zip" && zip -q AGS_Starter.zip README.txt LICENSE THIRD-PARTY-NOTICES.md)
ls -la dist
