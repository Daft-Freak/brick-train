#pragma once

#include <cstdint>
#include <map>

#include "FileLoader.hpp"
#include "ObjectData.hpp"

// stores object data
// this is the best name I could come up with...
class ObjectDataStore final
{
public:
    using TrainData = std::vector<std::tuple<int, int, int, int>>;

    ObjectDataStore(FileLoader &fileLoader);

    const ObjectData *getObject(int32_t id);

    const TrainData &getTrainData();

private:
    FileLoader &fileLoader;

    std::map<int32_t, ObjectData> data;

    // list of two pairs of coords from train.dat
    TrainData trainData;
};