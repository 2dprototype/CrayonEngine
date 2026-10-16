#!/usr/bin/env bash
# Builds and runs every headless test (no GPU, no window, no CMake needed): the CPU rasterizer,
# software-backend engine tests, culling/batching/terrain/skybox/transparency tests and the Lua
# library tests. Needs g++ (C++20), gcc, python3 and (for the Lua tests) a system Lua 5.x library.
#   tools/run_tests.sh            run everything
#   tools/run_tests.sh culling    run only tests whose name contains "culling"
set -u
cd "$(dirname "$0")/.."
OBJ="${TMPDIR:-/tmp}/crayon_test_obj"; mkdir -p "$OBJ"
INC="-Isrc -Ivendor/glad/include -Ivendor/glm -Ivendor/luajit/include -Ivendor/stb/include -Ivendor/tinyobj/include -Ivendor/tinygltf/include -Ivendor/sdl3/include -Ivendor/imgui"
FLAGS="-std=c++20 -O1 -DGLM_ENABLE_EXPERIMENTAL"
FILTER="${1:-}"
pass=0; fail=0

echo "== building engine objects"
for f in mesh3d mesh3d_soft model3d texture batch2d font shader camera image_data static_batch3d lightmap2d skybox terrain; do
  g++ $FLAGS $INC -c src/graphics/$f.cpp -o "$OBJ/$f.o" & done
g++ -std=c++20 -O2 -Isrc -Ivendor/glm -c src/graphics/soft/soft_raster.cpp -o "$OBJ/soft_raster.o" &
gcc -O1 -c vendor/glad/src/glad.c -Ivendor/glad/include -o "$OBJ/glad.o" &
gcc -O1 -w -DTINYGLTF3_ENABLE_FS -Ivendor/tinygltf/include -c vendor/tinygltf/src/tiny_gltf_v3.c -o "$OBJ/tinygltf.o" &
wait

run() {   # name [args...]
  local name=$1; shift
  [[ -n "$FILTER" && "$name" != *"$FILTER"* ]] && return
  if g++ $FLAGS $INC test/soft/$name.cpp "$OBJ"/*.o -o "$OBJ/t_$name" -lm -ldl 2>"$OBJ/$name.err"; then
    if timeout 300 "$OBJ/t_$name" "$@" > "$OBJ/$name.out" 2>&1; then echo "  PASS  $name"; pass=$((pass+1));
    else echo "  FAIL  $name   (see $OBJ/$name.out)"; fail=$((fail+1)); fi
  else echo "  FAIL  $name   (build error, see $OBJ/$name.err)"; fail=$((fail+1)); fi
}
echo "== C++ tests"
run culling_test
run texture_state_test
run features2d_test
run transparency_test
run skybox_test
run terrain_test
run static_batch_test
run soft_engine_test examples/assets/models/monkey.obj "" "$OBJ/engine_frame"

# Real-physics tests need Jolt. Point JOLT_LIB at a libJolt.a built for this machine
# (cmake the bundled vendor/JoltPhysics, or compile its Jolt/**/*.cpp with
#  -DJPH_DEBUG_RENDERER -DJPH_PROFILE_ENABLED -DJPH_OBJECT_STREAM). Skipped when unset.
if [[ -n "${JOLT_LIB:-}" && ( -z "$FILTER" || "physics" == *"$FILTER"* || "$FILTER" == "physics"* ) ]]; then
  echo "== physics tests (Jolt)"
  PH="$OBJ/phys"; mkdir -p "$PH"
  PD="-DJPH_DEBUG_RENDERER -DJPH_PROFILE_ENABLED -DJPH_OBJECT_STREAM"
  if g++ $FLAGS $PD $INC -Ivendor/JoltPhysics -Ivendor/box2d/include -c src/physics/physics_system.cpp -o "$PH/physics_system.o" 2>"$PH/build.err" \
     && g++ $FLAGS $PD $INC -Ivendor/JoltPhysics -c src/physics/model_colliders.cpp -o "$PH/model_colliders.o" 2>>"$PH/build.err" \
     && g++ $FLAGS $PD $INC -Ivendor/JoltPhysics test/soft/physics_shapes_test.cpp "$PH"/*.o "$OBJ"/*.o -o "$PH/t_physics" "$JOLT_LIB" -lpthread -lm -ldl 2>>"$PH/build.err"; then
    if timeout 600 "$PH/t_physics" > "$PH/out.txt" 2>&1; then echo "  PASS  physics_shapes_test"; pass=$((pass+1)); else echo "  FAIL  physics_shapes_test (see $PH/out.txt)"; fail=$((fail+1)); fi
  else echo "  FAIL  physics_shapes_test (build error, see $PH/build.err)"; fail=$((fail+1)); fi
elif [[ -z "$FILTER" ]]; then
  echo "== physics tests: skipped (set JOLT_LIB=/path/to/libJolt.a to run them)"
fi

if [[ -z "$FILTER" || "lua" == *"$FILTER"* || "$FILTER" == "lua" ]]; then
  echo "== rasterizer unit test"
  if g++ -std=c++20 -O2 -Isrc -Ivendor/glm -Ivendor/tinyobj/include test/soft/soft_raster_test.cpp src/graphics/soft/soft_raster.cpp -o "$OBJ/t_raster" 2>/dev/null \
     && "$OBJ/t_raster" examples/assets/models/monkey.obj "$OBJ/raster" | grep -q "geometry preserved after weld+reorder: YES"; then echo "  PASS  soft_raster_test"; pass=$((pass+1)); else echo "  FAIL  soft_raster_test"; fail=$((fail+1)); fi
  echo "== Lua tests"
  if python3 test/lua/check_lua51.py >/dev/null; then echo "  PASS  lua 5.1 compatibility"; pass=$((pass+1)); else echo "  FAIL  lua 5.1 compatibility"; fail=$((fail+1)); fi
  for t in test_prelude smoke_examples; do
    if python3 test/lua/run_lua.py test/lua/$t.lua >/dev/null 2>&1; then echo "  PASS  $t"; pass=$((pass+1)); else echo "  FAIL  $t"; fail=$((fail+1)); fi
  done
  python3 tools/embed_lua.py >/dev/null && git diff --quiet src/scripting/lua_prelude_data.cpp 2>/dev/null || true
fi
echo; echo "passed: $pass   failed: $fail"
[[ $fail -eq 0 ]]
