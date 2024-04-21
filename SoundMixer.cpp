#include "SoundMixer.hpp"

SoundMixer::SoundMixer(FileLoader &fileLoader) : loader(fileLoader)
{
}

uint32_t SoundMixer::playSound(const std::shared_ptr<Mix_Chunk> &sound, int priority)
{
    // TODO: priority
    int chan = Mix_PlayChannel(-1, sound.get(), 0);

    if(chan != -1)
    {
        SoundInfo newInfo;
        newInfo.chunk = sound;
        newInfo.id = nextSoundId++;        
        playingSounds[chan] = newInfo;

        return newInfo.id;
    }

    return ~0u;
}

bool SoundMixer::isSoundPlaying(uint32_t id) const
{
    for(auto &soundChan : playingSounds)
    {
        if(soundChan.second.id == id)
            return Mix_Playing(soundChan.first);
    }
    return false;
}

SoundLoader &SoundMixer::getLoader()
{
    return loader;
}
