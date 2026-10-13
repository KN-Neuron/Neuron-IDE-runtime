#include <Runtime.hpp>
#include <cstddef>
#include <cstdio>
#include <exception>
#include <iostream>
#include <logging/Logging.hpp>
#include <span>
#include <string>

namespace {
constexpr const char* kDefaultConfigPath = "config.json";

quill::Logger* logger() { return logging::logger(logging::Subsystem::App); }

void printUsage(const char* program) {
    std::cerr << "Usage: " << program << " [config.json] <experiment.neuroz>\n"
              << "  config.json defaults to '" << kDefaultConfigPath
              << "' in the working directory.\n";
}

int runApplication(std::span<char* const> args) {
    const logging::Session loggingSession;

    RuntimePaths paths{.config = kDefaultConfigPath, .experiment = ""};

    if (args.size() == 2) {
        paths.experiment = args[1];
    } else if (args.size() == 3) {
        paths.config     = args[1];
        paths.experiment = args[2];
    } else {
        printUsage(args.empty() ? "NeuronIDE" : args[0]);
        return 1;
    }

    LOG_INFO(logger(), "Starting with config '{}' and experiment '{}'", paths.config,
             paths.experiment);

    try {
        Runtime runtime(paths);
        runtime.run();
    } catch (const std::exception& e) {
        LOG_CRITICAL(logger(), "Run aborted: {}", e.what());
        return 1;
    }

    return 0;
}
}  // namespace

int main(int argc, char* argv[]) {
    try {
        return runApplication(std::span<char* const>(argv, static_cast<std::size_t>(argc)));
    } catch (const std::exception& e) {
        // Logging itself failed, so plain stderr is all that is left.
        std::fputs("NeuronIDE: fatal: ", stderr);
        std::fputs(e.what(), stderr);
        std::fputs("\n", stderr);
    } catch (...) {
        std::fputs("NeuronIDE: fatal: unknown error\n", stderr);
    }
    return 1;
}
