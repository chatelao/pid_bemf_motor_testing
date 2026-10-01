#!/usr/bin/env bash
set -e

# Target directory for packaged UF2 release assets
DIST_DIR="${1:-dist}"
mkdir -p "$DIST_DIR"

echo "=== Packaging Root Firmware ==="
pio run
if [ -f ".pio/build/seeed_xiao_rp2040/firmware.uf2" ]; then
    cp ".pio/build/seeed_xiao_rp2040/firmware.uf2" "$DIST_DIR/Marklin_Motor_Control.uf2"
    echo "Copied root firmware -> $DIST_DIR/Marklin_Motor_Control.uf2"
else
    echo "Error: Root firmware.uf2 not found!"
    exit 1
fi

echo "=== Packaging Example Firmwares ==="
for example in examples/*; do
    if [ -d "$example" ] && [ -f "$example/platformio.ini" ]; then
        name=$(basename "$example")
        echo "Building example: $name ..."
        (cd "$example" && pio run)

        uf2_path="$example/.pio/build/seeed_xiao_rp2040/firmware.uf2"
        if [ -f "$uf2_path" ]; then
            cp "$uf2_path" "$DIST_DIR/${name}.uf2"
            echo "Copied example $name -> $DIST_DIR/${name}.uf2"
        else
            echo "Error: $uf2_path not found for example $name"
            exit 1
        fi
    fi
done

echo "=== Packaged UF2 Assets in $DIST_DIR ==="
ls -lh "$DIST_DIR"/*.uf2
