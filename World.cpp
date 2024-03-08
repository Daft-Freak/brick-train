#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>

#include "World.hpp"
#include "ObjectData.hpp"

World::World(TextureLoader &texLoader, ObjectDataStore &objectDataStore) : texLoader(texLoader), objectDataStore(objectDataStore)
{
}

World::~World()
{
    delete[] tileObjectType;
}

bool World::loadSave(const std::filesystem::path &path, SDL_Renderer *renderer)
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

    if(renderer)
    {
        std::string backdropPath("backdrop/");

        // build path, default to "backdrop"
        backdropPath.append(backdropName[0] ? backdropName : "backdrop").append(".bmp");

        backdrop = texLoader.loadTexture(renderer, backdropPath);
    }

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

        // attempt to get texture
        std::shared_ptr<SDL_Texture> texture;

        if(renderer)
            texture = texLoader.loadTexture(renderer, objectId);

        // load object data
        auto data = objectDataStore.getObject(objectId);

        objects.emplace_back(Object{objectId, objectX, objectY, objectName, texture, data, {}});

        bool hasUnk = false;

        for(int j = 6; j < 16; j++)
        {
            if(objectData[j])
            {
                if(!hasUnk)
                    std::cout << "object " << objectId << " (\"" << objectName << "\") at " << objectX << ", " << objectY << std::hex;

                std::cout << " unk" << j << "=" << int(objectData[j]);
                hasUnk = true;
            }
        }

        if(hasUnk)
            std::cout << std::dec << std::endl;

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
    if(backdrop)
    {
        // TODO: repeat/scale?
        SDL_Rect r = {};
        SDL_QueryTexture(backdrop.get(), nullptr, nullptr, &r.w, &r.h);
        SDL_RenderCopy(renderer, backdrop.get(), nullptr, &r);
    }

    for(auto &object : objects)
    {
        // TODO: layers, ordering, everything else
        if(object.texture && object.data)
        {
            // TODO: animations
            int w = object.data->bitmapSizeX * 16;
            int h = object.data->bitmapSizeY * 16;

            SDL_Rect sr{0, 0, w, h};
            SDL_Rect dr = {object.x * 16, object.y * 16, w, h};

            SDL_RenderCopy(renderer, object.texture.get(), &sr, &dr);
        }
    }
}