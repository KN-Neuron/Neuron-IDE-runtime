#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "parser/Parser.hpp"
#include "scene/Scene.hpp"
#include "scene/SceneObject.hpp"
#include "scene/components/BlinkComponent.hpp"
#include "utils/ParserTestUtils.hpp"

TEST(ParserFileTest, ThrowsWhenFileDoesNotExist) {
    EXPECT_THROW(Parser::parse("/nonexistent/path/scene.pb"), std::runtime_error);
}

TEST(ParserFileTest, ReturnsNonNullSceneForValidFile) {
    auto              scene = utils::buildSimpleScene();
    const std::string path  = (std::filesystem::temp_directory_path() / "valid_scene.pb").string();
    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(out.is_open());
        scene.SerializeToOstream(&out);
    }

    auto result = Parser::parse(path);
    ASSERT_NE(result, nullptr);

    std::filesystem::remove(path);
}