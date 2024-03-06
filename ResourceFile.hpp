#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <vector>

class ResourceFile final
{
public:
    ResourceFile(const std::filesystem::path &path);

    std::optional<std::vector<uint8_t>> getResourceContents(std::string_view path);

private:
    struct ResourceHeader
    {
        uint32_t offset;
        uint32_t size;
        uint32_t flags;
    };

    std::filesystem::path dataPath;

    std::map<std::string, ResourceHeader, std::less<>> resources;
};