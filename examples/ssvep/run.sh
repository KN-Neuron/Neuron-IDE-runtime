#!/usr/bin/env bash
# Runs the SSVEP example. Close the window to stop.
#
# By default it starts the fake EEG stream ../lsl_testing/outlet (next to this repo). Set
# EEG_OUTLET to another program that streams what config.json expects, or to "none" when the
# stream is already running (a real amplifier, for instance).
set -euo pipefail

example_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_dir="$(cd "$example_dir/../.." && pwd)"
runtime="$repo_dir/build/src/NeuronIDE"
outlet="${EEG_OUTLET:-$repo_dir/../lsl_testing/outlet}"
run_dir="$repo_dir/build/example_ssvep"

if [[ ! -x "$runtime" ]]; then
    echo "Missing $runtime: build the runtime first" >&2
    exit 1
fi
if [[ "$outlet" != "none" && ! -x "$outlet" ]]; then
    echo "Missing $outlet: set EEG_OUTLET to a fake EEG stream, or to 'none'" >&2
    exit 1
fi

# Asset paths in the scene resolve against the folder holding the .neuroz file.
mkdir -p "$run_dir"
cp "$example_dir"/{config.json,ssvep.py,target.png,arrow.png} "$run_dir/"
protoc --proto_path="$repo_dir/protoFiles" --encode=NeuronIDE.Scene \
    "$repo_dir/protoFiles/neuronide.proto" \
    < "$example_dir/experiment.pbtxt" > "$run_dir/experiment.neuroz"

if [[ "$outlet" != "none" ]]; then
    "$outlet" &
    outlet_pid=$!
    trap 'kill "$outlet_pid" 2>/dev/null || true' EXIT
fi

# The recording is written to the working directory.
cd "$run_dir"
"$runtime" config.json experiment.neuroz
