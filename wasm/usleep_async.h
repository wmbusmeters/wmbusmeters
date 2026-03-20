#pragma once

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

// Mappe usleep automatisch auf Asyncify-kompatiblen Sleep
inline void usleep(unsigned int usec)
{
    // emscripten_sleep erwartet Millisekunden
    emscripten_sleep(usec / 1000);
}

#else
#include <unistd.h>  // normale POSIX usleep
#endif
