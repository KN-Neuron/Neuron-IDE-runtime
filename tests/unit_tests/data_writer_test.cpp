#include <gtest/gtest.h>

#include <chrono>
#include <data_structures/EEGData.hpp>
#include <data_structures/Marker.hpp>
#include <datawriter/CSVFormatStrategy.hpp>
#include <datawriter/DataWriter.hpp>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
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
        DataWriter writer(std::make_unique<CSVFormatStrategy>());
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

    eegQueue->enqueue(EEGData{kEegTimestamp, {kChannelOne, kChannelTwo, kChannelThree}});
    markerQueue->enqueue(Marker{"stimulus_on", kMarkerTimestamp});

    {
        DataWriter writer(std::make_unique<CSVFormatStrategy>());
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

TEST(CSVFormatStrategyTest, OpenRefusesToOverwriteAnExistingFile) {
    const auto filePath = makeTempFilePath("csv_existing");
    {
        std::ofstream existing(filePath);
        existing << "earlier recording\n";
    }

    CSVFormatStrategy strategy;
    EXPECT_THROW(strategy.open(filePath.string()), std::runtime_error);

    const auto lines = readAllLines(filePath);
    ASSERT_EQ(lines.size(), 1U);
    EXPECT_EQ(lines.front(), "earlier recording");

    fs::remove(filePath);
}

TEST(CSVFormatStrategyTest, WritesThroughTheCreatedFileEvenIfThePathIsReplaced) {
#ifdef _WIN32
    GTEST_SKIP() << "Windows does not allow renaming a file that is open";
#endif
    const auto filePath  = makeTempFilePath("csv_swapped");
    const auto movedPath = fs::path(filePath).replace_extension(".moved.csv");

    CSVFormatStrategy strategy;
    strategy.open(filePath.string());

    // Another process moves our file away and puts its own under the same name.
    fs::rename(filePath, movedPath);
    {
        std::ofstream other(filePath);
        other << "other recording\n";
    }

    strategy.writeHeader();
    strategy.close();

    const auto otherLines = readAllLines(filePath);
    ASSERT_EQ(otherLines.size(), 1U);
    EXPECT_EQ(otherLines.front(), "other recording") << "the other file must not be touched";

    const auto ownLines = readAllLines(movedPath);
    ASSERT_EQ(ownLines.size(), 1U);
    EXPECT_EQ(ownLines.front(), "type,timestamp,payload");

    fs::remove(filePath);
    fs::remove(movedPath);
}
