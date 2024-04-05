#include <algorithm>

#include "Train.hpp"

#include "World.hpp"

static const int rearWheelDist = 22;
static const int nextCarriageDist = 38;

Train::Train(World &world, uint16_t engineId, std::string name) : world(world), engine(*this, std::move(world.addObject(engineId, 0, 0, name)))
{
    speed = 35; // TODO: min/max speed from .dat
}

Train::Train(Train &&other) : world(other.world), engine(*this, std::move(other.engine.getObject()))
{
    speed = other.speed;
    
    for(auto &carriage : other.carriages)
    {
        carriages.emplace_back(*this, std::move(carriage.getObject()));
        carriages.back().copyPosition(carriage);
    }

    engine.copyPosition(other.engine);
}

void Train::update(uint32_t deltaMs)
{
    if(!engine.update(deltaMs, speed))
        return;

    bool first = true;

    for(auto it = carriages.begin(); it != carriages.end(); ++it)
    {
        if(!it->update(deltaMs, first ? engine : *(it - 1)))
            break;

        first = false;
    }

}

void Train::render(SDL_Renderer *renderer, int scrollX, int scrollY, float zoom)
{
    std::vector<Part *> parts;
    parts.reserve(carriages.size() + 1);

    parts.push_back(&engine);

    for(auto &carriage : carriages)
    {
        if(carriage.getValidPos())
            parts.push_back(&carriage);
    }

    // z-order-ish
    std::sort(parts.begin(), parts.end(), [](auto &a, auto &b){return a->getObject().getPixelY() < b->getObject().getPixelY();});

    for(auto &part : parts)
        part->getObject().render(renderer, scrollX, scrollY, 6, zoom);
}

void Train::placeInObject(Object &obj)
{
    engine.placeInObject(obj);

    for(auto &carriage : carriages)
        carriage.placeInObject(obj);

    enterObject(engine, obj);
}

void Train::enterObject(Part &part, Object &obj)
{
    // TODO: other end if reversing
    bool isFirst = &part == &engine;

    // close crossing on train entering
    // TODO: should do before train reaches
    // TODO: make sure there are no minifigs
    if(isFirst && obj.getData()->specialType == ObjectData::SpecialType::LevelCrossing)
    {
        // animation name is inconsistent
        if(!obj.setAnimation("closed"))
            obj.setAnimation("default");
    }
}

void Train::leaveObject(Part &part, Object &obj)
{
    bool isLast = carriages.empty() || &part == &carriages.back();

    // re-open crossing after train leaves
    // TODO: delay?
    if(isLast && obj.getData()->specialType == ObjectData::SpecialType::LevelCrossing)
        obj.setAnimation("open");
}

Train::Part::Part(Train &parent, Object &&object) : parent(parent), object(std::move(object))
{
}

bool Train::Part::update(uint32_t deltaMs, int speed)
{
    object.update(deltaMs);

    // find the object we're currently on
    auto obj = parent.world.getObjectAt(curObjectCoord.x, curObjectCoord.y);

    if(!obj)
    {
        validPos = false;
        return false; // uh oh
    }

    auto objData = obj->getData();

    if(!objData || objData->coords.empty())
    {
        validPos = false;
        return false; // how did we get here?
    }

    // advance
    int dir = curObjectCoord.reverse ? -1 : 1;

    objectCoordPos += (deltaMs / 1000.0f) * speed * dir;

    int coordIndex = std::floor(objectCoordPos);

    auto &coords = curObjectCoord.alternate ? objData->altCoords : objData->coords;

    if(coordIndex < 0 || coordIndex >= static_cast<int>(coords.size() - 1))
    {
        // moving to next object
        if(enterNextObject(obj, objData))
            coordIndex = std::floor(objectCoordPos);
        else
            return false;
    }

    // get coords again as object might have changed
    auto &finalCoords = curObjectCoord.alternate ? objData->altCoords : objData->coords;
    float frac = objectCoordPos - coordIndex;

    int px0, py0, px1, py1;
    getWorldCoord(finalCoords[coordIndex], px0, py0, *obj);
    getWorldCoord(finalCoords[coordIndex + 1], px1, py1, *obj);

    float newX = px0 + (px1 - px0) * frac;
    float newY = py0 + (py1 - py0) * frac;

    setPosition(obj, objData, newX, newY);
    return validPos;
}

bool Train::Part::update(uint32_t deltaMs, Part &prevPart)
{
    object.update(deltaMs);

    // get position from prev part
    int coordIndex;
    auto pos = prevPart.getNextCarriagePos(coordIndex, objectCoordPos);

    float newX = std::get<0>(pos);
    float newY = std::get<1>(pos);

    auto newCoordMeta = prevPart.getCoordMeta(coordIndex);

    if(!newCoordMeta)
    {
        validPos = false;
        return false;
    }

    // find the object we're currently on
    auto obj = parent.world.getObjectAt(newCoordMeta->x, newCoordMeta->y);

    if(!obj)
    {
        validPos = false;
        return false;
    }

    auto objData = obj->getData();

    if(!objData || objData->coords.empty())
        return false; // how did we get here?

    if(newCoordMeta->x != curObjectCoord.x || newCoordMeta->y != curObjectCoord.y)
    {
        // copy info for looking behind later
        for(int i = 2; i > 0; i--)
            prevObjectCoord[i] = prevObjectCoord[i - 1];

        prevObjectCoord[0] = curObjectCoord;

        parent.enterObject(*this, *obj);

        // set new obj
        curObjectCoord = *newCoordMeta;
    }

    setPosition(obj, objData, newX, newY);
    return validPos;
}

void Train::Part::placeInObject(Object &inObj)
{
    auto data = inObj.getData();

    if(!data || data->coords.empty())
        return;

    auto coord = data->coords[0];

    int px, py;
    getWorldCoord(coord, px, py, inObj);
    object.setPixelPos(px, py);

    objectCoordPos = 0.0f;
    curObjectCoord.reverse = false;
    curObjectCoord.alternate = false;
    curObjectCoord.x = inObj.getX();
    curObjectCoord.y = inObj.getY() + data->bitmapSizeY - data->physSizeY;

    // flip direction so that we're always exiting a depot/tunnel
    if((data->specialType == ObjectData::SpecialType::Depot || data->specialType == ObjectData::SpecialType::Tunnel) &&
       (data->specialSide == ObjectData::SpecialSide::Bottom || data->specialSide == ObjectData::SpecialSide::Left))
    {
        curObjectCoord.reverse = true;
        objectCoordPos = data->coords.size() - 2;
    }

    // clear prev objects
    for(int i = 0; i < 3; i++)
    {
        prevObjectCoord[i].x = -1;
        prevObjectCoord[i].y = -1;
    }
}

void Train::Part::getWorldCoord(const std::tuple<int, int> &coord, int &x, int &y, const Object &obj)
{
    x = obj.getX() * World::tileSize + std::get<0>(coord);
    y = obj.getY() * World::tileSize + std::get<1>(coord);
}

void Train::Part::copyPosition(const Part &other)
{
    objectCoordPos = other.objectCoordPos;
    curObjectCoord = other.curObjectCoord;

    for(int i = 0; i < 3; i++)
        prevObjectCoord[i] = other.prevObjectCoord[i];
}

std::tuple<float, float> Train::Part::getNextCarriagePos(int &finalCoordIndex, float &finalCoordPos)
{
    auto obj = parent.world.getObjectAt(curObjectCoord.x, curObjectCoord.y);

    if(!obj)
        return {0.0f, 0.0f}; // uh oh

    auto objData = obj->getData();

    if(!objData || objData->coords.empty())
        return {0.0f, 0.0f};

    int lastUsed;
    return lookBehind(nextCarriageDist, obj, objData, lastUsed, finalCoordIndex, finalCoordPos);
}

Object &Train::Part::getObject()
{
    return object;
}

bool Train::Part::getValidPos() const
{
    return validPos;
}

// helper to position and orientation
void Train::Part::setPosition(Object *obj, const ObjectData *objData, float newX, float newY)
{
    // use the coord for the front of the train
    // try to find the back
    int lastUsedObj;
    int rearCoordIndex;
    float rearCoordPos;
    auto rearCoord = lookBehind(rearWheelDist, obj, objData, lastUsedObj, rearCoordIndex, rearCoordPos);

    // leave objects
    for(int i = 2; i > lastUsedObj; i--)
    {
        if(prevObjectCoord[i].x != -1)
        {
            auto prevObj = parent.world.getObjectAt(prevObjectCoord[i].x, prevObjectCoord[i].y);
            if(prevObj)
                parent.leaveObject(*this, *prevObj);

            prevObjectCoord[i].x = prevObjectCoord[i].y = -1;
        }
    }

    float rearX = std::get<0>(rearCoord);
    float rearY = std::get<1>(rearCoord);

    // orient train
    float angle = std::atan2(rearX - newX, rearY - newY);
    int frame = std::round(angle * 64.0f / M_PI);
    
    frame = (frame + 96) % 128;

    object.setAnimationFrame(frame);

    // adjust pos using the train data
    auto &trainData = parent.world.getObjectDataStore().getTrainData();

    if(frame < static_cast<int>(trainData.size()))
    {
        auto engineData = object.getData();
        
        // compensate for rendering using hotspot first
        newX = newX + engineData->hotspotX - std::get<0>(trainData[frame]);
        newY = newY + engineData->hotspotY - std::get<1>(trainData[frame]);
    }

    object.setPixelPos(newX, newY);

    validPos = rearCoordIndex != -1 || std::abs(rearCoordPos - objectCoordPos) >= (rearWheelDist - 1);
}

bool Train::Part::enterNextObject(Object *&obj, const ObjectData *&objData)
{
    auto &coords = curObjectCoord.alternate ? objData->altCoords : objData->coords;

    auto finalCoord = curObjectCoord.reverse ? coords.front() : coords.back();

    int x, y;
    getWorldCoord(finalCoord, x, y, *obj);

    auto newObj = parent.world.getObjectAt(x / World::tileSize, y / World::tileSize);

    if(!newObj || newObj == obj)
        return false;

    // make coord relative to new object
    x -= newObj->getX() * World::tileSize;
    y -= newObj->getY() * World::tileSize;

    auto newObjData = newObj->getData();

    if(!newObjData || newObjData->coords.empty())
        return false; // not again...

    // copy info for looking behind later
    for(int i = 2; i > 0; i--)
        prevObjectCoord[i] = prevObjectCoord[i - 1];

    prevObjectCoord[0] = curObjectCoord;

    // figure out which coord path we're on, and at which end
    // coords overlap so the last coord in the prev object is the second in the new one
    bool hasAlt = !newObjData->altCoords.empty();

    bool matchesCoords = newObjData->coords[1] == std::make_tuple(x, y) || newObjData->coords[newObjData->coords.size() - 2] == std::make_tuple(x, y);
    bool matchesAltCoords = false;

    if(hasAlt)
        matchesAltCoords = newObjData->altCoords[1] == std::make_tuple(x, y) || newObjData->altCoords[newObjData->altCoords.size() - 2] == std::make_tuple(x, y);

    if(newObjData->specialType == ObjectData::SpecialType::Points)
    {
        auto fs = newObj->getCurrentFrameset();
        bool open = fs && fs->name == "open";

        // pick the right path for points
        if(matchesCoords && matchesAltCoords)
        {
            if(open)
                matchesCoords = false;
            else
                matchesAltCoords = false;
        }
        else
        {
            // may need to switch
            if(open != matchesAltCoords)
                newObj->setAnimation(open ? "closed" : "open");
        }
    }

    curObjectCoord.alternate = matchesAltCoords;

    auto &newCoords = curObjectCoord.alternate ? newObjData->altCoords : newObjData->coords;

    bool newRev = newCoords[newCoords.size() - 2] == std::make_tuple(x, y);

    if(!curObjectCoord.reverse && !newRev)
        objectCoordPos -= (coords.size() - 2);
    else if(curObjectCoord.reverse && !newRev) // backwards -> forwards
        objectCoordPos = objectCoordPos * -1.0f + 1.0f;
    else if(!curObjectCoord.reverse) // forwards -> backwards
    {
        objectCoordPos -= (coords.size() - 1);
        objectCoordPos = (newCoords.size() - 2) - objectCoordPos;
    }
    else // backwards -> backwards
        objectCoordPos = (newCoords.size() - 2) + objectCoordPos;

    curObjectCoord.reverse = newRev;

    // enter new object
    parent.enterObject(*this, *newObj);

    // use new object
    obj = newObj;
    objData = newObjData;

    // add offset to make sure we point to a tile that has occupancy
    curObjectCoord.x = obj->getX() + x / World::tileSize;
    curObjectCoord.y = obj->getY() + y / World::tileSize;

    return true;
}

Train::Part::CoordMeta *Train::Part::getCoordMeta(int index)
{
    if(index == -1)
        return &curObjectCoord;

    if(index < 3)
        return &prevObjectCoord[index];

    // out of bounds
    return nullptr;
}

std::tuple<float, float> Train::Part::lookBehind(int dist, const Object *obj, const ObjectData *objData, int &lastUsedObj, int &finalObjectIndex, float &finalCoordPos)
{
    auto &finalCoords = curObjectCoord.alternate ? objData->altCoords : objData->coords;

    int coordIndex = std::floor(objectCoordPos);
    float frac = objectCoordPos - coordIndex;

    int rearCoordIndex = coordIndex + (curObjectCoord.reverse ? dist : -dist);

    std::tuple<int, int> rearCoord0, rearCoord1;
    auto rearObj = obj;

    lastUsedObj = -1;
    finalObjectIndex = -1;

    if(rearCoordIndex < 0 || rearCoordIndex >= static_cast<int>(finalCoords.size() - 1))
    {
        auto rearObjData = objData;
        int rearCoordMax = finalCoords.size() - 1;
        bool prevAlt = curObjectCoord.alternate;

        for(int i = 0; i < 3; i++)
        {
            if(prevObjectCoord[i].x == -1)
                break;

            // look back at prev object
            if(rearCoordIndex > 0)
                rearCoordIndex = rearCoordIndex - (rearCoordMax - 1);
            else
                rearCoordIndex = -rearCoordIndex;

            // this should exist, we were just there
            rearObj = parent.world.getObjectAt(prevObjectCoord[i].x, prevObjectCoord[i].y);
            rearObjData = rearObj->getData();

            auto &prevCoords = prevObjectCoord[i].alternate ? rearObjData->altCoords : rearObjData->coords;

            rearCoordIndex = prevObjectCoord[i].reverse ? rearCoordIndex : prevCoords.size() - (rearCoordIndex + 2);
            rearCoordMax = prevCoords.size() - 1;
            prevAlt = prevObjectCoord[i].alternate;

            if(rearCoordIndex >= 0 && rearCoordIndex < rearCoordMax)
            {
                lastUsedObj = finalObjectIndex = i;

                // keep a buffer for calculating next car pos
                int remainingCoords = prevObjectCoord[i].reverse ? rearCoordMax - rearCoordIndex : rearCoordIndex;

                if(remainingCoords <= (nextCarriageDist - rearWheelDist))
                    lastUsedObj++;

                rearCoord0 = prevCoords[rearCoordIndex];
                rearCoord1 = prevCoords[rearCoordIndex + 1];
                break;
            }
        }

        if(rearObj == obj || rearCoordIndex < 0 || rearCoordIndex >= rearCoordMax)
        {
            // still failed, clamp
            auto &prevCoords = prevAlt ? rearObjData->altCoords : rearObjData->coords;

            rearCoordIndex = rearCoordIndex < 0 ? 0 : rearCoordMax - 1;
            rearCoord0 = prevCoords[rearCoordIndex];
            rearCoord1 = prevCoords[rearCoordIndex + 1];
        }
    }
    else
    {
        // rear of train is in same object
        rearCoord0 = finalCoords[rearCoordIndex];
        rearCoord1 = finalCoords[rearCoordIndex + 1];

        // keep buffer
        int remainingCoords = curObjectCoord.reverse ? finalCoords.size() - 1 - rearCoordIndex : rearCoordIndex;

        if(remainingCoords <= (nextCarriageDist - rearWheelDist))
            lastUsedObj++;
    }

    int px0, py0, px1, py1;

    getWorldCoord(rearCoord0, px0, py0, *rearObj);
    getWorldCoord(rearCoord1, px1, py1, *rearObj);

    float rearX = px0 + (px1 - px0) * frac;
    float rearY = py0 + (py1 - py0) * frac;

    finalCoordPos = rearCoordIndex + frac;

    return {rearX, rearY};
}
