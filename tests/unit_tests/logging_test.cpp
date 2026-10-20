#include <gtest/gtest.h>

#include <array>
#include <logging/Logging.hpp>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>

namespace {
using logging::Subsystem;

constexpr std::array<std::pair<Subsystem, std::string_view>, 5> kExpectedNames{{
    {Subsystem::App, "app"},
    {Subsystem::Runtime, "runtime"},
    {Subsystem::LslReader, "lslreader"},
    {Subsystem::Renderer, "renderer"},
    {Subsystem::Scene, "scene"},
}};
}  // namespace

TEST(LoggingTest, EverySubsystemHasItsOwnNamedLogger) {
    std::set<quill::Logger*> distinct;
    for (const auto& [subsystem, name] : kExpectedNames) {
        quill::Logger* logger = logging::logger(subsystem);
        ASSERT_NE(logger, nullptr);
        EXPECT_EQ(logger->get_logger_name(), name);
        distinct.insert(logger);
    }
    EXPECT_EQ(distinct.size(), kExpectedNames.size());
}

TEST(LoggingTest, ReturnsTheSameLoggerOnEveryCall) {
    EXPECT_EQ(logging::logger(Subsystem::Runtime), logging::logger(Subsystem::Runtime));
}

TEST(LoggingTest, ParsesLevelNamesIgnoringCase) {
    EXPECT_EQ(logging::parseLogLevel("debug"), quill::LogLevel::Debug);
    EXPECT_EQ(logging::parseLogLevel("Info"), quill::LogLevel::Info);
    EXPECT_EQ(logging::parseLogLevel("WARNING"), quill::LogLevel::Warning);
    EXPECT_EQ(logging::parseLogLevel("none"), quill::LogLevel::None);
}

TEST(LoggingTest, RejectsNamesThatAreNotSettableLevels) {
    EXPECT_EQ(logging::parseLogLevel(""), std::nullopt);
    EXPECT_EQ(logging::parseLogLevel("verbose"), std::nullopt);
    EXPECT_EQ(logging::parseLogLevel("backtrace"), std::nullopt)
        << "Quill throws when a logger is set to Backtrace";
}
