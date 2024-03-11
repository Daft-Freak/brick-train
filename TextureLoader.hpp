#pragma once

#include <SDL.h>

#include "FileLoader.hpp"

class TextureLoader final
{
public:
    TextureLoader(FileLoader &fileLoader);

    std::shared_ptr<SDL_Texture> loadTexture(std::string_view relPath);
    std::shared_ptr<SDL_Texture> loadTexture(int32_t id);

    void setRenderer(SDL_Renderer *renderer);

private:
    std::shared_ptr<SDL_Texture> findTexture(std::string_view relPath) const;

    FileLoader &fileLoader;

    SDL_Renderer *renderer = nullptr;

    std::map<std::string, std::weak_ptr<SDL_Texture>, std::less<>> textures;
};