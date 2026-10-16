#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
mkdir -p build
c++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -fsanitize=undefined -fno-sanitize-recover=all \
    -I src tests/test_world_contracts.cpp -o build/test_world_contracts
./build/test_world_contracts
c++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -fsanitize=undefined -fno-sanitize-recover=all \
    -I src tests/test_world_repository.cpp src/core/world/world_repository.cpp \
    src/core/world/dependency_stencil.cpp -o build/test_world_repository
./build/test_world_repository
c++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -fsanitize=undefined -fno-sanitize-recover=all \
    -I src tests/test_surface_contracts.cpp src/core/geometry/surface_contracts.cpp -o build/test_surface_contracts
./build/test_surface_contracts
c++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -fsanitize=undefined -fno-sanitize-recover=all \
    -I src tests/test_surface_build.cpp src/core/geometry/surface_contracts.cpp \
    src/core/geometry/surface_build_service.cpp src/core/geometry/convex_clipper.cpp \
    src/core/geometry/picking_bvh.cpp src/core/world/world_repository.cpp \
    src/core/world/dependency_stencil.cpp -o build/test_surface_build
./build/test_surface_build
c++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -fsanitize=undefined -fno-sanitize-recover=all \
    -I src tests/test_bridge_candidates.cpp src/core/geometry/surface_contracts.cpp \
    src/core/geometry/surface_build_service.cpp src/core/geometry/convex_clipper.cpp \
    src/core/geometry/picking_bvh.cpp src/core/world/world_repository.cpp \
    src/core/world/dependency_stencil.cpp -o build/test_bridge_candidates
./build/test_bridge_candidates
c++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -pthread -fsanitize=undefined -fno-sanitize-recover=all \
    -I src tests/test_job_scheduler.cpp src/core/runtime/job_scheduler.cpp -o build/test_job_scheduler
./build/test_job_scheduler
c++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -pthread -fsanitize=undefined -fno-sanitize-recover=all \
    -I src tests/test_surface_job_queue.cpp src/core/runtime/surface_job_queue.cpp \
    src/core/runtime/job_scheduler.cpp src/core/geometry/surface_build_service.cpp \
    src/core/geometry/surface_contracts.cpp src/core/geometry/convex_clipper.cpp \
    src/core/geometry/picking_bvh.cpp src/core/world/world_repository.cpp \
    src/core/world/dependency_stencil.cpp -o build/test_surface_job_queue
./build/test_surface_job_queue
c++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -fsanitize=undefined -fno-sanitize-recover=all \
    -I src tests/test_terrain.cpp src/gradient_noise.cpp src/terrain_generator.cpp \
    src/octahedron_geometry.cpp src/chunk_mesher.cpp src/surface_joiner.cpp -o build/test_terrain
./build/test_terrain
c++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -fsanitize=undefined -fno-sanitize-recover=all \
    -I src tests/test_surface_joiner.cpp src/gradient_noise.cpp src/terrain_generator.cpp \
    src/octahedron_geometry.cpp src/chunk_mesher.cpp src/surface_joiner.cpp -o build/test_surface_joiner
./build/test_surface_joiner
c++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -fsanitize=undefined -fno-sanitize-recover=all \
    -I src tests/test_ambient_occlusion.cpp src/ambient_occlusion.cpp \
    src/octahedron_geometry.cpp src/chunk_mesher.cpp src/surface_joiner.cpp -o build/test_ambient_occlusion
./build/test_ambient_occlusion
godot --headless --path project --script res://tests/test_terrain.gd
