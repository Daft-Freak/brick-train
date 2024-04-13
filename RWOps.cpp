#include "RWOps.hpp"

SDL_RWops *rwOpsFromStream(const std::unique_ptr<std::istream> &stream)
{
    auto rwops = SDL_AllocRW();
    rwops->size = [](SDL_RWops *context) -> Sint64
    {
        return -1; // TODO?
    };

    rwops->seek = [](SDL_RWops *context, Sint64 offset, int whence) -> Sint64
    {
        auto stream = reinterpret_cast<std::istream *>(context->hidden.unknown.data1);

        auto dir = std::ios::beg;
        switch(whence)
        {
            case RW_SEEK_SET:
                break;
            case RW_SEEK_CUR:
                dir = std::ios::cur;
                break;
            case RW_SEEK_END:
                dir = std::ios::end;
                break;
            default:
                return -1;
        }

        // only handles read ptr
        stream->seekg(offset, dir);

        if(!*stream)
            return -1;

        return stream->tellg();
    };

    rwops->read = [](SDL_RWops *context, void *ptr, size_t size, size_t maxnum) -> size_t
    {
        auto stream = reinterpret_cast<std::istream *>(context->hidden.unknown.data1);

        return stream->read(reinterpret_cast<char *>(ptr), size * maxnum).gcount() / size;
    };

    rwops->write = [](SDL_RWops *context, const void *ptr, size_t size, size_t maxnum) -> size_t
    {
        return 0; // read-only
    };

    rwops->close = [](SDL_RWops *context) -> int
    {
        return 0;
    };

    rwops->hidden.unknown.data1 = stream.get();

    return rwops;
}