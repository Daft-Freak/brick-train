#include <filesystem>
#include <iostream>

#include <SDL.h>

#include "FileLoader.hpp"
#include "ObjectDataStore.hpp"
#include "TextureLoader.hpp"
#include "World.hpp"

namespace fs = std::filesystem;

static bool quit = false;

static void pollEvents(World &world)
{
    SDL_Event event;
    while(SDL_PollEvent(&event))
    {
        switch(event.type)
        {
            case SDL_WINDOWEVENT:
            {
                if(event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                {
                    // get the real size used by the renderer
                    auto window = SDL_GetWindowFromID(event.window.windowID);
                    auto renderer = SDL_GetRenderer(window);
                    int w, h;
                    SDL_GetRendererOutputSize(renderer, &w, &h);

                    world.setWindowSize(w, h);
                }
                break;
            }
            case SDL_QUIT:
                quit = true;
                break;

            default:
                world.handleEvent(event);
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
        basePath = fs::canonical(tmp);
        SDL_free(tmp);
    }

    // setup loaders/resources
    FileLoader fileLoader(basePath);
    TextureLoader texLoader(fileLoader);
    ObjectDataStore objStore(fileLoader);

    fileLoader.addResourceFile("disc/art-res/resource");

    auto &dataPath = fileLoader.getDataPath();

    // SDL init
    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0)
    {
        std::cerr << "Failed to init SDL!\n";
        return 1;
    }

    auto window = SDL_CreateWindow("Brick Train", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, screenWidth, screenHeight, SDL_WINDOW_ALLOW_HIGHDPI);

    auto renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC);

    texLoader.setRenderer(renderer);

    World testWorld(fileLoader, texLoader, objStore);

    testWorld.setWindowSize(screenWidth, screenHeight);

    testWorld.loadSave(dataPath / "disc/art-res/SAVEGAME/4BRIDGES.SAV");

    uint32_t lastTime = SDL_GetTicks();

    while(!quit)
    {
        pollEvents(testWorld);

        uint32_t now = SDL_GetTicks();
        auto delta = now - lastTime;
        lastTime = now;

        testWorld.update(delta);

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        testWorld.render(renderer);

        SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    return 0;
}
