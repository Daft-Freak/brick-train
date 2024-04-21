#include "SoundMixer.hpp"

SoundMixer::SoundMixer(FileLoader &fileLoader) : loader(fileLoader)
{
}

int SoundMixer::playSound(const std::shared_ptr<Mix_Chunk> &sound, int priority)
{
    // TODO: priority
    int chan = Mix_PlayChannel(-1, sound.get(), 0);

    if(chan != -1)
        playingSounds[chan] = sound;

    return chan;
}

SoundLoader &SoundMixer::getLoader()
{
    return loader;
}
