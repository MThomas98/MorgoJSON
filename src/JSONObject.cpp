#include <morgojson/JSONObject.hpp>

#include <fstream>
#include <system_error>

bool JSONObject::read(std::filesystem::path const& file_path)
{
    std::error_code error;
    auto const size = std::filesystem::file_size(file_path, error);
    if (error) return false;

    std::ifstream in{file_path};
    if (!in) return false;

    m_data.resize_and_overwrite(size, 
        [&in](char* buffer, std::size_t size)
        {
            in.read(buffer, static_cast<std::streamsize>(size));
            return static_cast<std::size_t>(in.gcount());
        });

    return true;
}

std::string JSONObject::getData() const
{
    return m_data;
}

void JSONObject::parse(std::string_view data)
{
    
}
