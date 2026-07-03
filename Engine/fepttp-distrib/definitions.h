#pragma once
#include <iostream>

namespace Definitions {
    constexpr int IO_POST_SUCCESS = 0;
    constexpr int IO_POST_FAILURE = -1;
    constexpr int IO_POST_NOT_SOCK = -2;
    constexpr int COULD_NOT_FIND = -1;
    constexpr int TOO_LARGE = -2;
    constexpr int THREW_ERROR = -3;

    constexpr int CONTINUE = -4;
    constexpr int CLOSE_ERASE_CONTINUE = -5;
    constexpr int ERASE_CONTINUE = -6;
    constexpr int SCHEDULE_RETRY = -7;
    constexpr int NO_ERR = -8;
    
    constexpr int REQUEST_MALFORMED = 1;
}