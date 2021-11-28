#include <iostream>
#include <stdint.h>

#include "Config.h"

struct DirEntry 
{
    uint8_t type;
    uint8_t available;
    uint32_t inumber;
    char name[Config::NAME_SIZE];
    uint32_t protect;
    char password[10];
};