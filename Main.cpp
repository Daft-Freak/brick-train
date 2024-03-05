#include <filesystem>
#include <iostream>

#include <SDL.h>

#include "World.hpp"

namespace fs = std::filesystem;

static bool quit = false;

static void pollEvents()
{
    SDL_Event event;
    while(SDL_PollEvent(&event))
    {
        switch(event.type)
        {
            case SDL_QUIT:
                quit = true;
                break;
        }
    }
}

int main(int argc, char *argv[])
{
    const int screenWidth = 1280;
    const int screenHeight = 1024;

    // get base path
    fs::path basePath;
    auto tmp = SDL_GetBasePath();
    if(tmp)
    {
        basePath = tmp;
        SDL_free(tmp);
    }

    // find data path
    fs::path dataPath = basePath / "data";
    if(!fs::exists(dataPath))
        dataPath = basePath / "../data";

    // SDL init
    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0)
    {
        std::cerr << "Failed to init SDL!\n";
        return 1;
    }

    auto window = SDL_CreateWindow("Brick Train", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, screenWidth, screenHeight, 0);

    auto renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC);

    World testWorld;

    testWorld.loadSave(dataPath / "disc/art-res/SAVEGAME/4BRIDGES.SAV");

    while(!quit)
    {
        pollEvents();

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        testWorld.render(renderer);

        SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    return 0;
}
