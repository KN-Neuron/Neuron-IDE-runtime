#ifndef LOGGING_HPP
#define LOGGING_HPP

#include <quill/LogMacros.h>
#include <quill/Logger.h>

#include <cstdint>
#include <optional>
#include <string_view>

namespace logging {

enum class Subsystem : std::uint8_t { App, Runtime, LslReader, Renderer, Scene };

// Safe to call from any thread. Messages reach stderr only while a Session is
// running; without one they are queued and never printed.
quill::Logger* logger(Subsystem subsystem);

// Parses a NEURONIDE_LOG_LEVEL value such as "debug" or "WARNING". Returns
// nullopt for anything a logger cannot be set to.
std::optional<quill::LogLevel> parseLogLevel(std::string_view name);

// Runs the logging backend thread; the destructor writes out every queued
// message. Create it first in main() so it outlives every thread that logs.
// Keep one at a time: the backend is shared, so destroying any Session stops it.
class Session {
   public:
    Session();
    ~Session();

    Session(const Session&)            = delete;
    Session& operator=(const Session&) = delete;
    Session(Session&&)                 = delete;
    Session& operator=(Session&&)      = delete;
};

}  // namespace logging

#endif  // LOGGING_HPP
