#include <gtest/gtest.h>

#include <morgojson/JSONObject.hpp>

#include <expected>
#include <filesystem>
#include <fstream>
#include <system_error>

namespace
{
    std::filesystem::path const test_data_dir{MORGOJSON_TEST_DATA_DIR};

    std::expected<std::string, std::error_code> readFile(std::filesystem::path const& file_path)
    {
        std::error_code error;
        auto const file_size = std::filesystem::file_size(file_path, error);
        if (error) return std::unexpected{error};

        std::ifstream in{file_path};
        if (!in) return std::unexpected{std::make_error_code(std::errc::io_error)};

        std::string data;
        data.resize_and_overwrite(file_size, 
            [&in](char* buffer, std::size_t size)
            {
                in.read(buffer, static_cast<std::streamsize>(size));
                return static_cast<std::size_t>(in.gcount());
            });
        
        return data;
    }
}

TEST(JSONObject, ReadsFile)
{
    auto const file_path = test_data_dir / "test_1.json";
    auto const expected = readFile(file_path);
    ASSERT_TRUE(expected.has_value()) << expected.error().message();

    JSONObject object;
    ASSERT_TRUE(object.read(file_path));
    EXPECT_EQ(object.getData(), *expected);
}

TEST(JSONObject, ReadFailsForMissingFile)
{
    JSONObject object;
    EXPECT_FALSE(object.read(test_data_dir / "does_not_exist.json"));
    EXPECT_EQ(object.getData(), "");
}
