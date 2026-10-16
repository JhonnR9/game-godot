#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
# Reuse the already configured godot-cpp read-only. Override for another checkout.
bindings_dir="${GODOT_CPP_DIR:-/home/jhonn/Documents/projects/game-godot/godot-cpp}"
mkdir -p project/bin/linux
c++ -std=c++17 -fPIC -shared -O2 -g -DDEBUG_ENABLED -DDEBUG_METHODS_ENABLED \
    -I "$bindings_dir/include" -I "$bindings_dir/gen/include" -I "$bindings_dir/gdextension" \
    src/*.cpp src/core/world/*.cpp src/core/geometry/*.cpp src/core/runtime/*.cpp "$bindings_dir/bin/libgodot-cpp.linux.template_debug.x86_64.a" \
    -o project/bin/linux/libEXTENSION-NAME.linux.template_debug.x86_64.so -pthread
