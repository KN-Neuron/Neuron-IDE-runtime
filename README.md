# Neuron-IDE-runtime

## Setup and Build Instructions

This project uses CMake and requires a modern C++17 compiler.

### 1. Build the Project
To configure and compile the project, open your terminal in the root directory and run:

```bash
# Generate the build system in the "build" directory
cmake -B build

# Compile the project
cmake --build build
```

Once built, the main executable will be available inside the `build` directory.

### 2. Running Tests
This project uses Google Test (GTest) and CTest for testing. The tests are categorized by labels so you can run them selectively.

**To run all tests:**
```bash
cd build
ctest --output-on-failure
```

**To run only unit tests:**
```bash
cd build
ctest -L unit --output-on-failure
```

**To run only component tests:**
```bash
cd build
ctest -L component --output-on-failure
```