#ifndef TEMP_DIR_HPP
#define TEMP_DIR_HPP

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <string>
#include <system_error>

namespace utils {

// A fresh directory under the system temp folder, removed with everything in it on destruction.
// The name includes the running test, so tests in parallel processes never share one.
class TempDir {
   public:
    TempDir() {
        const auto*       info = ::testing::UnitTest::GetInstance()->current_test_info();
        const std::string testName =
            info != nullptr ? std::string(info->test_suite_name()) + "_" + info->name() : "none";
        root = std::filesystem::temp_directory_path() /
               ("neuronide_" + testName + "_" +
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(root);
    }

    ~TempDir() {
        std::error_code errorCode;
        std::filesystem::remove_all(root, errorCode);
    }

    TempDir(const TempDir&)            = delete;
    TempDir& operator=(const TempDir&) = delete;
    TempDir(TempDir&&)                 = delete;
    TempDir& operator=(TempDir&&)      = delete;

    [[nodiscard]] const std::filesystem::path& path() const { return root; }

   private:
    std::filesystem::path root;
};

}  // namespace utils

#endif  // TEMP_DIR_HPP
