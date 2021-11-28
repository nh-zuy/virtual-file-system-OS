#ifndef CONFIG_H
#define CONFIG_H

#include <iostream>
#include <stdint.h>

class Config {
public:
    /* Size of block: 512 byte */
    const static size_t BLOCK_SIZE = 512;

    /* Magic number */
    const static uint32_t MAGIC_NUMBER = 0xf0f03410;
          
    /* The number of inode in Inode Block */      
    const static uint32_t INODES_PER_BLOCK = 16;

    /* The number of direct block in Inode */             
    const static uint32_t POINTERS_PER_INODE = 5;

    /* The number of inode in indirect Inode Block */  
    const static uint32_t POINTERS_PER_BLOCK = 128; 

    /* The length of arbitary name */
    const static uint32_t NAME_SIZE = 16;

    /* The number of entry in Directory Block */
    const static uint32_t ENTRIES_PER_DIR = 7;

    /* The number of directory in Directory Block */
    const static uint32_t DIR_PER_BLOCK = 8;
};

#endif