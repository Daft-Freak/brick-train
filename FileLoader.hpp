#pragma once
#include <filesystem>
#include <istream>
#include <list>

#include "ResourceFile.hpp"
#include "StringTable.hpp"

class FileLoader final
{
public:
    FileLoader(std::filesystem::path basePath);

    std::unique_ptr<std::istream> openResourceFile(std::string_view relPath);
    std::unique_ptr<std::istream> openResourceFile(uint32_t id, std::string_view ext);

    const std::filesystem::path &getDataPath();

    void addResourceFile(std::string_view relPath);

private:
    std::filesystem::path basePath, dataPath;

    StringTable stringTable;

    std::list<ResourceFile> resourceFiles;
};