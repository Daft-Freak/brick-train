#pragma once

#include <SDL.h>
#include <SDL_mixer.h>

#include "FileLoader.hpp"

class SoundLoader final
{
public:
    SoundLoader(FileLoader &fileLoader);

    std::shared_ptr<Mix_Chunk> loadSound(std::string_view relPath);
    std::shared_ptr<Mix_Chunk> loadSound(int32_t id);

private:
    std::shared_ptr<Mix_Chunk> findSound(std::string_view relPath) const;

    FileLoader &fileLoader;

    std::map<std::string, std::weak_ptr<Mix_Chunk>, std::less<>> sounds;
};