#pragma once

#include <SDL.h>

#include "FileLoader.hpp"

class TextureLoader final
{
public:
    TextureLoader(FileLoader &fileLoader);

    std::shared_ptr<SDL_Texture> loadTexture(SDL_Renderer *renderer, std::string_view relPath);
    std::shared_ptr<SDL_Texture> loadTexture(SDL_Renderer *renderer, uint32_t id);

private:
    std::shared_ptr<SDL_Texture> findTexture(std::string_view relPath) const;

    FileLoader &fileLoader;

    std::map<std::string, std::weak_ptr<SDL_Texture>, std::less<>> textures;
};