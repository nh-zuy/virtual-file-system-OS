#ifndef META_BLOCK_H
#define META_BLOCK_H

#include <iostream>
#include <stdint.h>

struct MetaBlock 
{
    uint32_t magicNumber;
    uint32_t blocks;
    uint32_t inodeBlocks;
    uint32_t inodes;
    uint32_t dirBlocks;
    uint32_t protect;
    char password[257];
};

#endif