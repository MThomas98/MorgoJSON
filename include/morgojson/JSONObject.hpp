#pragma once

#include <filesystem>
#include <string>
#include <string_view>

class JSONObject
{
public:
    bool read(std::filesystem::path const& file_path);

    std::string getData() const;

private:
    void parse(std::string_view data);

    std::string m_data;
};