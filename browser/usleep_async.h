#pragma once

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

inline void usleep(unsigned int usec)
{
    emscripten_sleep(usec / 1000);
}

#else
#include <unistd.h>
#endif
