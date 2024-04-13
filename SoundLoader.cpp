#include <iostream>

#include "SoundLoader.hpp"
#include "RWOps.hpp"

SoundLoader::SoundLoader(FileLoader &fileLoader) : fileLoader(fileLoader)
{
}

std::shared_ptr<Mix_Chunk> SoundLoader::loadSound(std::string_view relPath)
{
    auto tex = findSound(relPath);

    if(tex)
        return tex;

    // TODO: some sounds have .dat files with a few properties

    auto stream = fileLoader.openResourceFile(relPath);

    if(!stream)
    {
        std::cerr << "Failed to open " << relPath << "\n";
        return nullptr;
    }

    // load bmp
    auto sound = Mix_LoadWAV_RW(rwOpsFromStream(stream), true);

    if(!sound)
    {
        std::cerr << "Failed to load sound " << relPath << "(" << SDL_GetError() << ")" << "\n";
        return nullptr;
    }


    std::shared_ptr<Mix_Chunk> texPtr(sound, Mix_FreeChunk);

    // save
    sounds.emplace(relPath, texPtr);

    return texPtr;
}

std::shared_ptr<Mix_Chunk> SoundLoader::loadSound(int32_t id)
{
    // this would use openResourceFile(id), but we need to normalise id/path for the cache
    auto path = fileLoader.lookupId(id, ".wav");

    if(!path)
        return nullptr;

    return loadSound(path.value());
}

std::shared_ptr<Mix_Chunk> SoundLoader::findSound(std::string_view relPath) const
{
    auto it = sounds.find(relPath);

    if(it != sounds.end())
    {
        auto ptr = it->second.lock();
        if(ptr)
            return ptr;
    }

    return nullptr;
}
