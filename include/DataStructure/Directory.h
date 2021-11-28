#include <iostream>
#include <stdint.h>

#include "Config.h"
#include "DirEntry.h"

struct Directory 
{
    uint16_t available;
    uint32_t inumber;
    char name[Config::NAME_SIZE];
    struct DirEntry table[Config::ENTRIES_PER_DIR];
};