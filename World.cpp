#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>

#include "World.hpp"

World::~World()
{
    delete[] tileObjectType;
}

bool World::loadSave(const std::filesystem::path &path)
{
    std::ifstream file(path, std::ios::binary);

    if(!file)
    {
        std::cerr << "Failed to open " << path;
        return false;
    }

    // initial header
    uint8_t header[0x114];

    if(file.read(reinterpret_cast<char *>(header), sizeof(header)).gcount() != sizeof(header))
    {
        std::cerr << "Failed to read header for " << path << "\n";
        return false;
    }

    // always the same in every file so far
    assert(header[0] == 8 && header[1] == 0);
    assert(header[6] == 0 && header[7] == 0);

    width = header[2] | header[3] << 8;
    height = header[4] | header[5] << 8;

    uint32_t numObjects = header[8] | header[9] << 8 | header[10] << 16 | header[11] << 24;
    uint16_t numTrains = header[12] | header[13] << 8;

    char *backdropName = reinterpret_cast<char *>(header + 14);

    // the rest is usually 0
#ifndef NDEBUG
    auto ptr = header + 14 + strlen(backdropName);
    for(; ptr < header + sizeof(ptr); ptr++)
        assert(*ptr == 0);
#endif

    // type of object in each tile?
    // might be the preview image?
    // empty tile=0, scenery=2, (non-rail)building=3, track=5, road=6, footpath=7
    delete[] tileObjectType;
    tileObjectType = new uint8_t[width * height];

    if(file.read(reinterpret_cast<char *>(tileObjectType), width * height).gcount() != width * height)
    {
        std::cerr << "Failed to read tile type data for " << path << "\n";
        return false;
    }

    // next is object data
    objects.clear();
    objects.reserve(numObjects);

    for(uint32_t i = 0; i < numObjects; i++)
    {
        uint8_t objectData[0x80];

        if(file.read(reinterpret_cast<char *>(objectData), sizeof(objectData)).gcount() != sizeof(objectData))
        {
            std::cerr << "Failed to object " << i << " in " << path << "\n";
            return false;
        }

        uint16_t objectId = objectData[0] | objectData[1] << 8;
        uint16_t objectX = objectData[2] | objectData[3] << 8;
        uint16_t objectY = objectData[4] | objectData[5] << 8;

        // 10 bytes unknown?

        char *objectName = reinterpret_cast<char *>(objectData + 16);

        objects.emplace_back(Object{objectId, objectX, objectY, objectName, {}});

        std::cout << "object " << objectId << " (\"" << objectName << "\") at " << objectX << ", " << objectY << std::hex;

        for(int j = 6; j < 16; j++)
        {
            if(objectData[j])
                std::cout << " unk" << j << "=" << int(objectData[j]);
        }

        std::cout << std::dec << "\n";

        // up to 5 minifigs
        auto minifigData = objectData + 0x1C;
        for(int j = 0; j < 5; j++, minifigData += 20)
        {
            uint32_t minifigId = minifigData[0] | minifigData[1] << 8 | minifigData[2] << 16 | minifigData[3] << 24;

            // 4 bytes unknown (possibly uninitialised data?)
            char *minifigName = reinterpret_cast<char *>(minifigData + 8);

            objects.back().minifigs.emplace_back(Minifig{minifigId, minifigName});

            if(minifigId)
            {
                std::cout << "\tminifig " << minifigId << " \"" << minifigName << "\" (unk " << std::hex
                          << int(minifigData[4]) << ", " << int(minifigData[5]) << ", "
                          << int(minifigData[6]) << ", " << int(minifigData[7]) << std::dec << ")\n";
            }
        }
    }

    // TODO: trains

    return true;
}

void World::render(SDL_Renderer *renderer)
{
    for(auto &object : objects)
    {
        // TODO: backdrop, images, layers, everything else
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
        SDL_Rect r{object.x * 16, object.y * 16, 16, 16};
        SDL_RenderFillRect(renderer, &r);
    }
}