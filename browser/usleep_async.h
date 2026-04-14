#pragma once

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

extern "C" {
    inline int usleep(unsigned int usec)
    {
    emscripten_sleep(usec / 1000);
    return 0;
    }
}

#else
#include <unistd.h>
#endif
