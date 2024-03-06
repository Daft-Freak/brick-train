#include <iostream>

#include "TextureLoader.hpp"

// istream -> RWops wrapper
// DOES NOT TAKE OWNERSHIP OF THE STREAM
// (don't free it)
static SDL_RWops *rwOpsFromStream(const std::unique_ptr<std::istream> &stream)
{
    auto rwops = SDL_AllocRW();
    rwops->size = [](SDL_RWops *context) -> Sint64
    {
        return -1; // TODO?
    };

    rwops->seek = [](SDL_RWops *context, Sint64 offset, int whence) -> Sint64
    {
        auto stream = reinterpret_cast<std::istream *>(context->hidden.unknown.data1);

        auto dir = std::ios::beg;
        switch(whence)
        {
            case RW_SEEK_SET:
                break;
            case RW_SEEK_CUR:
                dir = std::ios::cur;
                break;
            case RW_SEEK_END:
                dir = std::ios::end;
                break;
            default:
                return -1;
        }

        // only handles read ptr
        stream->seekg(offset, dir);

        if(!*stream)
            return -1;

        return stream->tellg();
    };

    rwops->read = [](SDL_RWops *context, void *ptr, size_t size, size_t maxnum) -> size_t
    {
        auto stream = reinterpret_cast<std::istream *>(context->hidden.unknown.data1);

        return stream->read(reinterpret_cast<char *>(ptr), size * maxnum).gcount();
    };

    rwops->write = [](SDL_RWops *context, const void *ptr, size_t size, size_t maxnum) -> size_t
    {
        return 0; // read-only
    };

    rwops->close = [](SDL_RWops *context) -> int
    {
        return 0;
    };

    rwops->hidden.unknown.data1 = stream.get();

    return rwops;
}

TextureLoader::TextureLoader(FileLoader &fileLoader) : fileLoader(fileLoader)
{
}

std::shared_ptr<SDL_Texture> TextureLoader::loadTexture(SDL_Renderer *renderer, std::string_view relPath)
{
    auto tex = findTexture(relPath);

    if(tex)
        return tex;

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

std::shared_ptr<SDL_Texture> TextureLoader::loadTexture(SDL_Renderer *renderer, uint32_t id)
{
    // this would use openResourceFile(id), but we need to normalise id/path for the cache
    auto path = fileLoader.lookupId(id, ".bmp");

    if(!path)
        return nullptr;

    return loadTexture(renderer, path.value());
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
