#!/usr/bin/env bash
# Runs one of the examples with a device config. Close the window to stop.
#
#   examples/run.sh <example> [device config]
#   examples/run.sh p300
#   EEG=device examples/run.sh reaction-time examples/devices/ba-midi-072.json
#
# The device config defaults to examples/devices/ba-midi-072.json. Unless EEG=device, a fake EEG
# stream shaped like that config (build/tools/fake_eeg) runs alongside, so no device is needed.
# With EEG=device the runtime records from the real stream, which must already be on LSL.
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
runtime="$repo_dir/build/src/NeuronIDE"
fake_eeg="$repo_dir/build/tools/fake_eeg"

if [[ $# -lt 1 || $# -gt 2 || ! -f "$repo_dir/examples/${1:-}/experiment.pbtxt" ]]; then
    echo "Usage: examples/run.sh <example> [device config]" >&2
    echo "Examples:" $(cd "$repo_dir/examples" && ls -d */experiment.pbtxt | cut -d/ -f1) >&2
    exit 2
fi
example="$1"
example_dir="$repo_dir/examples/$example"
config="$(realpath "${2:-$repo_dir/examples/devices/ba-midi-072.json}")"
eeg="${EEG:-fake}"
run_dir="$repo_dir/build/example_$example"

for program in "$runtime" "$fake_eeg"; do
    if [[ ! -x "$program" ]]; then
        echo "Missing $program: build the runtime first" >&2
        exit 1
    fi
done
if [[ "$eeg" != fake && "$eeg" != device ]]; then
    echo "EEG must be 'fake' or 'device', not '$eeg'" >&2
    exit 2
fi

# Asset paths in the scene resolve against the folder holding the .neuroz file, so the scene, its
# script and images go together into the run folder.
mkdir -p "$run_dir"
find "$example_dir" -maxdepth 1 -type f ! -name '*.md' ! -name '*.pbtxt' \
    -exec cp {} "$run_dir/" \;
cp "$config" "$run_dir/config.json"
protoc --proto_path="$repo_dir/protoFiles" --encode=NeuronIDE.Scene \
    "$repo_dir/protoFiles/neuronide.proto" \
    < "$example_dir/experiment.pbtxt" > "$run_dir/experiment.neuroz"

if [[ "$eeg" == fake ]]; then
    "$fake_eeg" "$run_dir/config.json" &
    fake_eeg_pid=$!
    trap 'kill "$fake_eeg_pid" 2>/dev/null || true' EXIT
fi

# The recording is written to the working directory.
cd "$run_dir"
"$runtime" config.json experiment.neuroz
