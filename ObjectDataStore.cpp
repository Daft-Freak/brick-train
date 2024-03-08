#include <iostream>

#include "ObjectDataStore.hpp"

ObjectDataStore::ObjectDataStore(FileLoader &fileLoader) : fileLoader(fileLoader)
{
}

const ObjectData *ObjectDataStore::getObject(int32_t id)
{
    if(id < 0)
        return nullptr;

    // find existing
    auto it = data.find(id);

    if(it != data.end())
        return &it->second;

    // try to load
    auto stream = fileLoader.openResourceFile(id, ".dat");

    if(!stream)
    {
        std::cerr << "Failed to open dat for object " << id << "\n";
        return nullptr;
    }

    ObjectData objDat;

    if(!objDat.loadDatStream(*stream))
    {
        std::cerr << "Failed to read dat for object " << id << "\n";
        return nullptr;
    }

    return &data.emplace(id, objDat).first->second;
}