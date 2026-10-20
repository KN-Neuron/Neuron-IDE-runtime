#include <quill/Backend.h>
#include <quill/Frontend.h>
#include <quill/core/QuillError.h>
#include <quill/sinks/ConsoleSink.h>

#include <array>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdlib>
#include <logging/Logging.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace logging {
namespace {
constexpr const char*     kLevelVariable = "NEURONIDE_LOG_LEVEL";
constexpr quill::LogLevel kDefaultLevel  = quill::LogLevel::Info;

constexpr const char* kSinkName        = "stderr";
constexpr const char* kPattern         = "%(time) %(log_level:<8) [%(logger)] %(message)";
constexpr const char* kTimestampFormat = "%Y-%m-%d %H:%M:%S.%Qms";

// Quill polls every 100 us by default. A run logs a handful of lines, so the
// backend can sleep far longer instead of waking up next to the render thread.
constexpr auto kBackendSleep = std::chrono::milliseconds(5);

// logger() indexes its cache by enumerator value: list every one, in order.
constexpr std::array kSubsystems{Subsystem::App, Subsystem::Runtime, Subsystem::LslReader,
                                 Subsystem::Renderer, Subsystem::Scene};

const char* nameOf(Subsystem subsystem) {
    switch (subsystem) {
        case Subsystem::App:
            return "app";
        case Subsystem::Runtime:
            return "runtime";
        case Subsystem::LslReader:
            return "lslreader";
        case Subsystem::Renderer:
            return "renderer";
        case Subsystem::Scene:
            return "scene";
    }
    return "unknown";
}

struct LevelSetting {
    quill::LogLevel            level = kDefaultLevel;
    std::optional<std::string> rejectedValue;
};

const LevelSetting& levelSetting() {
    static const LevelSetting setting = [] {
        LevelSetting result;
        const char*  value = std::getenv(kLevelVariable);
        if (value == nullptr) {
            return result;
        }
        if (const std::optional<quill::LogLevel> level = parseLogLevel(value)) {
            result.level = *level;
        } else {
            result.rejectedValue = value;
        }
        return result;
    }();
    return setting;
}

quill::Logger* createLogger(Subsystem subsystem) {
    quill::ConsoleSinkConfig sinkConfig;
    sinkConfig.set_stream("stderr");
    auto sink = quill::Frontend::create_or_get_sink<quill::ConsoleSink>(kSinkName, sinkConfig);

    // Log times are read by people next to wall-clock file names; the default
    // TSC clock would only add calibration to get there.
    quill::Logger* created = quill::Frontend::create_or_get_logger(
        nameOf(subsystem), std::move(sink),
        quill::PatternFormatterOptions{kPattern, kTimestampFormat, quill::Timezone::LocalTime},
        quill::ClockSourceType::System);
    created->set_log_level(levelSetting().level);
    return created;
}
}  // namespace

quill::Logger* logger(Subsystem subsystem) {
    static const auto loggers = [] {
        std::array<quill::Logger*, kSubsystems.size()> created{};
        for (const Subsystem each : kSubsystems) {
            created.at(static_cast<std::size_t>(each)) = createLogger(each);
        }
        return created;
    }();
    return loggers.at(static_cast<std::size_t>(subsystem));
}

std::optional<quill::LogLevel> parseLogLevel(std::string_view name) {
    quill::LogLevel level{};
    try {
        level = quill::loglevel_from_string(std::string(name));
    } catch (const quill::QuillError&) {
        return std::nullopt;
    }
    // Quill reserves Backtrace for itself; set_log_level() throws on it.
    if (level == quill::LogLevel::Backtrace) {
        return std::nullopt;
    }
    return level;
}

Session::Session() {
    quill::BackendOptions backendOptions;
    backendOptions.sleep_duration = kBackendSleep;

    // Crash signals only. Quill would end the process on SIGINT/SIGTERM, while SDL
    // turns them into SDL_QUIT, so the run shuts down and closes its recording.
    quill::SignalHandlerOptions signalOptions;
    signalOptions.catchable_signals = {SIGABRT, SIGFPE, SIGILL, SIGSEGV};

    quill::Backend::start<quill::FrontendOptions>(backendOptions, signalOptions);

    if (const std::optional<std::string>& rejected = levelSetting().rejectedValue) {
        LOG_WARNING(logger(Subsystem::App), "Ignoring {}='{}': not a log level, using the default",
                    kLevelVariable, *rejected);
    }
}

// stop() throws only when called from the backend thread itself, which never
// runs this destructor.
// NOLINTNEXTLINE(bugprone-exception-escape)
Session::~Session() { quill::Backend::stop(); }

}  // namespace logging
