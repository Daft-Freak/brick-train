#include "Object.hpp"

#include "World.hpp"

Object::Object(uint16_t id, uint16_t x, uint16_t y, std::string name, std::shared_ptr<SDL_Texture> texture, const ObjectData *data) : id(id), x(x), y(y), name(name), texture(texture), data(data)
{
    // set the default animation
    setDefaultAnimation();
}

void Object::update(uint32_t deltaMs)
{
    if(!data)
        return;

    if(velX || velY)
    {
        auto delta = static_cast<float>(deltaMs) / 1000.0f;
        pixelX += velX * delta;
        pixelY += velY * delta;

        bool done =  (velX > 0 && pixelX >= targetX)
                  || (velX < 0 && pixelX <= targetX)
                  || (velY > 0 && pixelY >= targetY)
                  || (velY < 0 && pixelY <= targetY);

        if(done)
        {
            if(reverse)
            {
                // go back to original pos
                targetX = x;
                targetY = y;
                velX = -velX;
                velY = -velY;
                reverse = false;
            }
            else
            {
                // mark object to be removed
                id = 0xFFFF;
            }
        }
    }

    // TODO: sounds
    if(currentAnimation != -1 && animationTimer)
    {
        animationTimer -= deltaMs;

        while(animationTimer <= 0)
        {
            auto &frameset = data->framesets[currentAnimation];

            // if start > end, play backwards
            int dir = frameset.startFrame > frameset.endFrame ? -1 : 1;

            if(frameset.splitFrames)
                dir *= 2;

            if(nextAnimation != -1)
            {
                // delayed frameset change
                currentAnimation = nextAnimation;
                currentAnimationFrame = data->framesets[currentAnimation].startFrame;
                nextAnimation = -1;
            }
            else
                currentAnimationFrame += dir;

            // check if we've reached the end
            if((dir > 0 && currentAnimationFrame > frameset.endFrame) || (dir < 0 && currentAnimationFrame < frameset.endFrame))
            {
                currentAnimationFrame = frameset.endFrame; // hold the last frame

                // move to next animation if one set
                if(frameset.nextFrameSet != -1)
                {
                    nextAnimation = frameset.nextFrameSet;
                    animationTimer = frameset.restartDelay * 1000;
                    continue;
                }

                // otherwise stop
                animationTimer = 0;
                break;
            }

            animationTimer += getFrameDelay();
        }
    }
}

void Object::render(SDL_Renderer *renderer, int scrollX, int scrollY, int z, float zoom)
{
    if(!texture || !data)
        return;

    int frameW, frameH;
    std::tie(frameW, frameH) = getFrameSize();

    // animation info
    auto frameset = getCurrentFrameset();

    int frameOffset = currentAnimationFrame * frameW;

    // if there's no occupancy data, draw the whole thing
    if(data->bitmapOccupancy.empty() && z == 6)
    {
        // ... using pixel offsets
        SDL_Rect dr{
            static_cast<int>(pixelX * zoom) - scrollX,
            static_cast<int>(pixelY * zoom) - scrollY,
            static_cast<int>(frameW * zoom),
            static_cast<int>(frameH * zoom)
        };

        SDL_Rect sr = {
            frameOffset,
            0,
            frameW,
            frameH
        };

        bool flipX = frameset && frameset->flipX;

        // also need to apply hotspot
        // (definitely used for the rainbow)
        dr.x -= data->hotspotX * zoom;
        dr.y -= data->hotspotY * zoom;

        SDL_RenderCopyEx(renderer, texture.get(), &sr, &dr, 0.0, nullptr, flipX ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
        return;
    }

    // "split" frames render a second image above the first one
    bool split = frameset ? frameset->splitFrames : false;

    if(z > data->maxBitmapOccupancy + (split ? 1 : 0))
        return;

    auto tileSize = World::tileSize;

    // copy frame
    for(int ty = 0; ty < int(data->bitmapSizeY); ty++)
    {
        for(int tx = 0; tx < int(data->bitmapSizeX); tx++)
        {
            // TODO: x flip
            int tileZ = data->bitmapOccupancy[tx + ty * data->bitmapSizeX];
            
            SDL_Rect sr{frameOffset + tx * tileSize, ty * tileSize, tileSize, tileSize};
            SDL_Rect dr{
                static_cast<int>((x + tx) * tileSize * zoom) - scrollX,
                static_cast<int>((y + ty) * tileSize * zoom) - scrollY,
                static_cast<int>(tileSize * zoom),
                static_cast<int>(tileSize * zoom)
            };

            if(tileZ == z)
                SDL_RenderCopy(renderer, texture.get(), &sr, &dr);
            // TODO: try to combine?

            if(split && tileZ + 1 == z)
            {
                // second layer, a bit higher
                sr.x += frameW;
                SDL_RenderCopy(renderer, texture.get(), &sr, &dr);
            }
        }
    }
}

void Object::renderDebug(SDL_Renderer *renderer, int scrollX, int scrollY, float zoom)
{
    if(!data)
        return;

    // "coords"
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);

    auto getXPos = [this, zoom, scrollX](int xOff) -> int
    {
        return (x * World::tileSize + xOff) * zoom - scrollX;
    };

    auto getYPos = [this, zoom, scrollY](int yOff) -> int
    {
        return (y * World::tileSize + yOff) * zoom - scrollY;
    };

    for(auto &coord : data->coords)
        SDL_RenderDrawPoint(renderer, getXPos(std::get<0>(coord)), getYPos(std::get<1>(coord)));

    // the other set (points/cross)
    SDL_SetRenderDrawColor(renderer, 255, 0, 255, 255);

    for(auto &coord : data->altCoords)
        SDL_RenderDrawPoint(renderer, getXPos(std::get<0>(coord)), getYPos(std::get<1>(coord)));

    // entry/exit
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);

    // left
    int px = 0;
    int py = data->entryExitOffsets[0];
    if(py)
        SDL_RenderDrawPoint(renderer, getXPos(px), getYPos(py));

    // bottom
    px = data->entryExitOffsets[1];
    py = data->bitmapSizeY * World::tileSize - 1;
    if(px)
        SDL_RenderDrawPoint(renderer, getXPos(px), getYPos(py));

    // right
    px = data->bitmapSizeX * World::tileSize - 1;
    py = data->entryExitOffsets[2];
    if(py)
        SDL_RenderDrawPoint(renderer, getXPos(px), getYPos(py));

    // top
    px = data->entryExitOffsets[3];
    py = 0;
    if(px)
        SDL_RenderDrawPoint(renderer, getXPos(px), getYPos(py));

    // free to roam
    SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
    SDL_Rect r{getXPos(data->freeToRoam[0]), getYPos(data->freeToRoam[1]), data->freeToRoam[2] - data->freeToRoam[0], data->freeToRoam[3] - data->freeToRoam[1]};
    
    if(r.w && r.h)
    {
        r.w *= zoom;
        r.h *= zoom;
        SDL_RenderDrawRect(renderer, &r);
    }
}

uint16_t Object::getId() const
{
    return id;
}

int Object::getX() const
{
    return x;
}

int Object::getY() const
{
    return y;
}

void Object::setX(int x)
{
    this->x = x;
}

void Object::setY(int y)
{
    this->y = y;
}

void Object::setPosition(int x, int y)
{
    setX(x);
    setY(y);
}

const ObjectData *Object::getData() const
{
    return data;
}


void Object::replace(uint16_t newId, std::shared_ptr<SDL_Texture> newTex, const ObjectData *newData)
{
    id = newId;
    texture = newTex;
    data = newData;
}

void Object::addMinifig(Minifig &&minifig)
{
    minifigs.emplace_back(std::move(minifig));
}

const ObjectData::Frameset *Object::getCurrentFrameset() const
{
    if(currentAnimation != -1)
        return &data->framesets[currentAnimation];

    return nullptr;
}

int Object::getFrameDelay() const
{
    auto frameset = getCurrentFrameset();
    if(frameset)
        return std::max(1, frameset->delay) * 30; // bit of a guess

    return 0;
}

void Object::setDefaultAnimation()
{
    if(data && data->defaultFrameset != -1)
        setAnimation(data->defaultFrameset);
}

void Object::setAnimation(int index)
{
    if(index < 0 || !data || index > data->numFramesets)
        return;

    currentAnimation = index;

    auto &frameset = data->framesets[currentAnimation];
    currentAnimationFrame = frameset.startFrame;

    animationTimer = getFrameDelay();
}

void Object::setAnimationFrame(int frame)
{
    // this is for things like trains that have animations, but the real frame is determined by something else
    // (orientation for trains)
    if(frame < 0 || frame > data->totalFrames)
        return;

    currentAnimationFrame = frame;
}

std::tuple<int, int> Object::getFrameSize() const
{
    if(!data || !texture)
        return {0, 0};

    // use occupancy data if present
    if(!data->bitmapOccupancy.empty())
        return {data->bitmapSizeX * 16, data->bitmapSizeY * 16};

    // fall back to image size / frames
    int w, h;

    SDL_QueryTexture(texture.get(), nullptr, nullptr, &w, &h);

    return {w / data->totalFrames, h};
}

void Object::setPixelPos(float x, float y)
{
    pixelX = x;
    pixelY = y;
}

void Object::setTargetPos(int tx, int ty, int vx, int vy, bool reverse)
{
    targetX = tx;
    targetY = ty;

    velX = vx;
    velY = vy;
    
    this->reverse = reverse;
}
