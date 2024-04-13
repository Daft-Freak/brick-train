#pragma once

#include <istream>
#include <memory>

#include <SDL.h>

// istream -> RWops wrapper
// DOES NOT TAKE OWNERSHIP OF THE STREAM
// (don't free it)
SDL_RWops *rwOpsFromStream(const std::unique_ptr<std::istream> &stream);