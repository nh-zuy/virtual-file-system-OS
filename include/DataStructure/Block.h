#include <iostream>
#include <stdint.h>

#include "Config.h"
#include "VolumeEmulator/Volume.h"
#include "Directory.h"
#include "MetaBlock.h"
#include "Inode.h"

/**
 * @brief Block is primary structure in Volume layout.
 * @brief There are 4 types: MetaBlock, InodeBlock, DataBlock, DirectoryBlock.
 **/
union Block
{
    struct MetaBlock metaBlock;
    struct Inode inodes[Config::INODES_PER_BLOCK];
    uint32_t pointers[Config::POINTERS_PER_BLOCK];
    char data[Config::BLOCK_SIZE];
    struct Directory directories[Config::DIR_PER_BLOCK];
};