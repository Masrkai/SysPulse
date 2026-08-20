# SysPulse Justfile
# Requires: c++ (g++/clang++), curl, tar, gtest
# The Slint GUI toolchain is fetched automatically (no cmake/rust needed)

# Default recipe when running `just` without arguments
default:
    @just --list

# --- Variables ---
cxx := "c++"
base_flags := "-std=c++17 -Wall -Wextra -Iinclude"

# Directories
build_dir := "build"
external_libs_dir := "libs"

# Source files
main_srcs := "src/main.cpp src/ConsoleInitializer.cpp src/TimeManager.cpp src/CPUStressTest.cpp src/MemoryStressTest.cpp"
lib_srcs := "src/ConsoleInitializer.cpp src/TimeManager.cpp src/CPUStressTest.cpp src/MemoryStressTest.cpp"
test_srcs := "tests/test_cpustresstest.cpp tests/test_linkedlist.cpp tests/test_memorystresstest.cpp"


# --- Slint (fetched at gui build time if not present in external_libs_dir ) ---
# Prebuilt official C++ SDK from the Slint GitHub release. Contains the
# slint-compiler binary, headers and libslint_cpp.so, so no CMake/Rust is
# needed.
slint_version := "1.17.1" #! NOTE C++17 is a minimum here
slint_arch := "x86_64" # one of x86_64 / arm64 / armhf
slint_url := "https://github.com/slint-ui/slint/releases/download/v" + slint_version + "/Slint-cpp-" + slint_version + "-Linux-" + slint_arch + ".tar.gz"


#! NOTE: i did not test this nor do plan to but this is meant to be a valid path for nudging the users who does not have a repo that has gtest (which is weird as i saw it everywhere) but slint is newer so it is a different case 
# --- gtest (fetched at test build time if not present in external_libs_dir) ---
# Prebuilt official C++ SDK from the gtest GitHub release.
gtest_version := "1.18.0" #! NOTE C++17 is a minimum here
gtest_url := "https://github.com/google/googletest/releases/download/v" + gtest_version + "/googletest-" + gtest_version + ".tar.gz"


# --- Build Targets ---

# Build the main executable (Release mode)
build:
    #!/usr/bin/env bash
    mkdir -p {{build_dir}}
    {{cxx}} {{base_flags}} -O2 -DNDEBUG {{main_srcs}} -o {{build_dir}}/SysPulse -lpthread

# Run the main executable
run: build
    ./{{build_dir}}/SysPulse

# Build unit tests
test-build:
    #!/usr/bin/env bash
    set -euo pipefail
    mkdir -p {{build_dir}}

    gtest_dir="{{external_libs_dir}}/googletest-{{gtest_version}}"

    # 1. Check if gtest is available in system packages using which
    if which gtest >/dev/null 2>&1 || which gtest-config >/dev/null 2>&1 || pkg-config --exists gtest >/dev/null 2>&1; then
        echo "Found system gtest package."
        {{cxx}} {{base_flags}} {{lib_srcs}} {{test_srcs}} -o {{build_dir}}/SysPulse_tests -lgtest -lgtest_main -lpthread
    else
        echo "System gtest not found via which."
        mkdir -p {{external_libs_dir}}

        # 2. Check if gtest exists in libs; if not, download it
        if [ ! -d "${gtest_dir}" ]; then
            echo "Fetching GoogleTest {{gtest_version}} into libs ..."
            mkdir -p {{build_dir}}
            curl -fL -o "{{build_dir}}/googletest.tar.gz" "{{gtest_url}}"
            tar -xzf "{{build_dir}}/googletest.tar.gz" -C "{{external_libs_dir}}"
            rm -f "{{build_dir}}/googletest.tar.gz"
        else
            echo "GoogleTest already exists in libs. Continuing with compiling normally..."
        fi

        # Build local gtest if not built yet
        if [ ! -f "${gtest_dir}/build/lib/libgtest.a" ]; then
            echo "Building GoogleTest from libs..."
            cmake -S "${gtest_dir}" -B "${gtest_dir}/build" -DCMAKE_BUILD_TYPE=Release >/dev/null 2>&1
            cmake --build "${gtest_dir}/build" -j >/dev/null 2>&1
        fi

        {{cxx}} {{base_flags}} \
            -I"${gtest_dir}/googletest/include" \
            {{lib_srcs}} {{test_srcs}} \
            "${gtest_dir}/build/lib/libgtest.a" \
            "${gtest_dir}/build/lib/libgtest_main.a" \
            -o {{build_dir}}/SysPulse_tests -lpthread
    fi

# Run unit tests
test: test-build
    ./{{build_dir}}/SysPulse_tests

# Build the main executable (Profiling mode)
build-profiling:
    #!/usr/bin/env bash
    mkdir -p {{build_dir}}
    {{cxx}} {{base_flags}} -O2 -g3 -fno-omit-frame-pointer -fno-inline-small-functions -DPROFILING_BUILD {{main_srcs}} -o {{build_dir}}/SysPulse_profiling -lpthread

# Build the GUI executable
gui-build:
    #!/usr/bin/env bash
    set -euo pipefail

    slint_dir="{{external_libs_dir}}/slint/Slint-cpp-{{slint_version}}-Linux-{{slint_arch}}"
    mkdir -p {{build_dir}}

    # 1. Check if slint compiler is available in system packages using which
    if which slint-compiler >/dev/null 2>&1; then
        echo "Found system slint-compiler."
        slint_compiler="slint-compiler"

        # Compile ui/main.slint into a generated header
        "${slint_compiler}" ui/main.slint > "{{build_dir}}/main.h"

        # Build the GUI using system slint
        {{cxx}} -std=c++20 -Wall -Wextra -Iinclude -I{{build_dir}} \
            src/main_gui.cpp {{lib_srcs}} -o {{build_dir}}/syspulse_gui -lslint_cpp -lpthread
    else
        echo "System slint-compiler not found via which."
        mkdir -p {{external_libs_dir}}/slint

        # 2. Check if Slint SDK exists in libs; if not, download it
        if [ ! -d "${slint_dir}" ]; then
            echo "Fetching Slint {{slint_version}} prebuilt SDK into libs ..."
            curl -fL -o "{{external_libs_dir}}/slint/slint-cpp.tar.gz" "{{slint_url}}"
            tar -xzf "{{external_libs_dir}}/slint/slint-cpp.tar.gz" -C "{{external_libs_dir}}/slint"
        else
            echo "Slint SDK already exists in libs. Continuing with compiling normally..."
        fi

        slint_compiler="${slint_dir}/bin/slint-compiler"

        # Compile ui/main.slint into a generated header with the bundled compiler
        LD_LIBRARY_PATH="${slint_dir}/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
            "${slint_compiler}" ui/main.slint > "{{build_dir}}/main.h"

        # Build the GUI (Slint 1.17 requires C++20)
        {{cxx}} -std=c++20 -Wall -Wextra -Iinclude -I{{build_dir}} -I"${slint_dir}/include/slint" \
            -L"${slint_dir}/lib" -Wl,-rpath,"${slint_dir}/lib" \
            src/main_gui.cpp {{lib_srcs}} -o {{build_dir}}/syspulse_gui -lslint_cpp -lpthread
    fi

# Run the GUI
gui: gui-build
    #!/usr/bin/env bash
    set -euo pipefail
    slint_dir="{{external_libs_dir}}/slint/Slint-cpp-{{slint_version}}-Linux-{{slint_arch}}"
    if [ -d "${slint_dir}" ] && [ -d "${slint_dir}/lib" ]; then
        LD_LIBRARY_PATH="${slint_dir}/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" ./{{build_dir}}/syspulse_gui
    else
        ./{{build_dir}}/syspulse_gui
    fi

# --- Utilities ---

# Generate compile_commands.json for clangd
# Usage: bear -- just compile-commands
compile-commands:
    @echo "To generate compile_commands.json for clangd, run:"
    @echo "  bear -- just build"
    @echo "  bear -- just test"
    @echo "  bear -- just gui"

# Clean all build artifacts
clean:
    rm -rf {{build_dir}} {{build_dir}} {{build_dir}} compile_commands.json

# Format code (optional, if you use clang-format)
format:
    find src include tests -name '*.cpp' -o -name '*.hpp' | xargs clang-format -i