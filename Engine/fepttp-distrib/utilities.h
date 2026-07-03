#pragma once
#include "config.h"
#include <iostream>

// CHANGE MODULO TO BITWISE AND
namespace Utils{
    int hashint(int input, int range){
        input = ((input >> 16) ^ input) * 0x45d9f3bu;
        input = ((input >> 16) ^ input) * 0x45d9f3bu;
        input = (input >> 16) ^ input;
        return (input % range);
    }
}