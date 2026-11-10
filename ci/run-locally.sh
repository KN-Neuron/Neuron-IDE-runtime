#!/usr/bin/env bash
# Runs CI's checks (ci/checks.sh) on your working tree, in the image CI runs them in.
#
#   ci/run-locally.sh               format, test and tidy
#   ci/run-locally.sh format tidy   only these
#
# Builds into build-ci/<check>, never your own build/. Like CI on a pull request, tidy checks only
# the files changed since the branch left origin/main.
set -euo pipefail
cd "$(dirname "$0")/.."

all=(format test tidy)
checks=("$@")
((${#checks[@]})) || checks=("${all[@]}")
for check in "${checks[@]}"; do
    if [[ " ${all[*]} " != *" $check "* ]]; then
        echo "Unknown check '$check'. Checks: ${all[*]}" >&2
        exit 2
    fi
done

tag=$(git hash-object ci/Dockerfile)
image=ghcr.io/kn-neuron/neuron-ide-runtime-ci:$tag
if ! docker image inspect "$image" > /dev/null 2>&1; then
    # Asking first keeps docker pull's bare "unauthorized" off the screen: ghcr.io answers that both
    # for a tag CI hasn't published and for a private package.
    if docker manifest inspect "$image" > /dev/null 2>&1; then
        docker pull "$image"
    else
        echo "Can't pull CI's image from ghcr.io: CI hasn't published this ci/Dockerfile yet, or the" \
            "package is private and you aren't logged in (docker login ghcr.io)." >&2
        image=neuronide-ci:$tag
        if docker image inspect "$image" > /dev/null 2>&1; then
            echo "Using $image, built here earlier from ci/Dockerfile." >&2
        else
            echo "Building $image from ci/Dockerfile." >&2
            docker build -t "$image" - < ci/Dockerfile
        fi
    fi
fi

base=$(git merge-base HEAD origin/main 2> /dev/null || true)

# With a terminal, Ctrl-C goes to the build inside the container instead of only to docker.
tty=()
if [[ -t 0 && -t 1 ]]; then
    tty=(-it)
fi

failed=()
for check in "${checks[@]}"; do
    echo "==> $check"
    status=0
    docker run --rm --init "${tty[@]}" --user "$(id -u):$(id -g)" -e HOME=/tmp \
        -e BUILD_DIR="build-ci/$check" -v "$PWD":/src -w /src "$image" \
        ci/checks.sh "$check" ${base:+"$base"} || status=$?
    if ((status == 130)); then
        exit 130
    fi
    if ((status != 0)); then
        failed+=("$check")
    fi
done

if ((${#failed[@]})); then
    echo "Failed: ${failed[*]}" >&2
    exit 1
fi
echo "Passed: ${checks[*]}"
