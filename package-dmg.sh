#!/usr/bin/env bash
# package-dmg.sh — build a Release iFancyZones.app, bundle Qt (macdeployqt) so it
# runs on machines without Qt, ad-hoc sign (with entitlements), and produce a
# drag-to-install DMG with an /Applications shortcut and the app's icon as the
# volume icon.
#
# Usage:  ./package-dmg.sh
# Output: apps/iFancyZones/releases/iFancyZones-<version>.dmg
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
SRC="$HERE/src/iFancyZones"
QT="${QT_PREFIX:-$HOME/Qt/6.11.1/macos}"
CM="${CMAKE:-$HOME/Qt/Tools/CMake/CMake.app/Contents/bin/cmake}"
NINJA="${NINJA:-$HOME/Qt/Tools/Ninja/ninja}"
MDQ="$QT/bin/macdeployqt"
REL="$HERE/releases"
BUILD="$SRC/build-release"
ICON="$SRC/packaging/iFancyZones.icns"
ENTITLEMENTS="$SRC/packaging/entitlements.plist"

# Version comes from CMakeLists project(iFancyZones VERSION x.y.z); override with VERSION=…
VERSION="${VERSION:-$(sed -n 's/^[[:space:]]*VERSION \([0-9][0-9.]*\).*/\1/p' "$SRC/CMakeLists.txt" | head -1)}"
VERSION="${VERSION:-1.0.0}"
echo "==> iFancyZones $VERSION"

echo "==> Configure + build (Release)"
"$CM" -S "$SRC" -B "$BUILD" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_MAKE_PROGRAM="$NINJA" \
    -DCMAKE_PREFIX_PATH="$QT" >/dev/null
"$CM" --build "$BUILD" --target iFancyZones >/dev/null

APP="$BUILD/iFancyZones.app"
[ -d "$APP" ] || { echo "ERROR: $APP not built"; exit 1; }

echo "==> macdeployqt (embed Qt frameworks)"
"$MDQ" "$APP" >/dev/null

echo "==> Verify self-contained (no Homebrew/Qt-install references)"
for f in "$APP/Contents/MacOS/iFancyZones" "$APP"/Contents/Frameworks/*.dylib; do
    [ -e "$f" ] || continue
    if otool -L "$f" | grep -q "/opt/homebrew\|/usr/local/opt\|$HOME/Qt"; then
        echo "ERROR: external dependency still referenced in $f"
        otool -L "$f" | grep -E "/opt/homebrew|/usr/local/opt|$HOME/Qt"; exit 1
    fi
done
echo "    OK — fully self-contained"

echo "==> Ad-hoc code sign (with entitlements)"
codesign --force --deep --options runtime \
    --entitlements "$ENTITLEMENTS" \
    --sign "${CODESIGN_IDENTITY:--}" "$APP"
codesign --verify --deep --strict "$APP"

echo "==> Build DMG"
mkdir -p "$REL"
STAGE="$(mktemp -d /tmp/ifz-dmg.XXXXXX)"
cp -R "$APP" "$STAGE/"
ln -s /Applications "$STAGE/Applications"          # drag-to-install shortcut
cp "$ICON" "$STAGE/.VolumeIcon.icns"               # DMG volume icon = app icon
cat > "$STAGE/READ ME.txt" <<'TXT'
iFancyZones — quản lý zone cửa sổ trên màn hình (giống FancyZones của Windows)
==============================================================================

Cài đặt: kéo  iFancyZones  thả vào thư mục  Applications.

Lần đầu mở: nếu macOS báo "unidentified developer", chuột phải vào app
→ Open → Open.

Quyền cần cấp: System Settings → Privacy & Security → Accessibility →
bật iFancyZones (để app có thể di chuyển / resize cửa sổ).

Author: Jason — tuanquynh.com
TXT

RW="$(mktemp -u /tmp/ifz-rw.XXXXXX).dmg"
MP="/Volumes/iFancyZones-build-$$"
hdiutil create -volname "iFancyZones" -srcfolder "$STAGE" -fs HFS+ -format UDRW -ov "$RW" >/dev/null
hdiutil attach "$RW" -mountpoint "$MP" -nobrowse >/dev/null
SETFILE="$(xcrun -f SetFile 2>/dev/null || echo /usr/bin/SetFile)"
[ -x "$SETFILE" ] && "$SETFILE" -a C "$MP" || true   # mark volume to use .VolumeIcon.icns
hdiutil detach "$MP" >/dev/null

OUT="$REL/iFancyZones-$VERSION.dmg"
TMP_OUT="$REL/.iFancyZones-$VERSION.new.dmg"        # write to temp then mv into place
hdiutil convert "$RW" -format UDZO -o "$TMP_OUT" -ov >/dev/null
mv -f "$TMP_OUT" "$OUT"
rm -f "$RW"; rm -rf "$STAGE"

# Give the .dmg FILE itself the app icon in Finder, via Cocoa NSWorkspace
# setIcon:forFile: (the reliable API; handles the resource fork + custom-icon flag).
echo "==> Set DMG file icon"
osascript - "$OUT" "$ICON" <<'OSA' >/dev/null 2>&1 && echo "    OK" || echo "    (skipped)"
use framework "AppKit"
on run argv
  set img to (current application's NSImage's alloc()'s initWithContentsOfFile:(item 2 of argv))
  current application's NSWorkspace's sharedWorkspace()'s setIcon:img forFile:(item 1 of argv) options:0
end run
OSA

echo "==> Done: $OUT"
ls -lh "$OUT"
