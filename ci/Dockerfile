# Every CI job runs in this image. CI publishes it as
# ghcr.io/kn-neuron/neuron-ide-runtime-ci:<git hash-object ci/Dockerfile>, so editing this file is
# what produces a new image; until then every run, and ci/run-locally.sh, gets the same compiler,
# SDL, protobuf, clang-format and clang-tidy.
FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates git \
        g++ cmake ninja-build \
        libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev \
        libprotobuf-dev protobuf-compiler \
        clang-format-20 clang-tidy-20 gcovr \
    && rm -rf /var/lib/apt/lists/*

# The mounted checkout belongs to another user than the container's (the CI runner's, or yours),
# and git refuses to work in it without this. CI's clang-tidy job runs git diff.
RUN git config --system --add safe.directory '*'

LABEL org.opencontainers.image.source=https://github.com/KN-Neuron/Neuron-IDE-runtime
