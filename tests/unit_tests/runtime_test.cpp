#include <SDL2/SDL.h>
#include <gtest/gtest.h>

#include <Runtime.hpp>
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "lsl_cpp.h"
#include "utils/ParserTestUtils.hpp"

// These tests drive a real LSLReader against a local LSL outlet over loopback,
// like the LSLReader tests do.

namespace {
namespace fs = std::filesystem;

constexpr int    kSurfaceSize    = 10;
constexpr int    kSurfaceDepth   = 32;
constexpr int    kStreamChannels = 1;
constexpr int    kWrongChannels  = 2;
constexpr double kSampleRateHz   = 250.0;
constexpr double kSampleValue    = 7.5;

constexpr auto kSamplePeriod = std::chrono::milliseconds(4);  // 250 Hz
// Generous: resolving a stream over loopback usually takes well under a second.
constexpr auto kAcquisitionTimeout = std::chrono::seconds(10);
constexpr auto kNoStreamTimeout    = std::chrono::milliseconds(500);
// Longer than kAcquisitionTimeout, so a recording that does start is always seen.
constexpr auto kRecordingWait      = std::chrono::seconds(15);
constexpr auto kRecordingPoll      = std::chrono::milliseconds(10);
constexpr auto kExperimentDuration = std::chrono::milliseconds(300);
// Upper bound for a lost stream to be noticed, re-resolved and rejected.
constexpr auto kFailureWait = std::chrono::seconds(15);

// The stream name is unique per test, so tests running in parallel processes
// never resolve each other's outlets.
std::string currentStreamName() {
    const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
    return std::string("runtime_test_") + info->name();
}

struct ConfigOptions {
    std::string display        = R"json({ "fullscreen": false })json";
    std::string outputFormat   = "csv";
    bool        channelEnabled = true;
};

std::string makeConfigJson(const std::string& streamName, const ConfigOptions& options = {}) {
    return R"json({
      "config_version": "1.1",
      "device_name": "Dev",
      "montage_standard": "10-20",
      "lsl_stream": {
        "name": ")json" +
           streamName + R"json(", "type": "EEG", "source_id": "runtime-test-src",
        "expected_channel_count": 1, "expected_sample_rate_hz": 250
      },
      "channels": [ { "index": 0, "label": "Fz", "enabled": )json" +
           (options.channelEnabled ? "true" : "false") + R"json(, "unit": "uV" } ],
      "display": )json" +
           options.display + R"json(,
      "output": { "format": ")json" +
           options.outputFormat + R"json(" }
    })json";
}

std::shared_ptr<SDL_Renderer> createSoftwareRenderer() {
    SDL_Surface*  surface  = SDL_CreateRGBSurfaceWithFormat(0, kSurfaceSize, kSurfaceSize,
                                                            kSurfaceDepth, SDL_PIXELFORMAT_RGBA32);
    SDL_Renderer* renderer = SDL_CreateSoftwareRenderer(surface);
    return {renderer, [surface](SDL_Renderer* target) {
                if (target != nullptr) {
                    SDL_DestroyRenderer(target);
                }
                if (surface != nullptr) {
                    SDL_FreeSurface(surface);
                }
            }};
}

// A software renderer backed by an in-memory surface: no window, GPU, or
// display required, so it runs anywhere (including headless CI). Injected in
// place of Runtime's production SDL window/vsync-renderer factory.
Runtime::RenderTargetFactory softwareRenderTargetFactory() {
    return [](const std::string&, const DisplayConfig&) { return createSoftwareRenderer(); };
}

// Records whether Runtime asked for a render target at all.
Runtime::RenderTargetFactory windowSpy(bool& windowOpened) {
    return [&windowOpened](const std::string&, const DisplayConfig&) {
        windowOpened = true;
        return createSoftwareRenderer();
    };
}

// Stands in for the EEG headset: an LSL outlet streaming a constant sample at
// the configured rate until it is destroyed.
class FakeCap {
   public:
    FakeCap(const std::string& streamName, int channelCount)
        : outlet(lsl::stream_info(streamName, "EEG", channelCount, kSampleRateHz, lsl::cf_double64,
                                  streamName + "-src")),
          pusher([this, channelCount](const std::stop_token& stopToken) {
              const std::vector<double> sample(static_cast<std::size_t>(channelCount),
                                               kSampleValue);
              while (!stopToken.stop_requested()) {
                  outlet.push_sample(sample);
                  std::this_thread::sleep_for(kSamplePeriod);
              }
          }) {}
    ~FakeCap() = default;

    FakeCap(const FakeCap&)            = delete;
    FakeCap& operator=(const FakeCap&) = delete;
    FakeCap(FakeCap&&)                 = delete;
    FakeCap& operator=(FakeCap&&)      = delete;

   private:
    lsl::stream_outlet outlet;
    std::jthread       pusher;  // last: stops pushing before the outlet goes away
};

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

std::vector<fs::path> recordingsIn(const fs::path& dir) {
    std::vector<fs::path> recordings;
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (entry.path().extension() == ".csv") {
            recordings.push_back(entry.path());
        }
    }
    return recordings;
}

std::ptrdiff_t countEegRows(const fs::path& recording) {
    const auto lines = readAllLines(recording);
    return std::count_if(lines.begin(), lines.end(),
                         [](const std::string& line) { return line.rfind("eeg,", 0) == 0; });
}

// Sleeps for `duration`, waking early when `stopToken` fires.
void sleepUnlessStopped(std::chrono::milliseconds duration, const std::stop_token& stopToken) {
    std::mutex                  mutex;
    std::condition_variable_any wakeUp;
    std::unique_lock            lock(mutex);
    wakeUp.wait_for(lock, stopToken, duration, [] { return false; });
}

using HelperAction = std::function<void(const std::stop_token&)>;

// Runs `action` on a helper thread once `count` recording files exist in `dir`
// - i.e. once run() is past the acquisition wait and rendering - while run()
// itself stays on the test's main thread, as SDL requires. After
// kRecordingWait the action runs anyway, so a broken run() cannot hang a test;
// destroying the returned thread before then skips the action.
std::jthread onceRecording(const fs::path& dir, HelperAction action, std::size_t count = 1) {
    return std::jthread([dir, count, action = std::move(action)](const std::stop_token& stopToken) {
        const auto deadline = std::chrono::steady_clock::now() + kRecordingWait;
        while (recordingsIn(dir).size() < count && std::chrono::steady_clock::now() < deadline) {
            if (stopToken.stop_requested()) {
                return;
            }
            std::this_thread::sleep_for(kRecordingPoll);
        }
        action(stopToken);
    });
}

void pushQuitEvent() {
    SDL_Event quit{};
    quit.type = SDL_QUIT;
    SDL_PushEvent(&quit);
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

        streamName = currentStreamName();
        writeText(tempDir / "config.json", makeConfigJson(streamName));
        writeExperimentFile(tempDir / "experiment.pb", utils::buildSimpleScene());
    }

    void TearDown() override {
        std::error_code errorCode;
        fs::remove_all(tempDir, errorCode);
    }

    const fs::path&    dir() const { return tempDir; }
    const std::string& stream() const { return streamName; }

    RuntimePaths paths() const {
        return RuntimePaths{.config     = (tempDir / "config.json").string(),
                            .experiment = (tempDir / "experiment.pb").string(),
                            .outputDir  = tempDir.string()};
    }

    std::unique_ptr<Runtime> makeRuntime(
        std::chrono::milliseconds acquisitionTimeout = kAcquisitionTimeout) const {
        return std::make_unique<Runtime>(paths(), softwareRenderTargetFactory(),
                                         acquisitionTimeout);
    }

    // Records for kExperimentDuration, then stops the runtime from another
    // thread. `recordingNumber` says which recording in dir() to wait for.
    void runBriefly(Runtime& runtime, std::size_t recordingNumber = 1) const {
        const std::jthread stopper = onceRecording(
            dir(),
            [&runtime](const std::stop_token& stopToken) {
                sleepUnlessStopped(kExperimentDuration, stopToken);
                runtime.requestStop();
            },
            recordingNumber);
        runtime.run();
    }

   private:
    fs::path    tempDir;
    std::string streamName;
};
}  // namespace

// --- Construction ----------------------------------------------------------

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

TEST_F(RuntimeFixture, MissingOutputDirectoryThrows) {
    RuntimePaths badPaths = paths();
    badPaths.outputDir    = (dir() / "no_such_dir").string();

    EXPECT_THROW(Runtime(badPaths, softwareRenderTargetFactory()), std::invalid_argument);
}

TEST_F(RuntimeFixture, UnknownOutputFormatThrows) {
    writeText(dir() / "config.json", makeConfigJson(stream(), {.outputFormat = "parquet"}));

    EXPECT_THROW(Runtime(paths(), softwareRenderTargetFactory()), std::invalid_argument);
}

TEST_F(RuntimeFixture, NullRenderTargetFactoryThrows) {
    EXPECT_THROW(Runtime(paths(), Runtime::RenderTargetFactory{}), std::invalid_argument);
}

TEST_F(RuntimeFixture, FactoryReturningNullRendererThrows) {
    const auto nullFactory = [](const std::string&,
                                const DisplayConfig&) -> std::shared_ptr<SDL_Renderer> {
        return nullptr;
    };
    EXPECT_THROW(Runtime(paths(), nullFactory), std::runtime_error);
}

TEST_F(RuntimeFixture, NonPositiveAcquisitionTimeoutThrows) {
    EXPECT_THROW(Runtime(paths(), softwareRenderTargetFactory(), std::chrono::milliseconds(0)),
                 std::invalid_argument);
}

TEST_F(RuntimeFixture, ConfigErrorsAreReportedBeforeTheWindowOpens) {
    // A valid config, but LSLReader rejects a cap with every channel disabled.
    writeText(dir() / "config.json", makeConfigJson(stream(), {.channelEnabled = false}));

    bool windowOpened = false;
    EXPECT_THROW(Runtime(paths(), windowSpy(windowOpened)), std::invalid_argument);
    EXPECT_FALSE(windowOpened);
}

TEST_F(RuntimeFixture, PassesTheConfiguredDisplayToTheRenderTargetFactory) {
    writeText(dir() / "config.json",
              makeConfigJson(stream(), {.display = R"json({ "index": 0, "width": 800 })json"}));

    DisplayConfig received;
    const auto    factory = [&received](const std::string&, const DisplayConfig& display) {
        received = display;
        return createSoftwareRenderer();
    };
    const Runtime runtime(paths(), factory);

    constexpr int kConfiguredWidth = 800;
    EXPECT_EQ(received.width, kConfiguredWidth);
}

// --- Acquisition gate ------------------------------------------------------

TEST_F(RuntimeFixture, RunThrowsWithoutRecordingWhenNoStreamAppears) {
    auto runtime = makeRuntime(kNoStreamTimeout);

    EXPECT_THROW(runtime->run(), std::runtime_error);
    EXPECT_TRUE(runtime->outputPath().empty());
    EXPECT_TRUE(recordingsIn(dir()).empty()) << "no stimulus may run without EEG";
}

TEST_F(RuntimeFixture, RunThrowsWithoutRecordingWhenStreamShapeDoesNotMatchConfig) {
    const FakeCap cap(stream(), kWrongChannels);
    auto          runtime = makeRuntime();

    try {
        runtime->run();
        FAIL() << "a stream with the wrong channel count must not start the experiment";
    } catch (const std::runtime_error& e) {
        EXPECT_NE(std::string(e.what()).find("channels"), std::string::npos) << e.what();
    }
    EXPECT_TRUE(recordingsIn(dir()).empty());
}

TEST_F(RuntimeFixture, ClosingTheWindowWhileWaitingForTheStreamAbortsWithoutRecording) {
    auto runtime = makeRuntime();
    pushQuitEvent();

    EXPECT_NO_THROW(runtime->run());
    EXPECT_TRUE(runtime->outputPath().empty());
    EXPECT_TRUE(recordingsIn(dir()).empty());
}

TEST_F(RuntimeFixture, RequestStopBeforeRunAbortsWithoutRecording) {
    auto runtime = makeRuntime();
    runtime->requestStop();

    EXPECT_NO_THROW(runtime->run());
    EXPECT_TRUE(recordingsIn(dir()).empty());
}

TEST_F(RuntimeFixture, RunStopsTheExperimentWhenAcquisitionFailsMidway) {
    auto cap     = std::make_unique<FakeCap>(stream(), kStreamChannels);
    auto runtime = makeRuntime();

    // Once recording, swap the cap for one the config rejects: LSLReader loses
    // the stream, re-resolves it, fails validation and gives up for good.
    const std::jthread saboteur =
        onceRecording(dir(), [&cap, &runtime, this](const std::stop_token& stopToken) {
            cap.reset();
            cap = std::make_unique<FakeCap>(stream(), kWrongChannels);

            // Safety net only: the failure itself must end run().
            sleepUnlessStopped(kFailureWait, stopToken);
            runtime->requestStop();
        });

    try {
        runtime->run();
        FAIL() << "run() must report that acquisition failed during the experiment";
    } catch (const std::runtime_error& e) {
        EXPECT_NE(std::string(e.what()).find("during the experiment"), std::string::npos)
            << e.what();
    }

    const auto recordings = recordingsIn(dir());
    ASSERT_EQ(recordings.size(), 1U);
    EXPECT_GT(countEegRows(recordings.front()), 0) << "data before the failure must be kept";
}

// --- Recording -------------------------------------------------------------

TEST_F(RuntimeFixture, RecordsEegUntilStopIsRequestedFromAnotherThread) {
    const FakeCap cap(stream(), kStreamChannels);
    auto          runtime = makeRuntime();

    const auto started = std::chrono::steady_clock::now();
    runBriefly(*runtime);
    const auto elapsed = std::chrono::steady_clock::now() - started;

    EXPECT_GE(elapsed, kExperimentDuration) << "run() returned before stop was requested";

    const fs::path output = runtime->outputPath();
    ASSERT_TRUE(fs::exists(output));
    const auto lines = readAllLines(output);
    ASSERT_FALSE(lines.empty());
    EXPECT_EQ(lines.front(), "type,timestamp,payload");
    EXPECT_GT(countEegRows(output), 0) << "samples from the stream must reach the recording";
}

TEST_F(RuntimeFixture, RunStopsOnQuitEvent) {
    const FakeCap      cap(stream(), kStreamChannels);
    auto               runtime = makeRuntime();
    const std::jthread closer =
        onceRecording(dir(), [](const std::stop_token&) { pushQuitEvent(); });

    // Must return on SDL_QUIT even though no stop was requested.
    runtime->run();

    EXPECT_TRUE(fs::exists(runtime->outputPath()));
}

TEST_F(RuntimeFixture, OutputFilenameDerivedFromExperimentNameAndDir) {
    const FakeCap cap(stream(), kStreamChannels);
    auto          runtime = makeRuntime();
    runBriefly(*runtime);

    const fs::path output = runtime->outputPath();
    EXPECT_EQ(output.parent_path(), dir());
    // Scene project name is "TestProject" (see ParserTestUtils::buildSimpleScene).
    EXPECT_EQ(output.filename().string().rfind("TestProject_", 0), 0U);
    EXPECT_EQ(output.extension(), ".csv");
}

TEST_F(RuntimeFixture, OutputFilenameSanitizesTheExperimentName) {
    // An authored name is not a safe file name: unfiltered, "block 1/run" would
    // point at a subdirectory that does not exist.
    writeExperimentFile(dir() / "experiment.pb",
                        utils::buildSimpleScene({.projectName = "block 1/run"}));

    const FakeCap cap(stream(), kStreamChannels);
    auto          runtime = makeRuntime();
    runBriefly(*runtime);

    const fs::path output = runtime->outputPath();
    EXPECT_EQ(output.parent_path(), dir());
    EXPECT_EQ(output.filename().string().rfind("block_1_run_", 0), 0U);
    EXPECT_TRUE(fs::exists(output));
}

TEST_F(RuntimeFixture, SecondRunThrowsAndLeavesTheRecordingIntact) {
    const FakeCap cap(stream(), kStreamChannels);
    auto          runtime = makeRuntime();
    runBriefly(*runtime);

    const fs::path output       = runtime->outputPath();
    const auto     linesBefore  = readAllLines(output);
    const auto     recordedRows = countEegRows(output);
    ASSERT_GT(recordedRows, 0);

    EXPECT_THROW(runtime->run(), std::logic_error);
    EXPECT_EQ(readAllLines(output), linesBefore);
    EXPECT_EQ(recordingsIn(dir()).size(), 1U);
}

TEST_F(RuntimeFixture, ConsecutiveRuntimesWriteSeparateRecordings) {
    const FakeCap cap(stream(), kStreamChannels);

    auto first = makeRuntime();
    runBriefly(*first);
    first.reset();

    auto second = makeRuntime();
    runBriefly(*second, 2);

    const auto recordings = recordingsIn(dir());
    ASSERT_EQ(recordings.size(), 2U);
    for (const auto& recording : recordings) {
        EXPECT_GT(countEegRows(recording), 0) << recording;
    }
}

// --- Threading -------------------------------------------------------------

TEST_F(RuntimeFixture, RunFromAnotherThreadThrows) {
    auto runtime = makeRuntime();

    bool rejected = false;
    std::jthread([&runtime, &rejected] {
        try {
            runtime->run();
        } catch (const std::logic_error&) {
            rejected = true;
        }
    }).join();

    EXPECT_TRUE(rejected) << "SDL events can only be handled on the thread that built the window";
}

// Covers the production SDL window + vsync renderer factory where the platform
// can provide one; skipped on headless machines (or without vsync).
TEST_F(RuntimeFixture, DefaultRenderTargetFactoryOnCapablePlatform) {
    // Let SDL pick the best available driver instead of the forced dummy one,
    // which cannot provide an accelerated renderer.
    unsetenv("SDL_VIDEODRIVER");

    std::unique_ptr<Runtime> runtime;
    try {
        runtime = std::make_unique<Runtime>(paths(), Runtime::defaultRenderTargetFactory(),
                                            kAcquisitionTimeout);
    } catch (const std::exception& e) {
        GTEST_SKIP() << "No vsynced accelerated render target on this platform: " << e.what();
    }

    const FakeCap      cap(stream(), kStreamChannels);
    const std::jthread closer =
        onceRecording(dir(), [](const std::stop_token&) { pushQuitEvent(); });
    runtime->run();

    EXPECT_TRUE(fs::exists(runtime->outputPath()));
}
