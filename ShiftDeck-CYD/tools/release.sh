#!/bin/sh
# Builds the firmware and packs it for the browser installer (flasher/).
#   sh tools/release.sh            -> flasher/firmware/shiftdeck.bin + version stamped in the manifests
#   sh tools/release.sh --publish  -> also pushes flasher/ to the public installer repo (GitHub Pages)
# The image is merged (bootloader + partitions + app) and flashed at 0x0; it
# stops before the LittleFS partition, so settings survive an update.
set -e
cd "$(dirname "$0")/.."

python3 -m platformio run -e cyd
B=.pio/build/cyd
PKG=~/.platformio/packages
VERSION=$(sed -n 's/^#define VERSION *"\(.*\)"/\1/p' src/config.h)
mkdir -p flasher/firmware
python3 "$PKG/tool-esptoolpy/esptool.py" --chip esp32 merge_bin -o flasher/firmware/shiftdeck.bin \
  --flash_mode dio --flash_freq 40m --flash_size 4MB \
  0x1000 "$B/bootloader.bin" 0x8000 "$B/partitions.bin" \
  0xe000 "$PKG/framework-arduinoespressif32/tools/partitions/boot_app0.bin" 0x10000 "$B/firmware.bin"
# App image alone: what Updater downloads over the air (src/Updater.h)
cp "$B/firmware.bin" flasher/firmware/shiftdeck-app.bin
for m in flasher/manifest-*.json; do
  sed -i '' "s/\"version\": *\"[^\"]*\"/\"version\": \"$VERSION\"/" "$m"
done
echo "flasher/firmware/shiftdeck.bin ready (v$VERSION)"

if [ "$1" = "--publish" ]; then
  REPO=${FLASHER_REPO:-$(gh api user --jq .login)/shiftdeck-flasher}
  TMP=$(mktemp -d)
  gh repo clone "$REPO" "$TMP" -- --depth 1
  rsync -a --delete --exclude .git flasher/ "$TMP"/
  cd "$TMP"
  git add -A
  git commit -q -m "ShiftDeck v$VERSION" || { echo "nothing new to publish"; exit 0; }
  git push -q
  echo "published v$VERSION to https://github.com/$REPO"
fi
