#pragma once

#include <map>

#include <SDL_mixer.h>

#include "SoundLoader.hpp"

class SoundMixer final
{
public:
    SoundMixer(FileLoader &fileLoader);

    uint32_t playSound(const std::shared_ptr<Mix_Chunk> &sound, int priority);

    bool isSoundPlaying(uint32_t id) const;

    SoundLoader &getLoader();

private:
    SoundLoader loader;

    struct SoundInfo
    {
        std::shared_ptr<Mix_Chunk> chunk;
        uint32_t id;
    };

    uint32_t nextSoundId = 0;

    std::map<int, SoundInfo> playingSounds;
};