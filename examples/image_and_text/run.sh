#!/usr/bin/env bash
# Runs the example against the fake EEG stream from ../lsl_testing/outlet. Close the window to stop.
set -euo pipefail

example_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_dir="$(cd "$example_dir/../.." && pwd)"
runtime="$repo_dir/build/src/NeuronIDE"
outlet="$repo_dir/../lsl_testing/outlet"
run_dir="$repo_dir/build/example_image_and_text"

for program in "$runtime" "$outlet"; do
    if [[ ! -x "$program" ]]; then
        echo "Missing $program" >&2
        exit 1
    fi
done

mkdir -p "$run_dir"
cp "$example_dir/config.json" "$example_dir/stimulus.png" "$run_dir/"
protoc --proto_path="$repo_dir/protoFiles" --encode=NeuronIDE.Scene \
    "$repo_dir/protoFiles/neuronide.proto" \
    < "$example_dir/experiment.pbtxt" > "$run_dir/experiment.neuroz"

"$outlet" &
outlet_pid=$!
trap 'kill "$outlet_pid" 2>/dev/null || true' EXIT

# The recording is written to the working directory.
cd "$run_dir"
"$runtime" config.json experiment.neuroz
