#include <Runtime.hpp>
#include <cstddef>
#include <exception>
#include <iostream>
#include <span>
#include <string>

namespace {
constexpr const char* kDefaultConfigPath = "config.json";

void printUsage(const char* program) {
    std::cerr << "Usage: " << program << " [config.json] <experiment.neuroz>\n"
              << "  config.json defaults to '" << kDefaultConfigPath
              << "' in the working directory.\n";
}
}  // namespace

int main(int argc, char* argv[]) {
    const std::span<char* const> args(argv, static_cast<std::size_t>(argc));

    RuntimePaths paths{.config = kDefaultConfigPath, .experiment = ""};

    if (args.size() == 2) {
        paths.experiment = args[1];
    } else if (args.size() >= 3) {
        paths.config     = args[1];
        paths.experiment = args[2];
    } else {
        printUsage(args.empty() ? "NeuronIDE" : args[0]);
        return 1;
    }

    try {
        Runtime runtime(paths);
        runtime.run();
    } catch (const std::exception& e) {
        std::cerr << "NeuronIDE: fatal: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
