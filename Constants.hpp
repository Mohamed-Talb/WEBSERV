#ifndef CONSTANTS_HPP
#define CONSTANTS_HPP

#include <cstddef>

namespace cgi {
    const size_t MAX_BUFFER = 1024 * 1024;
    const size_t RESUME_THRESHOLD = MAX_BUFFER / 2;
}

#endif