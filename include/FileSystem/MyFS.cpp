#include "MyFS.h"
#include "HashMachine/Hasher.h"

#include <algorithm>
#include <string>
#include <math.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <iostream>

bool MyFS::format(Volume *disk) {
    if (disk->isMounted()) {
        return false;
    } 

    /** 
     * write Meta Block 
     **/
    Block block;
    memset(&block, 0, sizeof(Block));

    block.metaBlock.magicNumber = Config::MAGIC_NUMBER;
    block.metaBlock.blocks = (uint32_t)(disk->size());
    block.metaBlock.inodeBlocks = (uint32_t)std::ceil((int(block.metaBlock.blocks) * 1.00)/10);
    block.metaBlock.inodes = block.metaBlock.inodeBlocks * Config::INODES_PER_BLOCK;
    block.metaBlock.dirBlocks = (uint32_t)std::ceil((int(block.metaBlock.blocks) * 1.00)/100);
    block.metaBlock.protect = 0;
    memset(block.metaBlock.password, 0, 257);

    /** Write down Meta Block to volume **/
    disk->writeBlock(0, block.data);

    /** Clean all the blocks of Volume */
    for(uint32_t i = 1; i <= block.metaBlock.inodeBlocks; i++){
        Block inodeBlock;

        /** First, clean each of Inodes */
        for(uint32_t j = 0; j < Config::INODES_PER_BLOCK; j++){
            inodeBlock.inodes[j].available = false;
            inodeBlock.inodes[j].size = 0;

            /** Clean all direct block pointers of Inode */
            for(uint32_t k = 0; k < Config::POINTERS_PER_INODE; k++) {   
                inodeBlock.inodes[j].directBlocks[k] = 0;
            }
    
            /** Clean indirect block pointer of Inode */
            inodeBlock.inodes[j].indirectBlock = 0;
        }

        /** Write down change of Inode Block **/
        disk->writeBlock(i, inodeBlock.data);
    }

    /** Free Data Blocks */
    uint32_t blocks = block.metaBlock.blocks;
    uint32_t inodeBlocks = block.metaBlock.inodeBlocks;
    uint32_t dirBlocks = block.metaBlock.dirBlocks;

    for(uint32_t i = inodeBlocks + 1; i < (blocks - dirBlocks); i++){
        Block dataBlock;
        memset(dataBlock.data, 0, Config::BLOCK_SIZE);
        disk->writeBlock(i, dataBlock.data);
    }

    /** 
     * Clean all Directory Blocks 
     **/
    for(uint32_t i = (blocks - dirBlocks); i < blocks; i++) {
        Directory directory;
        directory.inumber = -1;
        directory.available = 0;
        memset(directory.table, 0, sizeof(DirEntry) * Config::ENTRIES_PER_DIR);

        /** Clean routing table of Directory Block */
        Block directoryBlock;
        for(uint32_t j = 0; j < Config::DIR_PER_BLOCK; j++) {
            directoryBlock.directories[j] = directory;
        }

        /** Write down change */
        disk->writeBlock(i, directoryBlock.data);
    }

    /**
     *  Recreate root 
     **/
    Directory root;
    strcpy(root.name, "/");
    root.inumber = 0;
    root.available = 1;

    /**
     *   Create self entry.
     **/
    DirEntry entry;
    memset(&entry, 0, sizeof(entry));
    entry.inumber = 0;
    entry.type = 0;
    entry.available = 1;

    char self[] = ".";
    strcpy(entry.name, self);
    memcpy(&(root.table[0]), &entry, sizeof(DirEntry));

    
    /**
     *   Create back entry.
     **/
    char back[] = "..";
    strcpy(entry.name, back);
    memcpy(&(root.table[1]), &entry, sizeof(DirEntry));

    /**
     *   Empty the directories 
     **/
    Block dirBlock;
    memcpy(&(dirBlock.directories[0]), &root, sizeof(root));
    disk->writeBlock(blocks - 1, dirBlock.data);

    return true;
}

bool MyFS::mount(Volume *disk) {
    if(disk->isMounted()) {
        return false;
    }

    /** Read and check superblock */
    Block block;
    disk->readBlock(0, block.data);
    if (block.metaBlock.magicNumber != Config::MAGIC_NUMBER
        || block.metaBlock.inodeBlocks != std::ceil((block.metaBlock.blocks * 1.00)/10)
        || block.metaBlock.inodes != (block.metaBlock.inodeBlocks * Config::INODES_PER_BLOCK)
        || block.metaBlock.dirBlocks != (uint32_t)std::ceil((int(block.metaBlock.blocks) * 1.00)/100)) 
    {
        return false;
    }

    /** Handle Password Protection */
    if(block.metaBlock.protect) {
        char pass[1000], line[1000];

    	printf("Enter password: ");
        if (fgets(line, BUFSIZ, stdin) == NULL) {
    	    return false;
    	}
        sscanf(line, "%s", pass);

        Hasher hasher;
        hasher.update(pass);
        uint8_t * digest = hasher.digest();

        if(Hasher::toString(digest) == std::string(block.metaBlock.password)){
            printf("Disk Unlocked\n");
            return true;
        } else {
            printf("Password Failed. Exiting...\n");
            return false;
        }
        delete[] digest;
    }

    disk->mount();
    mountedDisk = disk;

    /** copy metadata */
    metaData = block.metaBlock;

    /** allocate free block, inode bitmap */ 
    freeBlocks.resize(metaData.blocks, false);
    inodeCounter.resize(metaData.inodeBlocks, 0);

    /** setting free bit map node 0 to true for superblock */
    freeBlocks[0] = true;

    /** read inode blocks */
    for(uint32_t i = 1; i <= metaData.inodeBlocks; i++) {
        disk->readBlock(i, block.data);

        for(uint32_t j = 0; j < Config::INODES_PER_BLOCK; j++) {
            if (block.inodes[j].available) {
                inodeCounter[i-1] += 1;

                /** set free bit map for inode blocks */
                if(block.inodes[j].available) {
                    freeBlocks[i] = true;
                }

                /** set free bit map for direct pointers */
                for(uint32_t k = 0; k < Config::POINTERS_PER_INODE; k++) {
                    uint32_t dataBlock = block.inodes[j].directBlocks[k];

                    if (dataBlock) {
                        if (dataBlock < metaData.blocks) {
                            freeBlocks[dataBlock] = true;
                        } else {
                            return false;
                        }
                    }
                }

                /** set free bit map for indirect pointers */
                if (block.inodes[j].indirectBlock){
                    if (block.inodes[j].indirectBlock < metaData.blocks) {
                        freeBlocks[block.inodes[j].indirectBlock] = true;

                        Block indirect;
                        mountedDisk->readBlock(block.inodes[j].indirectBlock, indirect.data);

                        for(uint32_t k = 0; k < Config::POINTERS_PER_BLOCK; k++) {
                            if (indirect.pointers[k] < metaData.blocks) {
                                freeBlocks[indirect.pointers[k]] = true;
                            } else {
                                return false;
                            }
                        }
                    } else {
                        return false;
                    }
                }
            }
        }
    }

    /** Allocate dir_counter */
    dirCounter.resize(metaData.dirBlocks, 0);

    Block dirBlock;
    for(uint32_t dirs = 0; dirs < metaData.dirBlocks; dirs++){
        disk->readBlock(metaData.blocks - 1 - dirs, dirBlock.data);

        for(uint32_t offset = 0; offset < Config::DIR_PER_BLOCK; offset++){
            if(dirBlock.directories[offset].available == 1) {
                dirCounter[dirs]++;
            }
        }
        
        if (dirs == 0){
            currentDir = dirBlock.directories[0];
        }
    }

    mounted = true;

    return true;
}

ssize_t MyFS::createInode() {
    if(!mounted) {
        return false;
    }

    /** read the superblock */
    Block block;
    mountedDisk->readBlock(0, block.data);

    /** locate free inode in inode table */    
    for(uint32_t i = 1; i <= metaData.inodeBlocks; i++) {
        /** check if inode block is full */
        if (inodeCounter[i-1] == Config::INODES_PER_BLOCK) {
            continue;
        } else {
            mountedDisk->readBlock(i, block.data);
        }
        
        /** find the first empty inode */
        for(uint32_t j = 0; j < Config::INODES_PER_BLOCK; j++) {
            /** set the inode to default values */
            if(!block.inodes[j].available) {
                block.inodes[j].available = true;
                block.inodes[j].size = 0;
                block.inodes[j].indirectBlock = 0;

                for(int ii = 0; ii < 5; ii++) {
                    block.inodes[j].directBlocks[ii] = 0;
                }

                freeBlocks[i] = true;
                inodeCounter[i - 1]++;

                mountedDisk->writeBlock(i, block.data);

                return (((i-1) * Config::INODES_PER_BLOCK) + j);
            }
        }
    }

    return -1;
}

bool MyFS::loadInode(size_t inumber, Inode *node) {
    if(!mounted) {
        return false;
    }

    if(inumber < 1 || inumber > metaData.inodes) { 
        return false;
    }

    /** find index of inode in the inode table */
    int blockId = inumber / Config::INODES_PER_BLOCK;
    int blockOffset = inumber % Config::INODES_PER_BLOCK;

    /** load the inode into Inode *node */
    Block block;
    if(inodeCounter[blockId]) {
        mountedDisk->readBlock(blockId + 1, block.data);

        if(block.inodes[blockOffset].available) {
            *node = block.inodes[blockOffset];
            return true;
        }
    }

    return false;
}

bool MyFS::removeInode(size_t inumber) {
    if(!mounted) {
        return false;
    }

    Inode node;

    /** check if the node is valid; if yes, then load the inode */
    if(loadInode(inumber, &node)) {
        node.available = false;
        node.size = 0;

        /** 
         * Decrement the corresponding inode block in inode counter 
         * if the inode counter decreases to 0, then set the free bit map to false */ 
        if(!(--inodeCounter[inumber / Config::INODES_PER_BLOCK])) {
            freeBlocks[inumber / Config::INODES_PER_BLOCK + 1] = false;
        }

        /** Free direct blocks */
        for(uint32_t i = 0; i < Config::POINTERS_PER_INODE; i++) {
            freeBlocks[node.directBlocks[i]] = false;
            node.directBlocks[i] = 0;
        }

        /** Free indirect blocks */
        if(node.indirectBlock) {
            Block indirect;
            mountedDisk->readBlock(node.indirectBlock, indirect.data);
            freeBlocks[node.indirectBlock] = false;
            node.indirectBlock = 0;

            for(uint32_t i = 0; i < Config::POINTERS_PER_BLOCK; i++) {
                if(indirect.pointers[i]) {
                    freeBlocks[indirect.pointers[i]] = false;
                }
            }
        }

        Block block;
        mountedDisk->readBlock(inumber / Config::INODES_PER_BLOCK + 1, block.data);
        block.inodes[inumber % Config::INODES_PER_BLOCK] = node;
        mountedDisk->writeBlock(inumber / Config::INODES_PER_BLOCK + 1, block.data);

        return true;
    }
    
    return false;
}

ssize_t MyFS::stat(size_t inumber) {
    if(!mounted) {
        return -1; 
    }

    /** load inode; if valid, return its size */
    Inode node;
    if(loadInode(inumber, &node)) {
        return node.size;
    }

    return -1;
}

void MyFS::readDataFromOffset(uint32_t blocknum, int offset, int *length, char **data, char **ptr) {
    /** read the block from disk and change the pointers accordingly */
    mountedDisk->readBlock(blocknum, *ptr);

    *data += offset;
    *ptr += Config::BLOCK_SIZE;
    *length -= (Config::BLOCK_SIZE - offset);

    return;
}

ssize_t MyFS::read(size_t inumber, char *data, int length, size_t offset) {

    /** sanity check */
    if (!mounted) {
        return -1;
    }

    /** IMPORTANT: start reading from index = offset */
    int size_inode = stat(inumber);
    
    /** if offset is greater than size of inode, then no data can be read 
     * if length + offset exceeds the size of inode, adjust length accordingly
    **/
    if((int)offset >= size_inode) {
        return 0;
    } else if(length + (int)offset > size_inode) {
        length = size_inode - offset;
    }

    /** 
     * Data is head. 
     * ptr is tail. 
     **/
    char *ptr = data;     
    int readByte = length;

    /** load inode; if invalid, return error */
    Inode node;
    if(loadInode(inumber, &node)) {
        /** the offset is within direct pointers */
        if(offset < Config::POINTERS_PER_INODE * Config::BLOCK_SIZE) {
            /** calculate the node to start reading from */
            uint32_t direct_node = offset / Config::BLOCK_SIZE;
            offset %= Config::BLOCK_SIZE;

            /** check if the direct node is valid */ 
            if(node.directBlocks[direct_node]) {
                readDataFromOffset(node.directBlocks[direct_node++], offset, &length, &data, &ptr);
                /** read the direct blocks */
                while(length > 0 && direct_node < Config::POINTERS_PER_INODE && node.directBlocks[direct_node]) {
                    readDataFromOffset(node.directBlocks[direct_node++], 0, &length, &data, &ptr);
                }

                /** if length <= 0, then enough data has been read */
                if(length <= 0) {
                    return readByte;
                } else {

                    /** check if all the direct nodes have been read completely 
                     *  and if the indirect pointer is valid
                    */
                    if (direct_node == Config::POINTERS_PER_INODE && node.indirectBlock) {
                        Block indirect;
                        mountedDisk->readBlock(node.indirectBlock, indirect.data);

                        /** read the indirect nodes */
                        for(uint32_t i = 0; i < Config::POINTERS_PER_BLOCK; i++) {
                            if(indirect.pointers[i] && length > 0) {
                                readDataFromOffset(indirect.pointers[i], 0, &length, &data, &ptr);
                            }
                            else break;
                        }

                        /** if length <= 0, then enough data has been read */
                        if(length <= 0) {
                            return readByte;
                        } else {
                            /** data exhausted but the length requested was more */
                            /** logically, this should never print */
                            return (readByte - length);
                        }
                    }
                    else {
                        /** data exhausted but the length requested was more */
                        /** logically, this should never print */
                        return (readByte - length);
                    }
                }
            }
            else {
                /** inode has no stored data */
                return 0;
            }
        } else {
            /** offset begins in the indirect block */
            /** check if the indirect node is valid */
            if(node.indirectBlock) {
                /** change offset accordingly and find the indirect node to start reading from */
                offset -= (Config::POINTERS_PER_INODE * Config::BLOCK_SIZE);
                uint32_t indirect_node = offset / Config::BLOCK_SIZE;
                offset %= Config::BLOCK_SIZE;

                Block indirect;
                mountedDisk->readBlock(node.indirectBlock, indirect.data);

                if(indirect.pointers[indirect_node] && length > 0) {
                    readDataFromOffset(indirect.pointers[indirect_node++], offset, &length, &data, &ptr);
                }

                /** Iterate through the indirect nodes */
                for(uint32_t i = indirect_node; i < Config::POINTERS_PER_BLOCK; i++) {
                    if(indirect.pointers[i] && length > 0) {
                        readDataFromOffset(indirect.pointers[i], 0, &length, &data, &ptr);
                    }
                    else break;
                }
                
                /** if length <= 0, then enough data has been read */
                if(length <= 0) {
                    return readByte;
                } else {
                    /** data exhausted but the length requested was more */
                    /** logically, this should never print */
                    return (readByte - length);
                }
            }
            else {
                /** the indirect node is invalid */
                return 0;
            }
        }
    }

    return -1;
}

uint32_t MyFS::allocateBlock() {
    if(!mounted) {
        return 0;
    } 

    /** Loop through the free bit map and allocate the first free block */
    for(uint32_t i = metaData.inodeBlocks + 1; i < metaData.blocks; i++) {
        if (freeBlocks[i] == 0) {
            freeBlocks[i] = true;

            return (uint32_t)i;
        }
    }

    /** Volume is full */
    return 0;
}


ssize_t MyFS::writeAndReturnSize(size_t inumber, Inode* node, int ret) {
    if(!mounted) {
        return -1;
    }

    /** Find index of inode in the inode table */
    int blockId = inumber / Config::INODES_PER_BLOCK;
    int blockOffset = inumber % Config::INODES_PER_BLOCK;

    /** store the node into the block */
    Block block;
    mountedDisk->readBlock(blockId + 1, block.data);

    block.inodes[blockOffset] = *node;
    mountedDisk->writeBlock(blockId + 1, block.data);

    return (ssize_t)ret;
}


void MyFS::readBuffer(int offset, int *read, int length, char *data, uint32_t blockId) {
    if(!mounted) {
        return;
    }
    
    /** allocate memory to ptr which acts as buffer for reading from disk */
    char* ptr = (char *)calloc(Config::BLOCK_SIZE, sizeof(char));

    /** read data into ptr and change pointers accordingly */ 
    for(int i = offset; i < (int)Config::BLOCK_SIZE && *read < length; i++) {
        ptr[i] = data[*read];
        *read = *read + 1;
    }
    mountedDisk->writeBlock(blockId, ptr);

    /** free the allocated memory */
    free(ptr);

    return;
}


bool MyFS::checkAllocation(Inode* node, int read, int offset, uint32_t &blocknum, 
        bool write_indirect, Block indirect) {
    if(!mounted) {
        return false;
    }
    
    /** if blocknum is 0, then allocate a new block */
    if(!blocknum) {
        blocknum = allocateBlock();

        /** set size of node and write back to disk if it is an indirect node */
        if (!blocknum) {
            node->size = read + offset;

            if(write_indirect) {
                mountedDisk->writeBlock(node->indirectBlock, indirect.data);
            }

            return false;
        }
    }

    return true;
}


ssize_t MyFS::write(size_t inumber, char *data, int length, size_t offset) {
    if(!mounted) {
        return -1;
    }
    
    Inode node;
    Block indirect;
    int read = 0;
    int orig_offset = offset;

    /** insufficient size */
    if (length + offset > (Config::POINTERS_PER_BLOCK + Config::POINTERS_PER_INODE) * Config::BLOCK_SIZE) {
        return -1;
    }

    /**
     *  if the inode is invalid, allocate inode.
     *  need not write to disk right now; will be taken care of in write_ret()
     */
    if (!loadInode(inumber, &node)) {
        node.available = true;
        node.size = length + offset;
        for(uint32_t ii = 0; ii < Config::POINTERS_PER_INODE; ii++) {
            node.directBlocks[ii] = 0;
        }
        node.indirectBlock = 0;
        inodeCounter[inumber / Config::INODES_PER_BLOCK]++;
        freeBlocks[inumber / Config::INODES_PER_BLOCK + 1] = true;
    }
    else {
        /** set size of the node */
        node.size = std::max((int)node.size, length + (int)offset);
    }

    /** check if the offset is within direct pointers */
    if(offset < Config::POINTERS_PER_INODE * Config::BLOCK_SIZE) {
        /** find the first node to start writing at and change offset accordingly */
        int direct_node = offset / Config::BLOCK_SIZE;
        offset %= Config::BLOCK_SIZE;

        /** check if the node is valid; if invalid; allocates a block and if no block is available, returns false */
        if(!checkAllocation(&node, read, orig_offset, node.directBlocks[direct_node], false, indirect)) { 
            return writeAndReturnSize(inumber, &node, read);
        }
        /** read from data buffer */       
        readBuffer(offset, &read, length, data, node.directBlocks[direct_node++]);

        /** enough data has been read from data buffer */
        if(read == length) {
            return writeAndReturnSize(inumber, &node, length);
        } else {
            /**
             * Store in direct pointers till either one of the two things happen:
             * 1. all the data is stored in the direct pointers
             * 2. the data is stored in indirect pointers
            **/

            /** start writing into direct nodes */
            for(int i = direct_node; i < (int)Config::POINTERS_PER_INODE; i++) {
                /** check if the node is valid; if invalid; allocates a block and if no block is available, returns false */
                if(!checkAllocation(&node, read, orig_offset, node.directBlocks[direct_node], false, indirect)) { 
                    return writeAndReturnSize(inumber, &node, read);
                }
                readBuffer(0, &read, length, data, node.directBlocks[direct_node++]);

                /** enough data has been read from data buffer */
                if(read == length) {
                    return writeAndReturnSize(inumber, &node, length);
                }
            }

            /** check if the indirect node is valid */
            if(node.indirectBlock) {
                mountedDisk->readBlock(node.indirectBlock, indirect.data);
            } else {
                /** check if the node is valid; if invalid; allocates a block and if no block is available, returns false */
                if(!checkAllocation(&node, read, orig_offset, node.indirectBlock, false, indirect)) { 
                    return writeAndReturnSize(inumber, &node, read);
                }
                mountedDisk->readBlock(node.indirectBlock, indirect.data);

                /** initialise the indirect nodes */
                for(int i = 0; i < (int)Config::POINTERS_PER_BLOCK; i++) {
                    indirect.pointers[i] = 0;
                }
            }
            
            /** write into indirect nodes */
            for (int j = 0; j < (int)Config::POINTERS_PER_BLOCK; j++) {
                /** check if the node is valid; if invalid; allocates a block and if no block is available, returns false */
                if(!checkAllocation(&node, read, orig_offset, indirect.pointers[j], true, indirect)) { 
                    return writeAndReturnSize(inumber, &node, read);
                }
                readBuffer(0, &read, length, data, indirect.pointers[j]);

                /** enough data has been read from data buffer */
                if(read == length) {
                    mountedDisk->writeBlock(node.indirectBlock, indirect.data);
                    return writeAndReturnSize(inumber, &node, length);
                }
            }

            /** space exhausted */
            mountedDisk->writeBlock(node.indirectBlock, indirect.data);
            return writeAndReturnSize(inumber, &node, read);
        }
    } else {
        /** find the first indirect node to write into and change offset accordingly */
        offset -= (Config::BLOCK_SIZE * Config::POINTERS_PER_INODE);
        int indirect_node = offset / Config::BLOCK_SIZE;
        offset %= Config::BLOCK_SIZE;

        /** check if the indirect node is valid */
        if(node.indirectBlock) {
            mountedDisk->readBlock(node.indirectBlock, indirect.data);
        } else {
            /** check if the node is valid; if invalid; allocates a block and if no block is available, returns false */
            if(!checkAllocation(&node, read, orig_offset, node.indirectBlock, false, indirect)) { 
                return writeAndReturnSize(inumber, &node, read);
            }
            mountedDisk->readBlock(node.indirectBlock, indirect.data);

            /** initialise the indirect nodes */
            for(int i = 0; i < (int)Config::POINTERS_PER_BLOCK; i++) {
                indirect.pointers[i] = 0;
            }
        }

        /** check if the node is valid; if invalid; allocates a block and if no block is available, returns false */
        if(!checkAllocation(&node, read, orig_offset, indirect.pointers[indirect_node], true, indirect)) { 
            return writeAndReturnSize(inumber, &node, read);
        }
        readBuffer(offset, &read, length, data, indirect.pointers[indirect_node++]);

        /** enough data has been read from data buffer */
        if(read == length) {
            mountedDisk->writeBlock(node.indirectBlock, indirect.data);
            return writeAndReturnSize(inumber, &node, length);
        } else {
            for(int j = indirect_node; j < (int)Config::POINTERS_PER_BLOCK; j++) {
                /** check if the node is valid; if invalid; allocates a block and if no block is available, returns false */
                if(!checkAllocation(&node, read, orig_offset, indirect.pointers[j], true, indirect)) { 
                    return writeAndReturnSize(inumber, &node, read);
                }
                readBuffer(0, &read, length, data, indirect.pointers[j]);

                /** enough data has been read from data buffer */
                if(read == length) {
                    mountedDisk->writeBlock(node.indirectBlock, indirect.data);
                    return writeAndReturnSize(inumber, &node, length);
                }
            }

            /** space exhausted */
            mountedDisk->writeBlock(node.indirectBlock, indirect.data);
            return writeAndReturnSize(inumber, &node, read);
        }
    }
  
    return -1;
}

bool MyFS::setPassword(){
    if(!mounted) { 
        return false;
    }

    if (metaData.protect) {
        return changePassword();
    }

    char pass[1000], line[1000];
    Block block;

    /**  Effort to get new password  */
    printf("Enter new password: ");
    if (fgets(line, BUFSIZ, stdin) == NULL) {
        return false;
    }
    sscanf(line, "%s", pass);
    
    /**  Using SHA-256 to hashing password  */
    Hasher hasher;
    hasher.update(pass);
    uint8_t * digest = hasher.digest();
    
    metaData.protect = 1;
    strcpy(metaData.password, Hasher::toString(digest).c_str());
    delete[] digest;
    
    /** Save changed file system to Volume */
    block.metaBlock = metaData;
    mountedDisk->writeBlock(0, block.data);
    printf("New password set.\n");

    return true;
}

bool MyFS::changePassword(){
    if(!mounted) { 
        return false;
    }

    if (metaData.protect) {
        char pass[1000], line[1000];
        printf("Enter current password: ");
        if (fgets(line, BUFSIZ, stdin) == NULL) {
            return false;
        } 
        sscanf(line, "%s", pass);
        
        Hasher hasher;
        hasher.update(pass);
        uint8_t * digest = hasher.digest();

        if(Hasher::toString(digest) != std::string(metaData.password)){
            printf("Old password incorrect.\n");
            return false;
        }
        delete[] digest;

        metaData.protect = 0;
    }

    return MyFS::setPassword();   
}

bool MyFS::removePassword(){
    if(!mounted) { 
        return false;
    }

    if (metaData.protect){
        /**  Initializations  */
        char pass[1000], line[1000];
        Block block;
        
        /**  Get current password  */
        printf("Enter old password: ");
        if (fgets(line, BUFSIZ, stdin) == NULL) return false;
        sscanf(line, "%s", pass);

        Hasher hasher;
        hasher.update(pass);
        uint8_t * digest = hasher.digest();
        
        /**  Curr password incorrect. Error  */
        if(Hasher::toString(digest) != std::string(metaData.password)){
            printf("Old password incorrect.\n");
            return false;
        }
        delete[] digest;
        
        /**  Update cached MetaData  */
        metaData.protect = 0;
        
        /**  Write back the changes  */
        block.metaBlock = metaData;
        mountedDisk->writeBlock(0, block.data);   
        printf("Password removed successfully.\n");
        
        return true;
    }

    return false;    
}

bool MyFS::setPasswordFile(char name[]) {
    /** Get offset of current directory */
    int offset = dirLookup(currentDir, name);
    if(offset == -1) { 
        return false;
    }

    /**   Read directory from block  */
    Directory dir = readDirFromOffset(offset);
    if(dir.available == 0) { 
        return false;
    }

    for(uint32_t i = 0; i < Config::ENTRIES_PER_DIR; i++){
        DirEntry entry = dir.table[i];

        if (entry.available == 0 || entry.type == 0){
            continue;
        }

        if (entry.protect) {
            char pass[1000], line[1000];
            printf("Enter new password: ");
            if (fgets(line, BUFSIZ, stdin) == NULL) {
                return false;
            } 
            sscanf(line, "%s", pass);
            
            entry.protect = 1;
            strcpy(entry.password, line);
            
            syncDirectory(currentDir);

            return true;
        }
    }

    return false;
}

bool MyFS::changePasswordFile(char name[]) {
    int offset = dirLookup(currentDir, name);
    if(offset == -1) { 
        return false;
    }

    /**   Read directory from block  */
    Directory dir = readDirFromOffset(offset);
    if(dir.available == 0) { 
        return false;
    }

    for(uint32_t i = 0; i < Config::ENTRIES_PER_DIR; i++){
        DirEntry entry = dir.table[i];

        if (entry.available == 0 || entry.type == 0){
            continue;
        }

        if (entry.protect) {
            char pass[1000], line[1000];
            printf("Enter new password: ");
            if (fgets(line, BUFSIZ, stdin) == NULL) {
                return false;
            } 
            sscanf(line, "%s", pass);
            
            entry.protect = 1;
            strcpy(entry.password, line);
            
            syncDirectory(currentDir);

            return true;
        }
    }

    return MyFS::setPasswordFile(name);
}

Directory MyFS::addDirEntry(Directory dir, uint32_t inum, uint32_t type, char name[]){
    Directory tempdir = dir;

    /** Find a free Dirent in the Table to be modified */
    uint32_t idx = 0;
    for(; idx < Config::ENTRIES_PER_DIR; idx++){
        if(tempdir.table[idx].available == 0) {
            break;
        }
    }

    /** If No Dirent Found, Error  */
    if(idx == Config::ENTRIES_PER_DIR){
        printf("Directory entry limit reached..exiting\n");
        tempdir.available = 0;
        return tempdir;
    }

    /**  Free Dirent found, add the new one to the table  */ 
    tempdir.table[idx].inumber = inum;
    tempdir.table[idx].type = type;
    tempdir.table[idx].available = 1;
    strcpy(tempdir.table[idx].name, name);

    return tempdir;
}

Directory MyFS::readDirFromOffset(uint32_t offset){
    if (offset < 0 
        || offset >= Config::ENTRIES_PER_DIR
        || currentDir.table[offset].available == 0 
        || currentDir.table[offset].type != 0) 
    {
        Directory emptyDir; 
        emptyDir.available = 0;

        return emptyDir;
    }

    /**   Get offsets and indexes  */
    uint32_t inumber = currentDir.table[offset].inumber;
    uint32_t blockId = inumber / Config::DIR_PER_BLOCK;
    uint32_t blockOffset = inumber % Config::DIR_PER_BLOCK;
    
    /**   Read Block  */
    Block block;
    mountedDisk->readBlock(metaData.blocks - 1 - blockId, block.data);

    return (block.directories[blockOffset]);
}

void MyFS::syncDirectory(Directory directory){
    /**   
     * Calculate offset and index  
     **/
    uint32_t blockId = directory.inumber / Config::DIR_PER_BLOCK;
    uint32_t blockOffset = directory.inumber % Config::DIR_PER_BLOCK;

    /**   Get Block from Volume  */
    Block block;
    mountedDisk->readBlock(metaData.blocks - 1 - blockId, block.data);
    block.directories[blockOffset] = directory;

    /**   Save change of Directory Block  */
    mountedDisk->writeBlock(metaData.blocks - 1 - blockId, block.data);
}

int MyFS::dirLookup(Directory directory,char name[]){
    /**   Search the curr_dir.Table for name  */
    uint32_t offset;
    for(offset = 0; offset < Config::ENTRIES_PER_DIR; offset++){
        if( directory.table[offset].available == 1 
            && strcmp(directory.table[offset].name, name) == 0) {
            break;
        }
    }

    /**   No such dir found  */
    if(offset == Config::ENTRIES_PER_DIR ){
        return -1;
    }

    return offset;
}

bool MyFS::mkdir(char name[Config::NAME_SIZE]){
    if(!mounted) { 
        return false;
    }

    /**   Find empty dirblock  */
    uint32_t blockId = 0;
    for(; blockId < metaData.dirBlocks; blockId++) {
        if(dirCounter[blockId] < Config::DIR_PER_BLOCK)
            break;
    }

    if (blockId == metaData.dirBlocks) {
        printf("Directory limit reached\n"); 
        return false;
    }

    /**   Read empty dirblock  */
    Block block;
    mountedDisk->readBlock(metaData.blocks - 1 - blockId, block.data);

    /**   Find empty directory in dirblock  */
    uint32_t offset = 0;
    for(; offset < Config::DIR_PER_BLOCK; offset++) {
        if(block.directories[offset].available == 0) {
            break;
        }
    }
    
    if(offset == Config::DIR_PER_BLOCK) { 
        printf("Error in creating directory.\n"); 
        return false;
    }

    /**   Create new directory  */
    Directory newDirectory, temp;
    memset(&newDirectory, 0, sizeof(Directory));
    newDirectory.inumber = blockId * Config::DIR_PER_BLOCK + offset;
    newDirectory.available = 1;
    strcpy(newDirectory.name, name);
    
    /**   Create new entries for "."  */
    char self[] = ".";
    temp = newDirectory;
    temp = addDirEntry(temp, temp.inumber , 0, self);
    
    /**   Create new entries for ".."  */
    char back[] = "..";
    temp = addDirEntry(temp, currentDir.inumber, 0, back);

    if(temp.available == 0) {
        printf("Error creating new directory\n"); 
        return false;
    }
    newDirectory = temp;
    
    /**   Add new entry to the curr_dir  */
    temp = addDirEntry(currentDir, newDirectory.inumber, 0, newDirectory.name);
    if(temp.available == 0) {
        printf("Error adding new directory\n");
        return false;
    }
    currentDir = temp;
    
    /**   Write the new directory back to the disk  */
    syncDirectory(newDirectory);
    syncDirectory(currentDir);

    /**   Increment the counter  */
    dirCounter[blockId]++;

    return true;
    
}

Directory MyFS::removeDirectory(Directory parent, char name[]) {

    /**  initializations  */
    Directory dir;

    /**  Sanity Checks  */
    if(!mounted) {
        dir.available = 0; 
        return dir;
    }

    /**  Get offset of the directory to be removed  */
    int offset = dirLookup(parent, name);
    if(offset == -1) {
        dir.available = 0; 
        return dir;
    }

    /**  Get block  */
    uint32_t inumber = parent.table[offset].inumber;
    uint32_t blockId = inumber / Config::DIR_PER_BLOCK;
    uint32_t blockOffset = inumber % Config::DIR_PER_BLOCK;

    Block block;
    mountedDisk->readBlock(metaData.blocks - 1 - blockId, block.data);

    /**  Check Directory  */
    dir = block.directories[blockOffset];
    if(dir.available == 0) {
        return dir;
    }

    /** Check if it is root directory */
    if(strcmp(dir.name, currentDir.name) == 0) {
        printf("Current Directory cannot be removed.\n"); 
        dir.available = 0; 
        return dir;
    }

    Directory temp;
    /**  Remove all Dirent in the directory to be removed  */
    for(uint32_t ii = 0; ii < Config::ENTRIES_PER_DIR; ii++){
        if (ii > 1 && dir.table[ii].available == 1) {
            temp = remove(dir, dir.table[ii].name);

            if (temp.available == 0) {
                return temp;
            }

            dir = temp;
        }

        dir.table[ii].available = 0;
    }

    /**  Read the block again, because the block may have changed by DirEntry  */
    mountedDisk->readBlock(metaData.blocks - 1 - blockId, block.data);

    /**  Write down change of directory  */
    dir.available = 0;
    block.directories[blockOffset] = dir;
    mountedDisk->writeBlock(metaData.blocks - 1 - blockId, block.data);

    /**  Remove it from the parent  */
    parent.table[offset].available = 0;
    syncDirectory(parent);

    /**  Update the counter  */
    dirCounter[blockId]--;

    return parent;
}

Directory MyFS::remove(Directory dir, char name[Config::NAME_SIZE]){
    if(!mounted) { 
        dir.available = 0; 
        return dir;
    }

    /**   Get the offset for removal  */
    int offset = dirLookup(dir, name);
    if(offset == -1) {
        dir.available = 0; 
        return dir;
    }

    /**   Check if directory  */
    if (dir.table[offset].type == 0) {
        return removeDirectory(dir, name);
    }

    /**   Remove the inode by inumber  */
    uint32_t inumber = dir.table[offset].inumber;
    if(!removeInode(inumber)) {
        dir.available = 0; 
        return dir;
    }

    /**   Remove the entry and save the change to Volume  */
    dir.table[offset].available = 0;
    syncDirectory(dir);

    return dir;
}

bool MyFS::rmdir(char name[Config::NAME_SIZE]){
    Directory temp = removeDirectory(currentDir, name);

    if(temp.available == 0){
        currentDir = temp;

        return true;
    }

    return false;
}

bool MyFS::touch(char name[Config::NAME_SIZE]){
    if (!mounted) {
        return false;
    }

    /**   Check if such file exists  */
    for(uint32_t offset = 0; offset < Config::ENTRIES_PER_DIR; offset++) {
        DirEntry entry = currentDir.table[offset];
        
        if (entry.available){
            if (strcmp(entry.name, name) == 0){
                printf("File already exists\n");
                return false;
            }
        }
    }

    /**  Allocate new inode for the file  */
    ssize_t newNodeId = MyFS::createInode();
    if (newNodeId == -1) {
        printf("Error creating new inode\n"); 
        return false;
    }

    /**   Add the directory entry in the curr_directory  */
    Directory temp = addDirEntry(currentDir, newNodeId, 1, name);
    if(temp.available == 0) { 
        printf("Error adding new file\n"); 
        return false;
    }
    currentDir = temp;
    
    /**   Save the changes to Volume  */
    syncDirectory(currentDir);

    return true;
}

bool MyFS::cd(char name[Config::NAME_SIZE]){
    if(!mounted) {
        return false;
    }

    int offset = dirLookup(currentDir, name);
    if((offset == -1) || (currentDir.table[offset].type == 1)){
        printf("No such directory\n");
        return false;
    }

    /**   Read the dirblock from the disk  */
    Directory temp = readDirFromOffset(offset);
    if(temp.available == 0) {
        return false;
    }
    currentDir = temp;

    return true;
}

bool MyFS::ls(){
    if(!mounted) {
        return false;
    }

    /**   Get offset of current file directory   */
    char self[] = ".";
    int offset = dirLookup(currentDir, self);
    if (offset == -1) {
        return false;
    }

    /**   Get directory from offset  */
    Directory dir = readDirFromOffset(offset);

    /**   Check if directory is assigned  */
    if(dir.available == 0) { 
        return false;
    }

    /**   Listing all the entris of directory  */
    for(uint32_t i = 0; i < Config::ENTRIES_PER_DIR; i++){
        DirEntry entry = dir.table[i];

        if (entry.available == 0){
            continue;
        }

        if (entry.type == 1) {
            printf("%-10u   %-16s   %-5s\n", entry.inumber, entry.name, "file");
        } else {
            printf("%-10u   %-16s   %-5s\n", entry.inumber, entry.name, "directory");
        }
    }

    return true;
}

bool MyFS::rm(char name[]){
    Directory temp = remove(currentDir, name);

    if(temp.available == 1){
        currentDir = temp;
        return true;
    }

    return false;
}

void MyFS::exit(){
    if(!mounted) {
        return;
    }

    mountedDisk->unmount();
    mounted = false;
    mountedDisk = nullptr;
}

bool MyFS::outport(char name[],const char *path) {
    if(!mounted) {
        return false;
    }

    /** Get table offset for the filename */
    int offset = dirLookup(currentDir, name);
    if(offset == -1) {
        return false;
    }

    if(currentDir.table[offset].type == 0) {
        return false;
    }

    /** Open File for copyout */
    FILE *stream = fopen(path, "w");
    if (stream == nullptr) {
    	fprintf(stderr, "Unable to open %s: %s\n", path, strerror(errno));
    	return false;
    }

    /** Read from the inode and write it to the File */
    char buffer[4*BUFSIZ] = { 0 };
    uint32_t inumber = currentDir.table[offset].inumber;
    offset = 0;
    while (true) {
    	ssize_t result = read(inumber, buffer, sizeof(buffer), offset);
    	if (result <= 0) {
    	    break;
		}
		fwrite(buffer, 1, result, stream);
		offset += result;
    }
    
    printf("%d bytes copied\n", offset);
    fclose(stream);

    return true;
}

bool MyFS::import(const char *path, char name[]) {
    if(!mounted) {
        return false;
    }

    /** Check if file exists. Else create one */
    touch(name);
    int offset = dirLookup(currentDir, name);
    if(offset == -1) {
        return false;
    }

    if(currentDir.table[offset].type == 0) {
        return false;
    }
    

    /** Open File for reading */
	FILE *stream = fopen(path, "r");
    if (stream == nullptr) {
    	fprintf(stderr, "Unable to open %s: %s\n", path, strerror(errno));
    	return false;
    }

    /** Read File and get the Data */
    char buffer[4 * BUFSIZ] = {0};
    uint32_t inumber = currentDir.table[offset].inumber;
    offset = 0;
    while (true) {
    	ssize_t result = fread(buffer, 1, sizeof(buffer), stream);
    	if (result <= 0) {
    	    break;
	    }

        ssize_t actual = write(inumber, buffer, result, offset);
        if (actual < 0) {
            fprintf(stderr, "fs.write returned invalid result %ld\n", actual);
            break;
        }

        /** Checks to ensure proper write */
        offset += actual;
        if (actual != result) {
            fprintf(stderr, "fs.write only wrote %ld bytes, not %ld bytes\n", actual, result);
            break;
        }
    }
    
    printf("%d bytes copied\n", offset);
    fclose(stream);
    
    return true;
}

