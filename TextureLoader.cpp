#include <iostream>

#include "TextureLoader.hpp"
#include "RWOps.hpp"

TextureLoader::TextureLoader(FileLoader &fileLoader) : fileLoader(fileLoader)
{
}

std::shared_ptr<SDL_Texture> TextureLoader::loadTexture(std::string_view relPath)
{
    auto tex = findTexture(relPath);

    if(tex)
        return tex;

    if(!renderer)
        return nullptr;

    auto stream = fileLoader.openResourceFile(relPath);

    if(!stream)
    {
        std::cerr << "Failed to open " << relPath << "\n";
        return nullptr;
    }

    // load bmp
    auto surface = SDL_LoadBMP_RW(rwOpsFromStream(stream), true);

    if(!surface)
    {
        std::cerr << "Failed to load bitmap " << relPath << "(" << SDL_GetError() << ")" << "\n";
        return nullptr;
    }

    // indexed bitmaps use index 0 as transparent
    // TODO: is this always true?
    if(SDL_PIXELTYPE(surface->format->format) == SDL_PIXELTYPE_INDEX8)
    {
        // set index 0 to transparent and convert to RGBA
        SDL_Colour trans{0, 0, 0, 0};
        SDL_SetPaletteColors(surface->format->palette, &trans, 0, 1);

        auto newSurf = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA32, 0);
        SDL_FreeSurface(surface);
        surface = newSurf;
    }

    // create texture
    auto texture = SDL_CreateTextureFromSurface(renderer, surface);

    SDL_FreeSurface(surface);

    if(!texture)
    {
        std::cerr << "Failed to create texture from " << relPath << "(" << SDL_GetError() << ")" << "\n";
        return nullptr;
    }

    std::shared_ptr<SDL_Texture> texPtr(texture, SDL_DestroyTexture);

    // save
    textures.emplace(relPath, texPtr);

    return texPtr;
}

std::shared_ptr<SDL_Texture> TextureLoader::loadTexture(int32_t id)
{
    // this would use openResourceFile(id), but we need to normalise id/path for the cache
    auto path = fileLoader.lookupId(id, ".bmp");

    if(!path)
        return nullptr;

    return loadTexture(path.value());
}

void TextureLoader::setRenderer(SDL_Renderer *renderer)
{
    this->renderer = renderer;
}

std::shared_ptr<SDL_Texture> TextureLoader::findTexture(std::string_view relPath) const
{
    auto it = textures.find(relPath);

    if(it != textures.end())
    {
        auto ptr = it->second.lock();
        if(ptr)
            return ptr;
    }

    return nullptr;
}
