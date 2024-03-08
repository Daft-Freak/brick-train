#include <fstream>

#include "FileLoader.hpp"

namespace fs = std::filesystem;

// simple streambuf backed by memory
class membuf final : public std::basic_streambuf<char>
{
public:
    membuf(char *ptr, size_t size)
    {
        setg(ptr, ptr, ptr + size);
    }

private:
    pos_type seekoff(off_type off, std::ios_base::seekdir dir, std::ios_base::openmode which)
    {
        pos_type ret(off_type(-1));

        auto ptr = gptr();
        auto beg = eback();
        auto end = egptr();

        switch(dir)
        {
            case std::ios_base::beg:
                ptr = beg + off;
                break;
            case std::ios_base::cur:
                ptr += off;
                break;
            case std::ios_base::end:
                ptr = end - off;
                break;
            default:
                ptr = nullptr;
        }

        // validate new ptr
        if(ptr >= beg && ptr < end)
        {
            setg(beg, ptr, end);
            return ptr - beg;
        }

        return ret;
    }

    pos_type seekpos(pos_type off, std::ios_base::openmode which)
    {
        auto ptr = eback() + off;
        if(ptr < egptr())
        {
            setg(eback(), ptr, egptr());
            return off;
        }

        return pos_type(off_type(-1));
    }
};

// stream backed by a uint8_t vector
class VectorStream final : public std::istream
{
public:
    VectorStream(std::vector<uint8_t> &&vec) : std::istream(&buffer), vec(std::move(vec)), buffer(reinterpret_cast<char *>(this->vec.data()), this->vec.size())
    {
        init(&buffer);
    }

private:
    std::vector<uint8_t> vec;
    membuf buffer;
};

FileLoader::FileLoader(std::filesystem::path basePath) : basePath(std::move(basePath))
{
    // find data path
    dataPath = this->basePath / "data";
    if(!fs::exists(dataPath))
        dataPath = this->basePath.parent_path() / "data";
    // TODO: maybe try harder

    // load the string table
    stringTable.loadFromExe(dataPath / "disc/Exe/loco.exe");
}

std::unique_ptr<std::istream> FileLoader::openResourceFile(std::string_view relPath)
{
    if(fs::exists(dataPath / "disc/art-res" / relPath))
        return std::make_unique<std::ifstream>(dataPath / "disc/art-res" / relPath, std::ios::binary);

    // TODO: handle case mismatch?

    // convert to lower case and look for resources
    std::string lowerPath(relPath);
    for(auto &c : lowerPath)
        c = tolower(c);

    for(auto &resFile : resourceFiles)
    {
        auto data = resFile.getResourceContents(lowerPath);
        if(data.has_value())
            return std::make_unique<VectorStream>(std::move(data.value()));
    }

    return nullptr;
}

std::unique_ptr<std::istream> FileLoader::openResourceFile(int32_t id, std::string_view ext)
{
    auto relPath = lookupId(id, ext);

    if(!relPath)
        return nullptr;

    return openResourceFile(relPath.value());
}

std::optional<std::string> FileLoader::lookupId(int32_t id, std::string_view ext)
{
    if(id < 0)
        return {};

    auto relPath = stringTable.lookupString(id);

    if(!relPath)
        return {};

    std::string pathStr(relPath.value());

    // convert slashes
    for(auto &c : pathStr)
    {
        if(c == '\\')
            c = '/';
    }

    return pathStr.append(ext);
}

const fs::path &FileLoader::getDataPath()
{
    return dataPath;
}

void FileLoader::addResourceFile(std::string_view relPath)
{
    resourceFiles.emplace_back(dataPath / relPath);
}
