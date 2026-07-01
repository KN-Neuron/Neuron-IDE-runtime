#include <SDL2/SDL.h>
#include <gtest/gtest.h>

#include <Runtime.hpp>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

#include "utils/ParserTestUtils.hpp"

namespace {
namespace fs = std::filesystem;

constexpr int  kSurfaceSize    = 10;
constexpr int  kSurfaceDepth   = 32;
constexpr auto kRenderSpinWait = std::chrono::milliseconds(50);

constexpr const char* kValidConfig = R"json({
  "config_version": "1.0",
  "device_name": "Dev",
  "montage_standard": "10-20",
  "lsl_stream": {
    "name": "runtime_test_stream", "type": "EEG", "source_id": "runtime-test-src",
    "expected_channel_count": 1, "expected_sample_rate_hz": 250
  },
  "channels": [ { "index": 0, "label": "Fz", "enabled": true, "unit": "uV" } ]
})json";

// A software renderer backed by an in-memory surface: no window, GPU, or
// display required, so it runs anywhere (including headless CI). Injected in
// place of Runtime's production SDL window/vsync-renderer factory.
Runtime::RenderTargetFactory softwareRenderTargetFactory() {
    return [](const std::string&) -> std::shared_ptr<SDL_Renderer> {
        SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(
            0, kSurfaceSize, kSurfaceSize, kSurfaceDepth, SDL_PIXELFORMAT_RGBA32);
        SDL_Renderer* renderer = SDL_CreateSoftwareRenderer(surface);
        return {renderer, [surface](SDL_Renderer* target) {
                    if (target != nullptr) {
                        SDL_DestroyRenderer(target);
                    }
                    if (surface != nullptr) {
                        SDL_FreeSurface(surface);
                    }
                }};
    };
}

void writeText(const fs::path& path, const std::string& content) {
    std::ofstream out(path, std::ios::binary);
    out << content;
}

void writeExperimentFile(const fs::path& path, const NeuronIDE::Scene& scene) {
    std::ofstream out(path, std::ios::binary);
    scene.SerializeToOstream(&out);
}

std::vector<std::string> readAllLines(const fs::path& path) {
    std::ifstream            input(path);
    std::vector<std::string> lines;
    std::string              line;
    while (std::getline(input, line)) {
        lines.push_back(line);
    }
    return lines;
}

// Owns the temp files that back a Runtime so a test never leaves artifacts on
// disk, and provides a valid config + experiment scene by default.
class RuntimeFixture : public ::testing::Test {
   protected:
    void SetUp() override {
        // SdlSession initializes SDL_INIT_VIDEO; the dummy driver makes that
        // succeed without a display. The software renderer is unaffected by it.
        setenv("SDL_VIDEODRIVER", "dummy", 1);

        tempDir = fs::temp_directory_path() /
                  ("neuronide_runtime_" +
                   std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directories(tempDir);

        writeText(tempDir / "config.json", kValidConfig);
        writeExperimentFile(tempDir / "experiment.pb", utils::buildSimpleScene());
    }

    void TearDown() override {
        std::error_code errorCode;
        fs::remove_all(tempDir, errorCode);
    }

    const fs::path& dir() const { return tempDir; }

    RuntimePaths paths() const {
        return RuntimePaths{.config     = (tempDir / "config.json").string(),
                            .experiment = (tempDir / "experiment.pb").string(),
                            .outputDir  = tempDir.string()};
    }

   private:
    fs::path tempDir;
};
}  // namespace

TEST_F(RuntimeFixture, ConstructsWithValidInputs) {
    EXPECT_NO_THROW({ const Runtime runtime(paths(), softwareRenderTargetFactory()); });
}

TEST_F(RuntimeFixture, OutputPathEmptyBeforeRun) {
    const Runtime runtime(paths(), softwareRenderTargetFactory());
    EXPECT_TRUE(runtime.outputPath().empty());
}

TEST_F(RuntimeFixture, MissingConfigThrows) {
    RuntimePaths badPaths = paths();
    badPaths.config       = (dir() / "does_not_exist.json").string();

    EXPECT_THROW(Runtime(badPaths, softwareRenderTargetFactory()), std::exception);
}

TEST_F(RuntimeFixture, MissingExperimentThrows) {
    RuntimePaths badPaths = paths();
    badPaths.experiment   = (dir() / "does_not_exist.pb").string();

    EXPECT_THROW(Runtime(badPaths, softwareRenderTargetFactory()), std::exception);
}

TEST_F(RuntimeFixture, UnknownOutputFormatThrows) {
    const std::string badFormatConfig = R"json({
      "config_version": "1.0", "device_name": "Dev", "montage_standard": "10-20",
      "lsl_stream": {
        "name": "runtime_test_stream", "type": "EEG", "source_id": "runtime-test-src",
        "expected_channel_count": 1, "expected_sample_rate_hz": 250
      },
      "channels": [ { "index": 0, "label": "Fz", "enabled": true, "unit": "uV" } ],
      "output": { "format": "parquet" }
    })json";
    writeText(dir() / "config.json", badFormatConfig);

    EXPECT_THROW(Runtime(paths(), softwareRenderTargetFactory()), std::invalid_argument);
}

TEST_F(RuntimeFixture, NullRenderTargetFactoryThrows) {
    EXPECT_THROW(Runtime(paths(), Runtime::RenderTargetFactory{}), std::invalid_argument);
}

TEST_F(RuntimeFixture, FactoryReturningNullRendererThrows) {
    const auto nullFactory = [](const std::string&) -> std::shared_ptr<SDL_Renderer> {
        return nullptr;
    };
    EXPECT_THROW(Runtime(paths(), nullFactory), std::runtime_error);
}

TEST_F(RuntimeFixture, OutputFilenameDerivedFromExperimentNameAndDir) {
    Runtime runtime(paths(), softwareRenderTargetFactory());

    // Stop the render loop immediately so run() returns after a single pass.
    runtime.requestStop();
    runtime.run();

    const fs::path output = runtime.outputPath();
    EXPECT_EQ(output.parent_path(), dir());
    // Scene project name is "TestProject" (see ParserTestUtils::buildSimpleScene).
    EXPECT_EQ(output.filename().string().rfind("TestProject_", 0), 0U);
    EXPECT_EQ(output.extension(), ".csv");
}

TEST_F(RuntimeFixture, RunWritesRecordingWithCsvHeader) {
    Runtime runtime(paths(), softwareRenderTargetFactory());

    runtime.requestStop();
    runtime.run();

    const fs::path output = runtime.outputPath();
    ASSERT_TRUE(fs::exists(output));

    const auto lines = readAllLines(output);
    ASSERT_FALSE(lines.empty());
    EXPECT_EQ(lines.front(), "type,timestamp,payload");
}

TEST_F(RuntimeFixture, RunStopsOnQuitEvent) {
    Runtime runtime(paths(), softwareRenderTargetFactory());

    SDL_Event quit;
    quit.type = SDL_QUIT;
    ASSERT_EQ(SDL_PushEvent(&quit), 1);

    // Must return promptly on SDL_QUIT even though no stop was requested.
    runtime.run();

    EXPECT_TRUE(fs::exists(runtime.outputPath()));
}

TEST_F(RuntimeFixture, RequestStopFromAnotherThreadEndsRun) {
    Runtime runtime(paths(), softwareRenderTargetFactory());

    // Ensure no stale SDL_QUIT from a previous test ends the loop early.
    SDL_FlushEvent(SDL_QUIT);

    std::thread worker([&runtime] { runtime.run(); });
    std::this_thread::sleep_for(kRenderSpinWait);
    runtime.requestStop();
    worker.join();

    EXPECT_TRUE(fs::exists(runtime.outputPath()));
}

// Covers the production SDL window + vsync renderer factory where the platform
// can provide an accelerated renderer; skipped on headless machines that can't.
TEST_F(RuntimeFixture, DefaultRenderTargetFactoryOnCapablePlatform) {
    // Let SDL pick the best available driver instead of the forced dummy one,
    // which cannot provide an accelerated renderer.
    unsetenv("SDL_VIDEODRIVER");

    try {
        Runtime runtime(paths(), Runtime::defaultRenderTargetFactory());

        SDL_Event quit;
        quit.type = SDL_QUIT;
        ASSERT_EQ(SDL_PushEvent(&quit), 1);
        runtime.run();

        EXPECT_TRUE(fs::exists(runtime.outputPath()));
    } catch (const std::exception& e) {
        GTEST_SKIP() << "No accelerated render target on this platform: " << e.what();
    }
}
