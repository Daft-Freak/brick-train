#include <charconv>
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

const ObjectDataStore::TrainData &ObjectDataStore::getTrainData()
{
    if(trainData.empty())
    {
        // 6146 == trains/train
        auto stream = fileLoader.openResourceFile(6146, ".dat");

        if(!stream)
            return trainData;

        std::string line;
        while(std::getline(*stream >> std::ws, line))
        {
            auto start = line.c_str();
            auto end = start + line.length();

            int vals[4] = {};

            for(int i = 0; i < 4 && start != end; i++)
            {
                auto res = std::from_chars(start, end, vals[i]);

                if(res.ec != std::errc{})
                    break;

                start = res.ptr;

                while(start != end && std::isspace(*start))
                    start++;
            }

            // terminator?
            if(vals[0] == -9)
                break;

            trainData.emplace_back(vals[0], vals[1], vals[2], vals[3]);
        }
        std::cout.flush();
    }

    return trainData;
}
