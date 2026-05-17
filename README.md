# Neuron-IDE-runtime

## Setup and Build Instructions

This project uses CMake and requires a modern C++17 compiler.

### 1. Clone repo and install required dependencies
This project uses GTest, LSL, SDL2, Clang-format, Clang-tidy, protobuf-compiler and cmake, but you don't need to install GTest and LSL because cmake will install it for you. Below is minimal linux setup.

```bash
git clone git@github.com:KN-Neuron/Neuron-IDE-runtime.git

sudo apt update
sudo apt install cmake clang-format clang-tidy libsdl2-dev protobuf-compiler
```

### 2. Build the Project
To configure and compile the project, open your terminal in the root directory and run:

```bash
# Generate the build system in the "build" directory
cmake -B build

# Compile the project
cmake --build build
```

Once built, the main executable will be available inside the `build` directory.

### 3. Running Tests
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

### 4. Code Formatting

This project includes a custom CMake target to automatically format all C++ source files using `clang-format`.

**To format your code:**
```bash
# Assuming you have already configured the build directory
cmake --build build --target format
```

This will run `clang-format -i` over all trackable source and header files, applying the style configuration specified in the project. 

### 5. Working with protobuf files

In case you want to create an example .pb file for testing or some other purpose, you have to first create a .pbtxt file (check out protoFiles/tests/test_scene.pbtxt for reference) which you then compile with this command:

```bash
protoc --encode=NeuronIDE.Scene protoFiles/neuronide.proto < protoFiles/tests/test_scene.pbtxt > protoFiles/tests/test_scene.pb
```