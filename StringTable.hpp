#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>

class StringTable final
{
public:
    bool loadFromExe(const std::filesystem::path &path);

    std::optional<std::string_view> lookupString(uint32_t id) const;

private:
    std::map<uint32_t, std::string> strings;
};