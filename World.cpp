#include <algorithm>
#include <cassert>
#include <charconv>
#include <cstring>
#include <fstream>
#include <iostream>

#include "World.hpp"

#include "IniFile.hpp"
#include "ObjectData.hpp"

World::World(FileLoader &fileLoader, TextureLoader &texLoader, ObjectDataStore &objectDataStore) :
    fileLoader(fileLoader), texLoader(texLoader), objectDataStore(objectDataStore), randomGen(std::random_device{}())
{
    loadEasterEggs();
}

World::~World()
{
    delete[] tileObjectType;
}

bool World::loadSave(const std::filesystem::path &path)
{
    std::ifstream file(path, std::ios::binary);

    if(!file)
    {
        std::cerr << "Failed to open " << path;
        return false;
    }

    // initial header
    uint8_t header[0x114];

    if(file.read(reinterpret_cast<char *>(header), sizeof(header)).gcount() != sizeof(header))
    {
        std::cerr << "Failed to read header for " << path << "\n";
        return false;
    }

    // always the same in every file so far
    assert(header[0] == 8 && header[1] == 0);
    assert(header[6] == 0 && header[7] == 0);

    width = header[2] | header[3] << 8;
    height = header[4] | header[5] << 8;

    uint32_t numObjects = header[8] | header[9] << 8 | header[10] << 16 | header[11] << 24;
    uint16_t numTrains = header[12] | header[13] << 8;

    // load backdrop
    char *backdropName = reinterpret_cast<char *>(header + 14);

    backdropPath = "backdrop/";

    // build path, default to "backdrop"
    backdropPath.append(backdropName[0] ? backdropName : "backdrop").append(".bmp");

    backdrop = texLoader.loadTexture(backdropPath);

    // the rest is usually 0
#ifndef NDEBUG
    auto ptr = header + 14 + strlen(backdropName);
    for(; ptr < header + sizeof(ptr); ptr++)
        assert(*ptr == 0);
#endif

    // type of object in each tile?
    // might be the preview image?
    // empty tile=0, scenery=2, (non-rail)building=3, track=5, road=6, footpath=7
    delete[] tileObjectType;
    tileObjectType = new uint8_t[width * height];

    if(file.read(reinterpret_cast<char *>(tileObjectType), width * height).gcount() != width * height)
    {
        std::cerr << "Failed to read tile type data for " << path << "\n";
        return false;
    }

    // next is object data
    objects.clear();
    objects.reserve(numObjects);

    std::vector<size_t> depots;

    for(uint32_t i = 0; i < numObjects; i++)
    {
        uint8_t objectData[0x80];

        if(file.read(reinterpret_cast<char *>(objectData), sizeof(objectData)).gcount() != sizeof(objectData))
        {
            std::cerr << "Failed to read object " << i << " in " << path << "\n";
            return false;
        }

        uint16_t objectId = objectData[0] | objectData[1] << 8;
        uint16_t objectX = objectData[2] | objectData[3] << 8;
        uint16_t objectY = objectData[4] | objectData[5] << 8;

        // 6-7 are probably padding to align the oversized frameset index

        int framesetIndex = objectData[8] | objectData[9] << 8 | objectData[10] << 16 | objectData[11] << 24; // this is waaay bigger than it needs to be

        // 4 bytes unknown?

        char *objectName = reinterpret_cast<char *>(objectData + 16);

        auto &object = addObject(objectId, objectX, objectY, objectName);

        object.setAnimation(framesetIndex);

        bool hasUnk = false;

        for(int j = 6; j < 16; j++)
        {
            if((j < 8 || j >= 12) && objectData[j])
            {
                if(!hasUnk)
                    std::cout << "object " << objectId << " (\"" << objectName << "\") at " << objectX << ", " << objectY << std::hex;

                std::cout << " unk" << j << "=" << int(objectData[j]);
                hasUnk = true;
            }
        }

        if(hasUnk)
            std::cout << std::dec << std::endl;

        // up to 5 minifigs
        auto minifigData = objectData + 0x1C;
        for(int j = 0; j < 5; j++, minifigData += 20)
        {
            uint32_t minifigId = minifigData[0] | minifigData[1] << 8 | minifigData[2] << 16 | minifigData[3] << 24;

            // 4 bytes unknown (possibly uninitialised data?)
            char *minifigName = reinterpret_cast<char *>(minifigData + 8);

            object.addMinifig(Minifig{minifigId, minifigName});

            if(minifigId)
            {
                std::cout << "\tminifig " << minifigId << " \"" << minifigName << "\" (unk " << std::hex
                          << int(minifigData[4]) << ", " << int(minifigData[5]) << ", "
                          << int(minifigData[6]) << ", " << int(minifigData[7]) << std::dec << ")\n";
            }
        }

        // collect depots for placing trains
        if(object.getData() && object.getData()->specialType == ObjectData::SpecialType::Depot)
            depots.push_back(objects.size() - 1);
    }

    // TODO: shuffle depots?
    size_t depotIndex = 0;

    // trains
    for(uint32_t i = 0; i < numTrains; i++)
    {
        uint8_t trainData[44];

        if(file.read(reinterpret_cast<char *>(trainData), sizeof(trainData)).gcount() != sizeof(trainData))
        {
            std::cerr << "Failed to read train " << i << " in " << path << "\n";
            return false;
        }

        // first we have the object ids for the engine and carriages
        auto ids = reinterpret_cast<uint32_t *>(trainData);
        // then another value related to each
        // for engine: 1 = not steam, 2 = steam ?
        // for carriage: 2 = passenger, 3 = cargo, 4 = mail ?
        auto type = reinterpret_cast<uint32_t *>(trainData + 16);
        // then a name
        auto name = reinterpret_cast<char *>(trainData + 32);

        std::cout << "\ttrain " << name;

        for(int j = 0; j < 4; j++)
        {
            if(ids[j])
                std::cout << " " << ids[j] << "(" << type[j] << ")";
        }

        std::cout << std::endl;

        Train train(*this, ids[0], name);

        // carriages
        for(int j = 1; j < 4; j++)
        {
            if(ids[j])
                train.addCarriage(ids[j]);
        }

        // assign to empty depot
        // TODO: if not enough depots, trains need to leave the depot immediately
        if(!depots.empty())
        {
            train.placeInObject(objects[depots[depotIndex]]);
            depotIndex = (depotIndex + 1) % depots.size();
        }

        trains.emplace_back(std::move(train));
    }

    clampScroll();

    applyLoadEasterEggs();

    // TODO: this should happen when closing the toybox
    applyInsertEasterEggs();

    return true;
}

void World::update(uint32_t deltaMs, SoundMixer &sound)
{
    for(auto &object : objects)
        object.update(deltaMs, sound);

    for(auto &train : trains)
        train.update(deltaMs, sound);

    updateTimeEasterEggs(deltaMs);

    //remove dead objects
    objects.erase(std::remove_if(objects.begin(), objects.end(), [](auto &obj){return obj.getId() == 0xFFFF;}), objects.end());
}

void World::handleEvent(SDL_Event &event)
{
    switch(event.type)
    {
        case SDL_MOUSEWHEEL:
        {
            if(event.wheel.y != 0)
            {
                int dir = event.wheel.y < 0 ? -1 : 1;
                if(event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED)
                    dir *= -1;

                auto oldZoom = zoom;
            
                zoom += event.wheel.y;

                // clamp to a possibly sensible range
                zoom = std::min(4.0f, std::max(1.0f, zoom));

                // adjust scroll
                // need to convert coords for hidpi
                auto renderer = SDL_GetRenderer(SDL_GetWindowFromID(event.wheel.windowID));
                float logMouseX, logMouseY;
                SDL_RenderWindowToLogical(renderer, event.wheel.mouseX, event.wheel.mouseY, &logMouseX, &logMouseY);

                // convert to world coord
                float mouseWorldX = (logMouseX + scrollX) / oldZoom;
                float mouseWorldY = (logMouseY + scrollY) / oldZoom;

                // convert back to screen using the new zoom
                float newMouseX = mouseWorldX * zoom - scrollX;
                float newMouseY = mouseWorldY * zoom - scrollY;

                // compensate for the difference
                scrollX += (newMouseX - logMouseX);
                scrollY += (newMouseY - logMouseY);

                clampScroll();
            }
            break;
        }

        case SDL_KEYUP:
        {
            // some basic scrolling
            // TODO: mouse scrolling?
            // TODO: smooth scroll/zoom
            switch(event.key.keysym.scancode)
            {
                case SDL_SCANCODE_W:
                case SDL_SCANCODE_UP:
                    scrollY -= tileSize;
                    clampScroll();
                    break;

                case SDL_SCANCODE_S:
                case SDL_SCANCODE_DOWN:
                    scrollY += tileSize;
                    clampScroll();
                    break;

                case SDL_SCANCODE_A:
                case SDL_SCANCODE_LEFT:
                    scrollX -= tileSize;
                    clampScroll();
                    break;

                case SDL_SCANCODE_D:
                case SDL_SCANCODE_RIGHT:
                    scrollX += tileSize;
                    clampScroll();
                    break;

                default:
                    break;
            }
            break;
        }
    }
}

void World::render(SDL_Renderer *renderer)
{
    // set clipping
    SDL_Rect clip{
        -scrollX, -scrollY,
        static_cast<int>(width * tileSize * zoom),
        static_cast<int>(height * tileSize * zoom)
    };

    SDL_Rect oldClip;
    SDL_RenderGetClipRect(renderer, &oldClip);

    SDL_RenderSetClipRect(renderer, &clip);

    if(backdrop)
    {
        // TODO: repeat/scale?
        SDL_Rect r = {-scrollX, -scrollY, 0, 0};
        SDL_QueryTexture(backdrop.get(), nullptr, nullptr, &r.w, &r.h);
        r.w *= zoom;
        r.h *= zoom;
        SDL_RenderCopy(renderer, backdrop.get(), nullptr, &r);
    }

    for(auto &object : objects)
        object.render(renderer, scrollX, scrollY, 1, zoom);

    // TODO: minifigs

    for(auto &train : trains)
        train.render(renderer, scrollX, scrollY, zoom);

    for(int z = 2; z < 7; z++)
    {
        for(auto &object : objects)
            object.render(renderer, scrollX, scrollY, z, zoom);
    }

    SDL_RenderSetClipRect(renderer, &oldClip);
}

void World::setWindowSize(unsigned int windowWidth, unsigned int windowHeight)
{
    this->windowWidth = windowWidth;
    this->windowHeight = windowHeight;

    clampScroll();
}

ObjectDataStore &World::getObjectDataStore()
{
    return objectDataStore;
}

Object World::createObject(uint16_t id, uint16_t x, uint16_t y, std::string name)
{
    // attempt to get texture
    auto texture = texLoader.loadTexture(id);

    // load object data
    auto data = objectDataStore.getObject(id);

    // set alpha if semi-transparent
    // assumes this object is the only user of the image
    if(data && texture && data->semiTransparent)
        SDL_SetTextureAlphaMod(texture.get(), 127);

    return {id, x, y, name, texture, data};
}

Object &World::addObject(uint16_t id, uint16_t x, uint16_t y, std::string name)
{
    return objects.emplace_back(std::move(createObject(id, x, y, name)));
}

Object *World::getObjectAt(unsigned int x, unsigned int y)
{
    // TODO: add some kind of lookup table for this
    for(auto &object : objects)
    {
        auto objectData = object.getData();
        if(!objectData)
            continue;

        auto objectX = object.getX();
        auto objectY = object.getY();

        // not something we should check
        if(objectX < 0 || objectY < 0 || !objectData->physSizeX)
            continue;

        // check physical coords
        unsigned int yAdjust = objectData->bitmapSizeY - objectData->physSizeY;

        if(x < static_cast<unsigned int>(objectX) || y < static_cast<unsigned int>(objectY) + yAdjust)
            continue;

        if(x >= objectX + objectData->physSizeX || y >= objectY + yAdjust + objectData->physSizeY)
            continue;

        // check occupancy
        int relX = x - objectX;
        int relY = y - (objectY + yAdjust);

        if(objectData->physicalOccupancy[relX + relY * objectData->physSizeX])
            return &object;
    }

    return nullptr;
}

std::vector<Object *> World::getTunnels(bool shuffled)
{
    std::vector<Object *> ret;

    for(auto &object : objects)
    {
        auto objectData = object.getData();
        if(!objectData)
            continue;
 
        if(objectData->specialType == ObjectData::SpecialType::Tunnel)
            ret.push_back(&object);
    }

    if(shuffled)
        std::shuffle(ret.begin(), ret.end(), randomGen);

    return ret;
}

// load the "global" easter eggs from EE.INI
void World::loadEasterEggs()
{
    // parsing helpers
    auto skipWhitespace = [](const char *&ptr, const char *end)
    {
        while(ptr != end && std::isspace(*ptr)) ptr++;
    };

    auto parseInt = [&skipWhitespace](int &v, const char *&ptr, const char *end)
    {
        skipWhitespace(ptr, end);

        auto res = std::from_chars(ptr, end, v);

        ptr = res.ptr;
        skipWhitespace(ptr, end);

        return res;
    };

    auto stream = fileLoader.openResourceFile("EE.INI");
    if(!stream)
    {
        std::cerr << "Could not open EE.INI!\n";
        return;
    }

    IniFile ini(*stream.get());

    auto timeEventsSection = ini.getSection("TimeEvents");

    if(timeEventsSection)
    {
        // these create objects at random intervals
        for(auto &event : *timeEventsSection)
        {
            // keys don't matter
            auto &val = event.second;

            // value is comma separated
            auto ptr = val.data();
            auto end = val.data() + val.length();

            auto error = [&ptr, &val]()
            {
                std::cerr << "Failed to parse time event " << val << " at offset " << ptrdiff_t(ptr - val.data()) << "\n";
            };

            int values[13];

            // first 11 ints
            for(int i = 0; i < 11; i++)
            {
                if(parseInt(values[i], ptr, end).ec != std::errc{} || *ptr != ',')
                {
                    error();
                    continue;
                }
                ptr++;
            }

            // type (always P or S)
            skipWhitespace(ptr, end);
            char type = *ptr++;
            if(*ptr != ',')
            {
                error();
                continue;
            }
            ptr++;

            // x/y
            for(int i = 11; i < 13; i++)
            {
                if(parseInt(values[i], ptr, end).ec != std::errc{} || (ptr != end && *ptr != ','))
                {
                    error();
                    continue;
                }
                ptr++;
            }

            // assing the values
            TimeEvent timeEvent = {};

            switch(type)
            {
                case 'P':
                    timeEvent.type = ObjectMotion::Port;
                    break;

                case 'S':
                    timeEvent.type = ObjectMotion::Starboard;
                    break;

                default:
                    std::cerr << "Unhandled TimeEvent type: " << type << "\n";
            }

            timeEvent.startDay = values[0];
            timeEvent.startMonth = values[1];

            timeEvent.endDay = values[2];
            timeEvent.endMonth = values[3];
    
            timeEvent.startHour = values[4];
            timeEvent.startMin = values[5];

            timeEvent.endHour = values[6];
            timeEvent.endMin = values[7];

            timeEvent.resId = values[8];
            timeEvent.resFrameset = values[9];

            timeEvent.periodMax = values[10];

            timeEvent.x = values[11];
            timeEvent.y = values[12];

            // init timer
            std::uniform_int_distribution distribution(10, timeEvent.periodMax);
            timeEvent.periodTimer = distribution(randomGen) * 1000;
    
            timeEvents.emplace_back(timeEvent);
        }
    }

    auto loadEventsSection = ini.getSection("LoadEvents");

    if(loadEventsSection)
    {
        // these replace objects at load time on certain days
        for(auto &event : *loadEventsSection)
        {
            // keys still don't matter
            auto &val = event.second;

            // value is comma separated
            auto ptr = val.data();
            auto end = val.data() + val.length();

            auto error = [&ptr, &val]()
            {
                std::cerr << "Failed to parse load event " << val << " at offset " << ptrdiff_t(ptr - val.data()) << "\n";
            };

            int values[6];

            // it's all ints
            for(int i = 0; i < 6; i++)
            {
                if(parseInt(values[i], ptr, end).ec != std::errc{} || (ptr != end && *ptr != ','))
                {
                    error();
                    continue;
                }
                ptr++;
            }

            LoadEvent loadEvent = {};

            loadEvent.startDay = values[0];
            loadEvent.startMonth = values[1];

            loadEvent.endDay = values[2];
            loadEvent.endMonth = values[3];

            loadEvent.oldId = values[4];
            loadEvent.newId = values[5];

            loadEvents.emplace_back(loadEvent);
        }
    }
}

void World::clampScroll()
{
    unsigned int worldWidth = width * tileSize * zoom;
    unsigned int worldHeight = height * tileSize * zoom;

    auto clampDim = [](unsigned int worldDim, unsigned int windowDim, int &scroll)
    {
        if(worldDim < windowDim)
        {
            // world smaller than display, center it
            scroll = static_cast<int>(worldDim - windowDim) / 2;
        }
        else
        {
            // world is larger, clamp to edges
            scroll = std::max(0, std::min(static_cast<int>(worldDim - windowDim), scroll));
        }
    };

    clampDim(worldWidth, windowWidth, scrollX);
    clampDim(worldHeight, windowHeight, scrollY);
}

void World::applyLoadEasterEggs()
{
    std::map<int, int> idMap;

    auto time = std::time(nullptr);
    auto tm = std::localtime(&time);

    int month = tm->tm_mon + 1;
    int day = tm->tm_mday;

    // annoyingly, we don't have the id of the backdrop
    // only its name... which might be in uppercase and won't match the value in the string table
    auto lowerBackdropPath = backdropPath;
    for(auto &c : lowerBackdropPath)
        c = std::tolower(c);

    for(auto &event : loadEvents)
    {
        // outside months
        if(month < event.startMonth || month > event.endMonth)
            continue;

        // before start date
        if(day < event.startDay && month == event.startMonth)
            continue;

        // after end date
        if(day > event.endDay && month == event.endMonth)
            continue;

        idMap.emplace(event.oldId, event.newId);

        // lookup the value to see if we need to change the backdrop
        auto filename = fileLoader.lookupId(event.oldId, ".bmp");
        if(filename && filename.value() == lowerBackdropPath)
        {
            auto newPath = fileLoader.lookupId(event.newId, ".bmp");
            if(newPath)
            {
                auto newTex = texLoader.loadTexture(newPath.value());
                if(newTex)
                    backdrop = newTex;
            }
        }
    }

    std::cout << idMap.size() << " load events for date " << day << "/" << month << std::endl;

    for(auto &object : objects)
    {
        // this doesn't have all the logic that insert has, but these usually don't change the size
        auto it = idMap.find(object.getId());

        if(it != idMap.end())
        {
            object.replace(it->second, texLoader.loadTexture(it->second), objectDataStore.getObject(it->second));

            object.setDefaultAnimation(); // saved animation may not exist in the new object
        }
    }

    // TODO: these can affect minifigs
}

void World::applyInsertEasterEggs()
{
    for(size_t i = 0; i < objects.size(); i++)
    {
        auto &object = objects[i];
        auto objectData = object.getData();

        if(!objectData)
            continue;

        for(auto &easterEgg : objectData->easterEggs)
        {
            if(easterEgg.type != ObjectData::EasterEggType::Insert)
                continue;

            // check ids
            if(!easterEgg.ids.empty())
            {
                std::vector<std::tuple<unsigned int, unsigned int>> toCheck;

                int yOffset = objectData->bitmapSizeY - objectData->physSizeY;

                auto objectX = object.getX();
                auto objectY = object.getY();

                // check across the top
                for(unsigned int x = 0; x < objectData->physSizeX; x++)
                    toCheck.emplace_back(objectX + x, objectY + yOffset - 1);

                // ... down the right side
                for(int y = -1; y < static_cast<int>(objectData->physSizeY); y++)
                    toCheck.emplace_back(objectX + objectData->physSizeX, objectY + yOffset + y);

                // ... back across the bottom
                for(int x = objectData->physSizeX; x >= -1; x--)
                    toCheck.emplace_back(objectX + x, objectY + yOffset + objectData->physSizeY);

                // ... and up the left size
                // there is a bug here, the first tile overlaps the previous side
                // this results in the nessie easter egg working without the top-left flower
                for(int y = objectData->physSizeY; y >= 0; y--)
                    toCheck.emplace_back(objectX - 1, objectY + yOffset + y);

                bool match = true;

                for(size_t i = 0; i < easterEgg.ids.size(); i++)
                {
                    if(easterEgg.ids[i] == -1)
                        continue;

                    int x = std::get<0>(toCheck[i]);
                    int y = std::get<1>(toCheck[i]);

                    auto objAt = getObjectAt(x, y);

                    if(!objAt || objAt->getId() != easterEgg.ids[i])
                    {
                        match = false;
                        break;
                    }
                }

                if(!match)
                    continue;
            }

            int oldYAdjust = objectData->bitmapSizeY - objectData->physSizeY;

            // update object
            if(easterEgg.changeId > 0)
            {
                // minifig data is reused as offset
                int xOff = easterEgg.minifigFrameset;
                int yOff = easterEgg.minifigTime;

                auto newX = object.getX() + xOff;
                auto newY = object.getY() + yOff;
                auto newData = objectDataStore.getObject(easterEgg.changeId);

                // need to adjust if the new object has different phys/bitmap height difference (fountain -> big fountain)
                int yAdjust = newData->bitmapSizeY - newData->physSizeY;
                newY += oldYAdjust - yAdjust;

                // remove overlapping objects
                for(unsigned int y = 0; y < newData->physSizeY; y++)
                {
                    for(unsigned int x = 0; x < newData->physSizeX; x++)
                    {
                        auto overlapObj = getObjectAt(newX + x, newY + y + yAdjust);
                        // set an invalid id, we'll remove them later
                        if(overlapObj && overlapObj != &object)
                            overlapObj->replace(0xFFFF);
                    }
                }

                object.setPosition(newX, newY);
                object.replace(easterEgg.changeId, texLoader.loadTexture(easterEgg.changeId), newData);
            }

            if(easterEgg.changeFrameset != -1)
                object.setAnimation(easterEgg.changeFrameset);

            // minifig values are used to store an offset, so skip those

            // create new object
            if(easterEgg.newId > 0)
            {
                int newX = object.getX() + easterEgg.x;
                int newY = object.getY() + easterEgg.y;

                // adjust y for physical vs bitmap size
                newY += oldYAdjust;

                auto &newObject = addObject(easterEgg.newId, newX, newY, "");
                newObject.setAnimation(easterEgg.newFrameset);
                auto newObjectData = newObject.getData();

                if(newObjectData && newObjectData->bitmapOccupancy.empty())
                {
                    // objects without occupancy seem to use pixel offsets
                    // (rainbow)
                    newObject.setPixelPos(objects[i].getX() * tileSize + easterEgg.x, (objects[i].getY() + oldYAdjust) * tileSize + easterEgg.y);
                }
            }
        }
    }

    // clean up removed objects
    objects.erase(std::remove_if(objects.begin(), objects.end(), [](auto &obj){return obj.getId() == 0xFFFF;}), objects.end());
}

void World::updateTimeEasterEggs(uint32_t deltaMs)
{
    auto time = std::time(nullptr);
    auto tm = std::localtime(&time);
    
    int month = tm->tm_mon + 1;
    int day = tm->tm_mday;

    int hour = tm->tm_hour;
    int min = tm->tm_min;

    for(auto &event : timeEvents)
    {
        event.periodTimer -= deltaMs;

        if(event.periodTimer < 0)
        {
            // restart timer
            std::uniform_int_distribution distribution(10, event.periodMax);
            event.periodTimer += distribution(randomGen) * 1000;

            // outside months (month == 0 is "any")
            if(event.startMonth && month < event.startMonth)
                continue;

            if(event.endMonth && month > event.endMonth)
                continue;

            // before start date
            if(day < event.startDay && (!event.startMonth || month == event.startMonth))
                continue;

            // after end date
            if(day > event.endDay && (!event.endMonth || month == event.endMonth))
                continue;

            // check time
            // all the events in EE.INI have a time range of 01:00 - 23:00, so nothing happens for two hours at night...
            if(hour < event.startHour || hour > event.endHour)
                continue;

            if(min < event.startMin && hour == event.startHour)
                continue;

            if(min > event.endMin && hour == event.endHour)
                continue;

            // finally create the object

            // TODO: cant/must have data

            int x = event.x;
            int y = event.y;

            bool xNonZero = x != 0;

            // random pos if -1
            if(x == -1)
                x = std::uniform_int_distribution<int>(0, width * tileSize)(randomGen);

            if(y == -1)
                y = std::uniform_int_distribution<int>(0, height * tileSize)(randomGen);

            auto &object = addObject(event.resId, x, y, "");
            object.setAnimation(event.resFrameset);

            // set up movement
            int targetX, targetY;
            int velX = 0, velY = 0;
            int speed = 35 * std::uniform_int_distribution(1, 3)(randomGen); // rough pixels/s from a lot of in-game cloud watching

            auto objectSize = object.getFrameSize();

            switch(event.type)
            {
                case ObjectMotion::None:
                    break;
                
                case ObjectMotion::Port:
                    targetY = y;
                    // if x was zero there is no target so scroll to the opposite side
                    targetX = xNonZero ? x : -std::get<0>(objectSize);
                    object.setX(width * tileSize);

                    velX = -speed;
                    break;

                case ObjectMotion::Starboard:
                    targetY = y;
                    targetX = xNonZero ? x : width * tileSize;
                    object.setX(-std::get<0>(objectSize));

                    velX = speed;
                    break;
            }

            object.setPixelPos(object.getX(), object.getY());

            // reverse back to start if x was specified
            object.setTargetPos(targetX, targetY, velX, velY, xNonZero);
        }
    }
}