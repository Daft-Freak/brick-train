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

        // 6-7 are probably padding to align the oversized frameset index

        int framesetIndex = objectData[8] | objectData[9] << 8 | objectData[10] << 16 | objectData[11] << 24; // this is waaay bigger than it needs to be

        // 4 bytes unknown?

        char *objectName = reinterpret_cast<char *>(objectData + 16);

        // attempt to get texture
        std::shared_ptr<SDL_Texture> texture;

        if(renderer)
            texture = texLoader.loadTexture(renderer, objectId);

        // load object data
        auto data = objectDataStore.getObject(objectId);

        auto &object = objects.emplace_back(objectId, objectX, objectY, objectName, texture, data);

        object.setAnimation(framesetIndex);

        bool hasUnk = false;

        for(int j = 6; j < 16; j++)
        {
            if((j < 8 || j >= 12) && objectData[j])
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

            object.minifigs.emplace_back(Minifig{minifigId, minifigName});

            if(minifigId)
            {
                std::cout << "\tminifig " << minifigId << " \"" << minifigName << "\" (unk " << std::hex
                          << int(minifigData[4]) << ", " << int(minifigData[5]) << ", "
                          << int(minifigData[6]) << ", " << int(minifigData[7]) << std::dec << ")\n";
            }
        }
    }

    // TODO: trains

    clampScroll();

    return true;
}

void World::update(uint32_t deltaMs)
{
    for(auto &object : objects)
        object.update(deltaMs);
}

void World::render(SDL_Renderer *renderer)
{
    if(backdrop)
    {
        // TODO: repeat/scale?
        SDL_Rect r = {-scrollX, -scrollY, 0, 0};
        SDL_QueryTexture(backdrop.get(), nullptr, nullptr, &r.w, &r.h);
        SDL_RenderCopy(renderer, backdrop.get(), nullptr, &r);
    }

    for(auto &object : objects)
        object.render(renderer, scrollX, scrollY, 1);

    // TODO: minifigs/trains

    for(int z = 2; z < 7; z++)
    {
        for(auto &object : objects)
            object.render(renderer, scrollX, scrollY, z);
    }
}

void World::setWindowSize(unsigned int windowWidth, unsigned int windowHeight)
{
    this->windowWidth = windowWidth;
    this->windowHeight = windowHeight;

    clampScroll();
}

void World::clampScroll()
{
    unsigned int worldWidth = width * tileSize;
    unsigned int worldHeight = height * tileSize;

    auto clampDim = [](unsigned int worldDim, unsigned int windowDim, int &scroll)
    {
        if(worldDim < windowDim)
        {
            // world smaller than display, center it
            scroll = static_cast<int>(worldDim - windowDim) / 2;
        }
        else
        {
            // world is larger, clamp to edges
            scroll = std::max(0, std::min(static_cast<int>(worldDim - windowDim), scroll));
        }
    };

    clampDim(worldWidth, windowWidth, scrollX);
    clampDim(worldHeight, windowHeight, scrollY);
}

World::Object::Object(uint16_t id, uint16_t x, uint16_t y, std::string name, std::shared_ptr<SDL_Texture> texture, const ObjectData *data) : id(id), x(x), y(y), name(name), texture(texture), data(data)
{
    // set the default animation
    if(data && data->defaultFrameset != -1)
        setAnimation(data->defaultFrameset);
}

void World::Object::update(uint32_t deltaMs)
{
    if(!data)
        return;

    // TODO: sounds
    if(currentAnimation != -1)
    {
        animationTimer -= deltaMs;

        while(animationTimer < 0)
        {
            auto &frameset = data->framesets[currentAnimation];

            if(nextAnimation != -1)
            {
                // delayed frameset change
                currentAnimation = nextAnimation;
                currentAnimationFrame = data->framesets[currentAnimation].startFrame;
                nextAnimation = -1;
            }
            else
                currentAnimationFrame += frameset.splitFrames ? 2 : 1;

            if(currentAnimationFrame > frameset.endFrame)
            {
                currentAnimationFrame = frameset.endFrame; // hold the last frame

                // move to next animation if one set
                if(frameset.nextFrameSet != -1)
                {
                    nextAnimation = frameset.nextFrameSet;
                    animationTimer = frameset.restartDelay * 1000;
                    continue;
                }

                // otherwise stop
                currentAnimation = frameset.nextFrameSet;
                break;
            }

            animationTimer += getFrameDelay();
        }
    }
}

void World::Object::render(SDL_Renderer *renderer, int scrollX, int scrollY, int z)
{
    if(!texture || !data)
        return;

    // animation info
    auto frameset = getCurrentFrameset();

    // "split" frames render a second image above the first one
    bool split = frameset ? frameset->splitFrames : false;

    if(z > data->maxBitmapOccupancy + (split ? 1 : 0))
        return;

    int w = data->bitmapSizeX * tileSize;

    int frameOffset = currentAnimationFrame * w;

    // copy frame
    for(int ty = 0; ty < int(data->bitmapSizeY); ty++)
    {
        for(int tx = 0; tx < int(data->bitmapSizeX); tx++)
        {
            int tileZ = data->bitmapOccupancy[tx + ty * data->bitmapSizeX];
            
            SDL_Rect sr{frameOffset + tx * tileSize, ty * tileSize, tileSize, tileSize};
            SDL_Rect dr{(x + tx) * tileSize - scrollX, (y + ty) * tileSize - scrollY, tileSize, tileSize};

            if(tileZ == z)
                SDL_RenderCopy(renderer, texture.get(), &sr, &dr);
            // TODO: try to combine?

            if(split && tileZ + 1 == z)
            {
                // second layer, a bit higher
                sr.x += w;
                SDL_RenderCopy(renderer, texture.get(), &sr, &dr);
            }
        }
    }
}

void World::Object::renderDebug(SDL_Renderer *renderer, int scrollX, int scrollY)
{
    if(!data)
        return;

    // "coords"
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);

    for(auto &coord : data->coords)
        SDL_RenderDrawPoint(renderer, std::get<0>(coord) + x * tileSize - scrollX, std::get<1>(coord) + y * tileSize - scrollY);

    // the other set (points/cross)
    SDL_SetRenderDrawColor(renderer, 255, 0, 255, 255);

    for(auto &coord : data->altCoords)
        SDL_RenderDrawPoint(renderer, std::get<0>(coord) + x * tileSize - scrollX, std::get<1>(coord) + y * tileSize - scrollY);

    // entry/exit
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);

    // left
    int px = 0;
    int py = data->entryExitOffsets[0];
    if(py)
        SDL_RenderDrawPoint(renderer, px + x * tileSize - scrollX, py + y * tileSize - scrollY);

    // bottom
    px = data->entryExitOffsets[1];
    py = data->bitmapSizeY * tileSize - 1;
    if(px)
        SDL_RenderDrawPoint(renderer, px + x * tileSize - scrollX, py + y * tileSize - scrollY);

    // right
    px = data->bitmapSizeX * tileSize - 1;
    py = data->entryExitOffsets[2];
    if(py)
        SDL_RenderDrawPoint(renderer, px + x * tileSize - scrollX, py + y * tileSize - scrollY);

    // top
    px = data->entryExitOffsets[3];
    py = 0;
    if(px)
        SDL_RenderDrawPoint(renderer, px + x * tileSize - scrollX, py + y * tileSize - scrollY);

    // free to roam
    SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
    SDL_Rect r{data->freeToRoam[0] + x * tileSize - scrollX, data->freeToRoam[1] + y * tileSize - scrollY, data->freeToRoam[2] - data->freeToRoam[0], data->freeToRoam[3] - data->freeToRoam[1]};
    
    if(r.w && r.h)
        SDL_RenderDrawRect(renderer, &r);
}

const ObjectData::Frameset *World::Object::getCurrentFrameset() const
{
    if(currentAnimation != -1)
        return &data->framesets[currentAnimation];

    return nullptr;
}

int World::Object::getFrameDelay() const
{
    auto frameset = getCurrentFrameset();
    if(frameset)
        return std::max(1, frameset->delay) * 30; // bit of a guess

    return 0;
}

void World::Object::setAnimation(int index)
{
    if(index < 0 || !data || index > data->numFramesets)
        return;

    currentAnimation = index;

    auto &frameset = data->framesets[currentAnimation];
    currentAnimationFrame = frameset.startFrame;

    animationTimer = getFrameDelay();
}
