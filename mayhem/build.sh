#!/usr/bin/env bash
# mayhem/build.sh — build ncnn (sanitized + SanCov) and its fuzz targets, plus a normal-flags test build.
set -euo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${SRC:=/mayhem}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS
cd "$SRC"

OUT="$SRC/build-mayhem"           # gitignored by upstream's `build*/`
FZ="$SRC/build-fuzz"
TS="$SRC/build-tests"
mkdir -p "$OUT"

# SanCov on the library too, so file-input tool targets and harnesses are coverage-instrumented.
FFLAGS="$SANITIZER_FLAGS $DEBUG_FLAGS -fsanitize=fuzzer-no-link"
COMMON_CMAKE=(-DCMAKE_BUILD_TYPE=Release -DNCNN_VULKAN=OFF -DNCNN_OPENMP=OFF
  -DNCNN_RUNTIME_CPU=OFF -DNCNN_AVX=OFF -DNCNN_AVX2=OFF -DNCNN_AVX512=OFF -DNCNN_FMA=OFF -DNCNN_F16C=OFF
  -DNCNN_XOP=OFF -DNCNN_AVXVNNI=OFF -DNCNN_AVX512VNNI=OFF
  -DNCNN_BUILD_EXAMPLES=OFF -DNCNN_BUILD_BENCHMARK=OFF -DNCNN_PYTHON=OFF -DNCNN_SIMPLEOCV=ON)

# 0) leak-detection off hook (SPEC §6.2 item 15), linked into every sanitized executable
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c mayhem/lsan_off.cc -o "$OUT/lsan_off.o"
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$OUT/standalone_main.o"

# 1) sanitized libncnn + the file-input converter tools (ncnn2mem, ncnnoptimize, darknet2ncnn)
cmake -S . -B "$FZ" "${COMMON_CMAKE[@]}" -DNCNN_BUILD_TOOLS=ON -DNCNN_BUILD_TESTS=OFF \
  -DCMAKE_C_FLAGS="$FFLAGS" -DCMAKE_CXX_FLAGS="$FFLAGS" \
  -DCMAKE_EXE_LINKER_FLAGS="$OUT/lsan_off.o"
cmake --build "$FZ" -j"$MAYHEM_JOBS" --target ncnn ncnn2mem ncnnoptimize darknet2ncnn
for t in ncnn2mem ncnnoptimize darknet/darknet2ncnn; do
  cp "$FZ/tools/$t" "$OUT/"
done

# 2) libFuzzer harnesses + standalone reproducers (fuzz_ncnnmerge/fuzz_mxnet2ncnn #include the tool's
#    source with main renamed, so their .o needs SanCov too)
INC=(-I"$SRC/src" -I"$FZ/src" -I"$SRC/mayhem/harnesses")
for h in fuzz_net fuzz_net_bin fuzz_param fuzz_c_api fuzz_imdecode fuzz_pixel fuzz_ncnnmerge fuzz_mxnet2ncnn; do
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS -fsanitize=fuzzer-no-link -std=c++11 "${INC[@]}" -c "mayhem/harnesses/$h.cpp" -o "$OUT/$h.o"
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE "$OUT/$h.o" "$OUT/lsan_off.o" \
    "$FZ/src/libncnn.a" -lpthread -o "$OUT/$h"
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS -fsanitize=fuzzer-no-link "$OUT/$h.o" "$OUT/standalone_main.o" "$OUT/lsan_off.o" \
    "$FZ/src/libncnn.a" -lpthread -o "$OUT/$h-standalone"
done

# 2b) protobuf-based converters (onnx2ncnn, caffe2ncnn), same in-process #include scheme. protoc and
#     libprotobuf come from apt (mayhem/Dockerfile); the generated .pb.cc is dependency code.
PB="$OUT/proto"
mkdir -p "$PB"
protoc -I"$SRC/tools/onnx" --cpp_out="$PB" "$SRC/tools/onnx/onnx.proto"
protoc -I"$SRC/tools/caffe" --cpp_out="$PB" "$SRC/tools/caffe/caffe.proto"
for p in onnx caffe; do
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS -std=c++14 -I"$PB" -c "$PB/$p.pb.cc" -o "$PB/$p.pb.o"
  h=fuzz_${p}2ncnn
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS -fsanitize=fuzzer-no-link -std=c++14 -I"$PB" "${INC[@]}" -c "mayhem/harnesses/$h.cpp" -o "$OUT/$h.o"
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE "$OUT/$h.o" "$PB/$p.pb.o" "$OUT/lsan_off.o" \
    -lprotobuf -lpthread -o "$OUT/$h"
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS -fsanitize=fuzzer-no-link "$OUT/$h.o" "$PB/$p.pb.o" "$OUT/standalone_main.o" \
    "$OUT/lsan_off.o" -lprotobuf -lpthread -o "$OUT/$h-standalone"
done

# 3) test suite with the project's NORMAL flags (no sanitizers) for mayhem/test.sh
TESTS=(test_c_api test_datareader test_expression test_modelbin test_net test_paramdict test_squeezenet
  test_mat_pixel test_mat_pixel_resize test_mat_pixel_rotate test_mat_pixel_affine test_mat_pixel_drawing
  test_batchnorm test_reshape test_reshape_oom test_convolution test_pooling test_innerproduct test_softmax)
cmake -S . -B "$TS" "${COMMON_CMAKE[@]}" -DNCNN_BUILD_TOOLS=ON -DNCNN_BUILD_TESTS=ON \
  -DCMAKE_C_FLAGS="$COVERAGE_FLAGS" -DCMAKE_CXX_FLAGS="$COVERAGE_FLAGS"
cmake --build "$TS" -j"$MAYHEM_JOBS" --target ncnn ncnn2mem "${TESTS[@]}"
$CXX -O2 $COVERAGE_FLAGS -std=c++11 -I"$SRC/src" -I"$TS/src" mayhem/harnesses/kat_squeezenet.cpp \
  "$TS/src/libncnn.a" -lpthread -o "$TS/kat_squeezenet"
printf '%s\n' "${TESTS[@]}" > "$TS/mayhem-tests.txt"
