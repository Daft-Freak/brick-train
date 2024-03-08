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
    ObjectDataStore(FileLoader &fileLoader);

    const ObjectData *getObject(int32_t id);

private:
    FileLoader &fileLoader;

    std::map<int32_t, ObjectData> data;
};