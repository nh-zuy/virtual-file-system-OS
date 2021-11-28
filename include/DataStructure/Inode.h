#ifndef INODE_H
#define INODE_H

#include <iostream>
#include <stdint.h>

#include "Config.h"

struct Inode 
{
    uint32_t available;
    uint32_t size;
    uint32_t directBlocks[Config::POINTERS_PER_INODE];
    uint32_t indirectBlock;
};

#endif