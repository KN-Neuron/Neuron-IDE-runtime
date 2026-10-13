#include <gtest/gtest.h>

#include <logging/Logging.hpp>

int main(int argc, char** argv) {
    const logging::Session loggingSession;
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
