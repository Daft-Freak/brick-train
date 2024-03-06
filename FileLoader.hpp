#pragma once
#include <filesystem>
#include <istream>
#include <list>

#include "ResourceFile.hpp"

class FileLoader final
{
public:
    FileLoader(std::filesystem::path basePath);

    std::unique_ptr<std::istream> openResourceFile(std::string_view relPath);

    const std::filesystem::path &getDataPath();

    void addResourceFile(std::string_view relPath);

private:
    std::filesystem::path basePath, dataPath;

    std::list<ResourceFile> resourceFiles;
};