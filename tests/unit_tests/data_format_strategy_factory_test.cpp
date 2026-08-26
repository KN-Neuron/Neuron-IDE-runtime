#include <gtest/gtest.h>

#include <datawriter/DataFormatStrategyFactory.hpp>
#include <memory>
#include <stdexcept>

TEST(DataFormatStrategyFactoryTest, CreatesCsvStrategy) {
    const auto strategy = DataFormatStrategyFactory::create("csv");
    ASSERT_NE(strategy, nullptr);
    EXPECT_EQ(strategy->fileExtension(), "csv");
}

TEST(DataFormatStrategyFactoryTest, FormatMatchingIsCaseInsensitive) {
    const auto strategy = DataFormatStrategyFactory::create("CSV");
    ASSERT_NE(strategy, nullptr);
    EXPECT_EQ(strategy->fileExtension(), "csv");
}

TEST(DataFormatStrategyFactoryTest, UnknownFormatThrows) {
    EXPECT_THROW(DataFormatStrategyFactory::create("parquet"), std::invalid_argument);
}

TEST(DataFormatStrategyFactoryTest, EmptyFormatThrows) {
    EXPECT_THROW(DataFormatStrategyFactory::create(""), std::invalid_argument);
}
