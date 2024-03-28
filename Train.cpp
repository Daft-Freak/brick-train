#include "Train.hpp"

#include "World.hpp"

Train::Train(World &world, uint16_t engineId, std::string name) : world(world), engine(std::move(world.addObject(engineId, 0, 0, name)))
{
    speed = 35; // TODO: min/max speed from .dat
}

void Train::update(uint32_t deltaMs)
{
    static int tf = 0;
    engine.update(deltaMs);

    tf = (tf + 1) % 128;

    engine.setAnimationFrame(tf);

    // find the object we're currently on
    auto obj = world.getObjectAt(curObjectX, curObjectY);

    if(!obj)
        return; // uh oh

    auto objData = obj->getData();

    if(!objData || objData->coords.empty())
        return; // how did we get here?

    // advance
    int dir = objectCoordReverse ? -1 : 1;

    objectCoordPos += (deltaMs / 1000.0f) * speed * dir;

    int coordIndex = std::floor(objectCoordPos);

    auto &coords = objectAltCoords ? objData->altCoords : objData->coords;

    if(coordIndex < 0 || coordIndex >= static_cast<int>(coords.size() - 1))
    {
        // moving to next object

        auto finalCoord = objectCoordReverse ? coords.front() : coords.back();

        int x, y;
        getPixelCoord(finalCoord, x, y, *obj);
    
        auto newObj = world.getObjectAt(x / World::tileSize, y / World::tileSize);

        if(!newObj)
            return;

        // make coord relative to new object
        x -= newObj->getX() * World::tileSize;
        y -= newObj->getY() * World::tileSize;

        auto newObjData = newObj->getData();

        if(!newObjData || newObjData->coords.empty())
            return; // not again...

        // figure out which coord path we're on, and at which end
        // coords overlap so the last coord in the prev object is the second in the new one
        bool hasAlt = !newObjData->altCoords.empty();

        if(hasAlt)
            objectAltCoords = newObjData->altCoords[1] == std::make_tuple(x, y) || newObjData->altCoords[newObjData->altCoords.size() - 2] == std::make_tuple(x, y);
        else
            objectAltCoords = false;

        auto &newCoords = objectAltCoords ? newObjData->altCoords : newObjData->coords;

        bool newRev = newCoords[newCoords.size() - 2] == std::make_tuple(x, y);

        if(!objectCoordReverse && !newRev)
            objectCoordPos -= (coords.size() - 2);
        else if(objectCoordReverse && !newRev) // backwards -> forwards
            objectCoordPos = objectCoordPos * -1.0f + 1.0f;
        else if(!objectCoordReverse) // forwards -> backwards
        {
            objectCoordPos -= (coords.size() - 1);
            objectCoordPos = (newCoords.size() - 2) - objectCoordPos;
        }
        else // backwards -> backwards
            objectCoordPos = (newCoords.size() - 2) + objectCoordPos;

        objectCoordReverse = newRev;

        // use new object
        obj = newObj;
        objData = newObjData;
        coordIndex = std::floor(objectCoordPos);

        // add offset to make sure we point to a tile that has occupancy
        curObjectX = obj->getX() + x / World::tileSize;
        curObjectY = obj->getY() + y / World::tileSize;

        auto newC = (objectAltCoords ? objData->altCoords : objData->coords)[coordIndex];
        int nx, ny;
        getPixelCoord(newC, nx, ny, *obj);
    }

    // get coords again as object might have changed
    auto &finalCoords = objectAltCoords ? objData->altCoords : objData->coords;
    float frac = objectCoordPos - coordIndex;

    int px0, py0, px1, py1;
    getPixelCoord(finalCoords[coordIndex], px0, py0, *obj);
    getPixelCoord(finalCoords[coordIndex + 1], px1, py1, *obj);

    float newX = px0 + (px1 - px0) * frac;
    float newY = py0 + (py1 - py0) * frac;

    engine.setPixelPos(newX, newY);
}

void Train::render(SDL_Renderer *renderer, int scrollX, int scrollY, float zoom)
{
    engine.render(renderer, scrollX, scrollY, 6, zoom);
}

void Train::placeInObject(Object &obj)
{
    auto data = obj.getData();

    if(!data || data->coords.empty())
        return;

    auto coord = data->coords[0];

    int px, py;
    getPixelCoord(coord, px, py, obj);
    engine.setPixelPos(px, py);

    objectCoordPos = 0.0f;
    objectCoordReverse = false;
    objectAltCoords = false;
    curObjectX = obj.getX();
    curObjectY = obj.getY() + data->bitmapSizeY - data->physSizeY;
}

void Train::getPixelCoord(const std::tuple<int, int> &coord, int &x, int &y, const Object &obj)
{
    x = obj.getX() * World::tileSize + std::get<0>(coord);
    y = obj.getY() * World::tileSize + std::get<1>(coord);
}
