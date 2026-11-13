#include <gtest/gtest.h>
#include <pybind11/embed.h>

#include <cstdint>
#include <logging/Logging.hpp>
#include <sstream>
#include <string>
#include <string_view>

#include "scripting/PythonInterpreter.hpp"

// The test process runs its own interpreter (TestMain), so every PythonInterpreter here shares it.

namespace {
namespace py = pybind11;

// What the [script] logger writes to stderr while `action` runs.
template <typename Action>
std::string scriptLogOf(const Action& action) {
    testing::internal::CaptureStderr();
    action();
    logging::logger(logging::Subsystem::Script)->flush_log();
    return testing::internal::GetCapturedStderr();
}

// The level of the [script] line that says exactly `message`, or empty when there is none.
std::string levelOf(const std::string& log, std::string_view message) {
    constexpr std::string_view kTag = "[script] ";
    std::istringstream         lines(log);
    for (std::string line; std::getline(lines, line);) {
        const auto tag = line.find(kTag);
        if (tag != std::string::npos && line.substr(tag + kTag.size()) == message) {
            std::istringstream fields(line.substr(0, tag));
            std::string        date;
            std::string        time;
            std::string        level;
            fields >> date >> time >> level;
            return level;
        }
    }
    return "";
}
}  // namespace

TEST(PythonInterpreterTest, SeedsRandomWithTheSeedItReports) {
    const PythonInterpreter python;
    const py::module_       random = py::module_::import("random");
    const auto              first  = random.attr("random")().cast<double>();

    random.attr("seed")(python.randomSeed());

    EXPECT_EQ(random.attr("random")().cast<double>(), first);
}

TEST(PythonInterpreterTest, DrawsAFreshSeedEveryTime) {
    const PythonInterpreter first;
    const PythonInterpreter second;

    EXPECT_NE(first.randomSeed(), second.randomSeed());
}

TEST(PythonInterpreterTest, LogsTheSeed) {
    std::uint64_t     seed = 0;
    const std::string log  = scriptLogOf([&seed] {
        const PythonInterpreter python;
        seed = python.randomSeed();
    });

    EXPECT_NE(log.find("random seeded with " + std::to_string(seed) + "\n"), std::string::npos)
        << log;
}

TEST(PythonInterpreterTest, LeavesAnInterpreterItDidNotStartRunning) {
    {
        const PythonInterpreter nested;
    }

    EXPECT_NE(Py_IsInitialized(), 0);
}

TEST(PythonInterpreterTest, LogsEveryPrintedLineAsInfo) {
    const std::string log = scriptLogOf([] { py::exec("print('first\\nsecond')"); });

    EXPECT_EQ(levelOf(log, "first"), "INFO") << log;
    EXPECT_EQ(levelOf(log, "second"), "INFO") << log;
}

TEST(PythonInterpreterTest, LogsStderrAsWarnings) {
    const std::string log =
        scriptLogOf([] { py::exec("import sys\nprint('odd', file=sys.stderr)"); });

    EXPECT_EQ(levelOf(log, "odd"), "WARNING") << log;
}

TEST(PythonInterpreterTest, LogsALineOnlyOnceItIsComplete) {
    const std::string log = scriptLogOf(
        [] { py::exec("import sys\nsys.stdout.write('half a')\nsys.stdout.write(' line\\n')"); });

    EXPECT_EQ(levelOf(log, "half a"), "") << log;
    EXPECT_EQ(levelOf(log, "half a line"), "INFO") << log;
}

TEST(PythonInterpreterTest, FlushLogsAnUnfinishedLine) {
    const std::string log = scriptLogOf(
        [] { py::exec("import sys\nsys.stdout.write('no newline')\nsys.stdout.flush()"); });

    EXPECT_EQ(levelOf(log, "no newline"), "INFO") << log;
}

// Python's text streams count characters, not the bytes of their UTF-8 encoding.
TEST(PythonInterpreterTest, WriteReturnsTheNumberOfCharacters) {
    const py::module_ sys = py::module_::import("sys");

    EXPECT_EQ(sys.attr("stdout").attr("write")("\xC5\xBC\xC3\xB3\xC5\x82w\n").cast<int>(), 5);
}
