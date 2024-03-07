#pragma once

#include <istream>
#include <vector>

class ObjectData final
{
public:
    bool loadDatStream(std::istream &stream);

    struct Frameset
    {
        std::string name;
        int startFrame = 0, endFrame = 0;
        int delay = 0;
        bool splitFrames = false; // second "layer" of frame
        int restartDelay = 0;
        int nextFrameSet = 0;
        int soundId = 0;
        int replayDelay = 0;
        bool flipX = false;
    };

    std::string name;

    unsigned int physSizeX = 0, physSizeY = 0, physSizeZ = 0;
    std::vector<int> physicalOccupancy; // TODO: this is really a bitmap

    unsigned int bitmapSizeX = 0, bitmapSizeY = 0;
    std::vector<int> bitmapOccupancy; // TODO: not a bitmap, but values are small

    int entryExitOffsets[4] = {0, 0, 0, 0}; // offsets along edges?

    int freeToRoam[4] = {0, 0, 0, 0}; // rect? minifigs can wander around in?

    // used for track pieces, second list is used for points and crossings
    std::vector<std::tuple<int, int>> coords, altCoords;

    int rmbSeq = -1; // next object when pressing right mouse button while placing?

    int hotspotX = 0, hotspotY = 0; // mouse related?

    int maxMinifigForResource = 0; // max minifigs created here?

    std::vector<int> possibleMinifigs;

    int maxEmployees = 0;
    std::vector<int> possibleEmployees;

    // start/end of work/opening time?
    int shiftStart = 0, shiftEnd = 1439;

    bool leisureDestination = false;

    int totalFrames = 0;
    int numFramesets = 0;
    int cursorFrameset = -1, defaultFrameset = -1, closedFrameset = -1;
    std::vector<Frameset> framesets;

    int buttonOffset[3] = {0, 0, 0};
};