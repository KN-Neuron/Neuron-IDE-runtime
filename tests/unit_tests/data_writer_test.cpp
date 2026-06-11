#include <gtest/gtest.h>

#include <EEGData.hpp>
#include <chrono>
#include <datawriter/DataWriter.hpp>
#include <datawriter/Marker.hpp>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {
namespace fs = std::filesystem;

constexpr float  kChannelOne      = 1.25F;
constexpr float  kChannelTwo      = 2.5F;
constexpr float  kChannelThree    = 3.75F;
constexpr double kEegTimestamp    = 12.5;
constexpr double kMarkerTimestamp = 13.25;
constexpr auto   kWriteWait       = std::chrono::milliseconds(50);

fs::path makeTempFilePath(const std::string& nameStem) {
    const auto uniqueSuffix =
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    return fs::temp_directory_path() / (nameStem + "_" + uniqueSuffix + ".csv");
}

std::vector<std::string> readAllLines(const fs::path& filePath) {
    std::ifstream            input(filePath);
    std::vector<std::string> lines;
    std::string              line;

    while (std::getline(input, line)) {
        lines.push_back(line);
    }

    return lines;
}
}  // namespace

TEST(DataWriterTest, WritesCsvHeaderOnStart) {
    const auto filePath = makeTempFilePath("datawriter_header");

    auto eegQueue    = std::make_shared<moodycamel::ConcurrentQueue<EEGData>>();
    auto markerQueue = std::make_shared<moodycamel::ConcurrentQueue<Marker>>();

    {
        DataWriter writer;
        writer.start(filePath.string(), eegQueue, markerQueue);
        writer.stop();
    }

    const auto lines = readAllLines(filePath);
    ASSERT_FALSE(lines.empty());
    EXPECT_EQ(lines.front(), "type,timestamp,payload");

    fs::remove(filePath);
}

TEST(DataWriterTest, FlushesEegAndMarkerRecords) {
    const auto filePath = makeTempFilePath("datawriter_records");

    auto eegQueue    = std::make_shared<moodycamel::ConcurrentQueue<EEGData>>();
    auto markerQueue = std::make_shared<moodycamel::ConcurrentQueue<Marker>>();

    eegQueue->enqueue(EEGData{{kChannelOne, kChannelTwo, kChannelThree}, kEegTimestamp});
    markerQueue->enqueue(Marker{"stimulus_on", kMarkerTimestamp});

    {
        DataWriter writer;
        writer.start(filePath.string(), eegQueue, markerQueue);
        std::this_thread::sleep_for(kWriteWait);
        writer.stop();
    }

    const auto lines = readAllLines(filePath);
    ASSERT_GE(lines.size(), 3U);
    EXPECT_EQ(lines[0], "type,timestamp,payload");
    EXPECT_EQ(lines[1], "eeg,12.5,\"1.25,2.5,3.75\"");
    EXPECT_EQ(lines[2], "marker,13.25,\"stimulus_on\"");

    fs::remove(filePath);
}
