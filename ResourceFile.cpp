#include <fstream>
#include <iostream>

#include "ResourceFile.hpp"

namespace fs = std::filesystem;

ResourceFile::ResourceFile(const fs::path &path)
{
    auto headerPath = fs::path(path).replace_extension("RFH");
    dataPath = fs::path(path).replace_extension("RFD");

    if(!fs::exists(headerPath) || !fs::exists(dataPath))
    {
        std::cerr << "Missing .RFD and/or .RFH for " << path << "\n";
        return;
    }

    // parse the header
    std::ifstream file(headerPath, std::ios::binary);

    uint32_t offset = 0;

    while(file)
    {
        auto readU32 = [&file]() -> uint32_t
        {
            uint8_t buf[4];
            file.read(reinterpret_cast<char *>(buf), 4);

            return buf[0] | buf[1] << 8 | buf[2] << 16 | buf[3] << 24;
        };

        auto nameLen = readU32();

        if(file.eof())
            break;

        std::string filename(nameLen - 1, 0);

        file.read(filename.data(), nameLen - 1);
        file.get(); // null

        auto fileSize = readU32();
        auto flags = readU32();

        // convert slashes and convert to lower case to help with lookups later
        for(auto &c : filename)
        {
            if(c =='\\')
                c = '/';
            else
                c = std::tolower(c);
        }

        resources.emplace(filename, ResourceHeader{offset, fileSize, flags});

        offset += fileSize;
    }
}

std::optional<std::vector<uint8_t>> ResourceFile::getResourceContents(std::string_view path)
{
    auto it = resources.find(path);

    if(it == resources.end())
        return {};

    auto &header = it->second;
    bool compressed = header.flags & 1;

    std::vector<uint8_t> data(header.size);

    std::ifstream file(dataPath, std::ios::binary);

    file.seekg(header.offset);

    file.read(reinterpret_cast<char *>(data.data()), data.size());

    // read failed
    if(!file || file.gcount() != header.size)
        return {};

    if(!compressed)
        return data;

    // decompress compressed file
    uint32_t decSize = data[0] | data[1] << 8 | data[2] << 16 | data[3] << 24;

    std::vector<uint8_t> decData(decSize);
    auto out = decData.begin();
    auto outEnd = decData.end();

    uint16_t rootNode = data[4] | data[5] << 8;
    uint16_t node = rootNode;
    auto in = data.begin() + 0x808;
    auto inEnd = data.end();

    for(; in != inEnd; ++in)
    {
        uint8_t bits = *in;
        for(int bit = 0; bit < 8; bit++, bits >>=1)
        {
            auto bitVal = bits & 1;
            auto nodePos = node * 4 + bitVal * 2 + 8;
            node = data[nodePos] | data[nodePos + 1] << 8;

            if(!(node & 0x100))
            {
                // terminal node
                *out++ = node & 0xFF;

                // stop at end of output buffer
                if(out == outEnd)
                    return decData;

                node = rootNode;
            }
        }
    }

    return decData;
}