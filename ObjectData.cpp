#include <cassert>
#include <charconv>
#include <iostream>

#include "ObjectData.hpp"

enum class ParseState
{
    Init,

    PhysOccupancyHeader,
    PhysOccupancyData,

    BitmapOccupancyHeader,
    BitmapOccupancyData,

    Framesets,

    CoordList,
    CoordList2,
};

bool ObjectData::loadDatStream(std::istream &stream)
{
    std::string line;

    ParseState state = ParseState::Init;

    unsigned int numCoords[2];

    auto splitLine = [](std::string_view str)
    {
        auto chars = " \t";
        std::vector<std::string_view> tokens;

        if(str.empty())
            return tokens;

        size_t pos = str.find_first_of(chars);

        while(pos != std::string::npos)
        {
            tokens.push_back(str.substr(0, pos));
            str = str.substr(str.find_first_not_of(chars, pos));

            pos = str.find_first_of(chars);
        }

        tokens.push_back(str);

        return tokens;
    };

    auto toInt = [](std::string_view str)
    {
        int val = 0;
        auto res = std::from_chars(str.data(), str.data() + str.length(), val);

        if(res.ec != std::errc{})
            std::cerr << "Bad int value \"" << str << "\"\n";

        return val;
    };

    while(std::getline(stream, line))
    {
        // trim trailing whitespace
        while(!line.empty() && isspace(line.back()))
            line.pop_back();

        if(line.empty())
            continue;

        auto split = splitLine(line);

        switch(state)
        {
            case ParseState::Init:
                if(line == "physical_occupancy")
                    state = ParseState::PhysOccupancyHeader;
                else if(line == "bitmap_occupancy")
                    state = ParseState::BitmapOccupancyHeader;
                else if(split[0] == "entry_exit")
                {
                    assert(split.size() == 5);

                    entryExitOffsets[0] = toInt(split[1]);
                    entryExitOffsets[1] = toInt(split[2]);
                    entryExitOffsets[2] = toInt(split[3]);
                    entryExitOffsets[3] = toInt(split[4]);
                }
                else if(split[0] == "RMBSeq")
                {
                    assert(split.size() == 2);

                    rmbSeq = toInt(split[1]);
                }
                else if(split[0] == "MaxMinifigForResource")
                {
                    assert(split.size() == 2);

                    maxMinifigForResource = toInt(split[1]);
                }
                else if(split[0] == "PossibleMinifigs")
                {
                    assert(split.size() == 6);

                    split.erase(split.begin());

                    for(auto &v : split)
                    {
                        int id = toInt(v);
                        if(id != -1)
                            possibleMinifigs.push_back(id);
                    }
                }
                else if(split[0] == "Shifts")
                {
                    assert(split.size() == 5);

                    shiftStart = toInt(split[1]) * 60 + toInt(split[2]);
                    shiftEnd = toInt(split[3]) * 60 + toInt(split[4]);
                }
                else if(split[0] == "total_number_of_frames")
                {
                    assert(split.size() == 2);

                    totalFrames = toInt(split[1]);
                    state = ParseState::Framesets;
                }
                // variations of the same thing, maybe the name isn't important?
                else if(split[0] == "track_coordinates" || split[0] == "closed-open" || split[0] == "closed/open" || split[0] == "coord" || split[0] == "coords" || split[0] == "co-ords")
                {
                    assert(split.size() == 3);

                    numCoords[0] = toInt(split[1]);
                    numCoords[1] = toInt(split[2]);

                    state = ParseState::CoordList;
                }
                else if(split[0] == "button" && split[1] == "offset")
                {
                    assert(split.size() == 5);

                    buttonOffset[0] = toInt(split[2]);
                    buttonOffset[1] = toInt(split[3]);
                    buttonOffset[2] = toInt(split[4]);
                }
                // TODO: ignore case?
                else if(split[0] == "Hotspot" || split[0] == "hotspot")
                {
                    assert(split.size() == 3);

                    hotspotX = toInt(split[1]);
                    hotspotY = toInt(split[2]);
                }
                else if(split[0] == "FreeToRoam")
                {
                    assert(split.size() == 5);

                    freeToRoam[0] = toInt(split[1]);
                    freeToRoam[1] = toInt(split[2]);
                    freeToRoam[2] = toInt(split[3]);
                    freeToRoam[3] = toInt(split[4]);
                }
                else if(split[0] == "LeisureDestination")
                {
                    assert(split.size() == 2);

                    leisureDestination = split[1] == "1";
                }
                else if(split[0] == "MaxEmployees")
                {
                    assert(split.size() == 2);

                    maxEmployees = toInt(split[1]);
                }
                else if(split[0] == "PossibleEmployees")
                {
                    assert(split.size() == 6);

                    split.erase(split.begin());

                    for(auto &v : split)
                    {
                        int id = toInt(v);
                        if(id != -1)
                            possibleEmployees.push_back(id);
                    }
                }
                else if(split[0] == "closedfs")
                {
                    assert(split.size() == 2);

                    closedFrameset = toInt(split[1]);
                }
                else if(split[0] == "Name")
                {
                    name = line.substr(5);
                }
                else if(line == "InsertSeq 0 0" || line == "MobileSeq 0 0" || line == "EasterEgg -1 0  -1 0 0  -1 -1 R 0 0" || line == "EasterEgg -1 0  -1 0 0  -1 0 R 0 0")
                {} // these are unhandled, but ignore the default values
                else if(line == "-9")
                {} // usually marks the end of some kind of list
                else if(split[0] == "//")
                {} // comment
                else
                    std::cout << "Unhandled object data: " << line << std::endl;
                break;

            case ParseState::PhysOccupancyHeader:
                // read dims for first non-empty line
                assert(split.size() == 3);

                physSizeX = toInt(split[0]);
                physSizeY = toInt(split[1]);
                physSizeZ = toInt(split[2]);

                physicalOccupancy.reserve(physSizeX * physSizeY * physSizeZ);

                state = ParseState::PhysOccupancyData;

                break;

            case ParseState::PhysOccupancyData:
            {
                // data row
                assert(split.size() == physSizeX);
                
                for(auto &v : split)
                    physicalOccupancy.push_back(toInt(v));

                unsigned int totalExpectedSize = physSizeX * physSizeY * physSizeZ;
                if(physicalOccupancy.size() == totalExpectedSize)
                {
                    // done, move to the next thing
                    state = ParseState::Init;
                }
                else if(physicalOccupancy.size() > totalExpectedSize)
                {
                    std::cerr << "Too many physical_occupancy values!\n";
                    return false;
                }
                break;
            }

            case ParseState::BitmapOccupancyHeader:
                // read dims for first non-empty line
                assert(split.size() == 2);

                bitmapSizeX = toInt(split[0]);
                bitmapSizeY = toInt(split[1]);

                assert(bitmapSizeX == physSizeX);
                assert(bitmapSizeY >= physSizeY); // bitmap occupancy can be taller than physical, filling the rows above with 0?

                state = ParseState::BitmapOccupancyData;
                break;

            case ParseState::BitmapOccupancyData:
            {
                // data row
                assert(split.size() == bitmapSizeX);
                
                for(auto &v : split)
                    bitmapOccupancy.push_back(toInt(v));

                unsigned int totalExpectedSize = bitmapSizeX * bitmapSizeY;
                if(bitmapOccupancy.size() == totalExpectedSize)
                {
                    // done, move to the next thing
                    state = ParseState::Init;
                }
                else if(bitmapOccupancy.size() > totalExpectedSize)
                {
                    std::cerr << "Too many bitmap_occupancy values!\n";
                    return false;
                }
                break;
            }

            case ParseState::Framesets:
                if(line == "-9")
                {
                    assert(framesets.size() == unsigned(numFramesets));
                    // this seems to be an end marker
                    state = ParseState::Init;
                }
                else if(split[0] == "number_of_frame_sets")
                {
                    assert(split.size() == 2);

                    numFramesets = toInt(split[1]);
                }
                // cursor/default seems more common, but a few files use the other one
                else if(split[0] == "cursor/default_frame_set" || split[0] == "cursor_frame_set")
                {
                    assert(split.size() == 3);

                    cursorFrameset = toInt(split[1]);
                    defaultFrameset = toInt(split[2]);
                }
                else
                {
                    assert(split.size() == 11);

                    Frameset fs;

                    fs.name = split[0];
                    fs.startFrame = toInt(split[1]);
                    fs.endFrame = toInt(split[2]);
                    fs.delay = toInt(split[3]);
                    fs.splitFrames = split[4] == "1";
                    fs.restartDelay = toInt(split[5]);
                    fs.nextFrameSet = toInt(split[6]);
                    fs.soundId = toInt(split[7]);
                    fs.replayDelay = toInt(split[8]);
                    fs.flipX = split[9] == "1";
                    // split[10]? this doesn't seem to be documented in the comments... or used

                    if(split[10] != "0")
                        std::cout << "frameset unk " << line << "\n";

                    framesets.emplace_back(fs);
                }
                break;

            case ParseState::CoordList:
                if(split[0] == "-9") // -9 -9 is sometimes used?
                {
                    assert(coords.size() == numCoords[0]);
                    // start second list if there is one (points)
                    state = numCoords[1] ? ParseState::CoordList2 : ParseState::Init;
                }
                else
                {
                    // sometimes the last newline is missing (pnt-ne)
                    // TODO: this suggests that newlines aren't part of the structure...
                    if(split.size() == 3 && split[2] == "-9")
                    {
                        split.pop_back();
                        state = numCoords[1] ? ParseState::CoordList2 : ParseState::Init;
                    }

                    assert(split.size() == 2);
                    coords.emplace_back(toInt(split[0]), toInt(split[1]));
                }
                break;

            case ParseState::CoordList2:
                if(split[0] == "-9") // -9 -9 is sometimes used?
                {
                    assert(altCoords.size() == numCoords[1]);
                    state = ParseState::Init;
                }
                else
                {
                    assert(split.size() == 2);
                    altCoords.emplace_back(toInt(split[0]), toInt(split[1]));
                }
                break;
        }
        
    }

    return false;
}