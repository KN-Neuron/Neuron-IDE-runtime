#!/usr/bin/env bash
# The checks CI runs, one per workflow step. Needs the tools of the ci/Dockerfile image;
# ci/run-locally.sh runs these checks in it.
#
#   ci/checks.sh format            clang-format, fails on any change it would make
#   ci/checks.sh test              build, run the tests, fail under 90 % line coverage
#   ci/checks.sh tidy [<commit>]   clang-tidy on the files changed since <commit>, or on every file
#
# Builds into $BUILD_DIR (default: build).
set -euo pipefail
cd "$(dirname "$0")/.."

build=${BUILD_DIR:-build}

check_format() {
    find src include tests -type f \( -name "*.cpp" -o -name "*.hpp" -o -name "*.c" -o -name "*.h" \) \
        -print0 | xargs -0 clang-format-20 --dry-run -Werror
}

check_test() {
    cmake -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DNEURON_IDE_ENABLE_COVERAGE=ON \
        -DNEURON_IDE_ENABLE_CLANG_TIDY=OFF
    # A build directory reused across branches (ci/run-locally.sh) keeps what earlier builds left:
    # coverage notes of objects this branch doesn't build, which make gcovr fail, and execution
    # counts, which would add to this run's.
    local objects
    objects=$(ninja -C "$build" -t targets all | sed -n 's/\.o: .*/.o/p')
    find "$build" -name '*.gcno' | while read -r notes; do
        object=${notes#"$build"/}
        grep -qxF "${object%.gcno}.o" <<< "$objects" || rm "$notes"
    done
    find "$build" -name '*.gcda' -delete
    cmake --build "$build" --parallel
    cmake --build "$build" --target coverage
}

check_tidy() {
    local base=${1:-}
    cmake -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        -DNEURON_IDE_ENABLE_CLANG_TIDY=OFF
    cmake --build "$build" --target neuronide_proto

    local files
    if [[ -n $base ]] && git diff --quiet "$base" -- .clang-tidy; then
        local patterns=('src/*.cpp' 'src/*.hpp' 'include/*.hpp' 'tests/*.cpp' 'tests/*.hpp')
        echo "clang-tidy: files changed since $base"
        # Untracked files exist only in a local working tree, never in CI's checkout.
        files=$(git diff --name-only --diff-filter=d "$base" -- "${patterns[@]}"
                git ls-files --others --exclude-standard -- "${patterns[@]}")
    else
        echo "clang-tidy: every file"
        files=$(git ls-files 'src/*.cpp' 'tests/*.cpp')
    fi
    if [[ -z $files ]]; then
        echo "No C++ files changed."
        return 0
    fi
    echo "$files" | xargs -P "$(nproc)" -n 1 clang-tidy-20 -p "$build" --quiet
}

case ${1:-} in
    format) check_format ;;
    test) check_test ;;
    tidy) check_tidy "${2:-}" ;;
    *)
        echo "Usage: ci/checks.sh format | test | tidy [<commit>]" >&2
        exit 2
        ;;
esac
