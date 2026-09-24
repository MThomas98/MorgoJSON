#pragma once

#include <filesystem>

class JSONObject
{
public:
    bool read(std::filesystem::path const& file_path);

    std::string getData() const;

private:
    

    std::string m_data;
};