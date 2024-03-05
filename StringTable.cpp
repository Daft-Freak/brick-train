#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

#include "StringTable.hpp"

static std::string convertUCS2ToUTF8(std::u16string_view u16)
{
    std::string ret;
    ret.reserve(u16.length()); // optimistic

    for(auto &c : u16)
    {
        assert(c < 0xD800 || c >= 0xE000); // not UTF-16, no surrogates

        if(c <= 0x7F)
            ret += static_cast<char>(c);
        else if(c <= 0x7FFF)
        {
            ret += static_cast<char>(0xC0 | c >> 6);
            ret += static_cast<char>(0x80 | (c & 0x3F));
        }
        else // <= 0xFFFF
        {
            ret += static_cast<char>(0xE0 | c >> 12);
            ret += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
            ret += static_cast<char>(0x80 | (c & 0x3F));
        }
    }

    return ret;
}

bool StringTable::loadFromExe(const std::filesystem::path &path)
{
    std::ifstream file(path, std::ios::binary);

    if(!file)
    {
        std::cerr << "Failed to open " << path << "\n";
        return false;
    }

    // extract the strings from the exe's resources

    // get offset to PE signature
    file.seekg(0x3C);

    uint8_t buf[40];
    file.read(reinterpret_cast<char *>(buf), 4);

    uint32_t peSigOffset = buf[0] | buf[1] << 8 | buf[2] << 16 | buf[3] << 24;

    // check signature
    file.seekg(peSigOffset);
    file.read(reinterpret_cast<char *>(buf), 4);

    if(memcmp(buf, "PE\0", 4) != 0)
    {
        std::cerr << "Bad PE signature in " << path << "\n";
        return false;
    }

    // look through sections

    if(file.read(reinterpret_cast<char *>(buf), 18).gcount() != 18)
    {
        return false;
    }

    uint16_t numSections = buf[2] | buf[3] << 8;
    uint16_t optionalHeaderSize = buf[16] | buf[17] << 8;

    file.seekg(peSigOffset + 24 + optionalHeaderSize);

    // resource table helper
    auto readResourceTable = [&file](uint32_t offset)
    {
        std::vector<std::tuple<uint32_t, uint32_t>> ret;
    
        uint8_t buf[16];
        file.seekg(offset);

        if(file.read(reinterpret_cast<char *>(buf), sizeof(buf)).gcount() != sizeof(buf))
            return ret;

        uint16_t numNameEntries = buf[12] | buf[13] << 8;
        uint16_t numIdEntries = buf[14] | buf[15] << 8;

        // skip name entries
        file.seekg(8 * numNameEntries, std::ios::cur);

        for(unsigned int i = 0; i < numIdEntries; i++)
        {
            if(file.read(reinterpret_cast<char *>(buf), 8).gcount() != 8)
                break;

            uint32_t id = buf[0] | buf[1] << 8 | buf[2] << 16 | buf[3] << 24;
            uint32_t offset = buf[4] | buf[5] << 8 | buf[6] << 16 | buf[7] << 24;

            ret.emplace_back(id, offset);
        }

        return ret;
    };

    for(unsigned int i = 0; i < numSections; i++)
    {
        if(file.read(reinterpret_cast<char *>(buf), 40).gcount() != 40)
        {
            std::cerr << "Failed to read section header in " << path << "\n";
            return false;
        }

        auto name = reinterpret_cast<char *>(buf);
        if(std::string_view(name) == ".rsrc")
        {
            // resource section
            uint32_t virtualAddress = buf[12] | buf[13] << 8 | buf[14] << 16 | buf[26] << 24;
            uint32_t rawDataOff = buf[20] | buf[21] << 8 | buf[22] << 16 | buf[23] << 24;
            auto typeIdEntries = readResourceTable(rawDataOff);

            for(auto typeEntry : typeIdEntries)
            {
                if(std::get<0>(typeEntry) == 6) // string
                {
                    auto nameOffset = std::get<1>(typeEntry) & 0x7FFFFFFF;
                    auto nameIdEntries = readResourceTable(rawDataOff + nameOffset);

                    for(auto &nameEntry : nameIdEntries)
                    {
                        auto nameId = std::get<0>(nameEntry);
                        auto langOffset = std::get<1>(nameEntry) & 0x7FFFFFFF;
                        auto langIdEntries = readResourceTable(rawDataOff + langOffset);

                        for(auto &langEntry : langIdEntries)
                        {
                            // almost reached the data
                            file.seekg(rawDataOff + std::get<1>(langEntry));

                            file.read(reinterpret_cast<char *>(buf), 4);

                            uint32_t dataAddr = buf[0] | buf[1] << 8 | buf[2] << 16 | buf[3] << 24;
                            file.seekg(dataAddr - virtualAddress + rawDataOff);

                            // now we can finally read the data
                            for(int j = 0; j < 16; j++) // each resource contains 16 strings
                            {
                                if(file.read(reinterpret_cast<char *>(buf), 2).gcount() != 2)
                                    break;

                                uint16_t length = buf[0] | buf[1] << 8;

                                if(length == 0)
                                    continue;

                                auto stringId = (nameId - 1) * 16 + j;

                                std::u16string str(length, 0);

                                file.read(reinterpret_cast<char *>(str.data()), length * 2);

                                // convert to utf-8
                                auto u8str = convertUCS2ToUTF8(str);

                                strings.emplace(stringId, std::move(u8str));
                            }
                        }
                    }
                }
            }

            break;
        }
    }

    return false;
}

std::optional<std::string_view> StringTable::lookupString(uint32_t id) const
{
    auto it = strings.find(id);

    if(it != strings.end())
        return it->second;

    return {};
}
