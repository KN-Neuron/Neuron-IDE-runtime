#include <gtest/gtest.h>

#include <logging/Logging.hpp>
#include <scripting/PythonInterpreter.hpp>

int main(int argc, char** argv) {
    const logging::Session loggingSession;
    // One for the whole process: Python cannot be reliably started again after it has shut down.
    const PythonInterpreter python;
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
