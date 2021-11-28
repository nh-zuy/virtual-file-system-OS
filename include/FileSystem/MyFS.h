#ifndef FILE_SYSTEM_H
#define FILE_SYSTEM_H

#include <cstring>
#include <stdint.h>
#include <vector>

#include "VolumeEmulator/Volume.h"
#include "DataStructure/Block.h"

class MyFS {
private:
    /** MyFS.Dat */
    Volume* mountedDisk;

    /** Metadata for File System */
    MetaBlock metaData;

    /** Check if file system has mounted */
    bool mounted;

    /** Inode Bitmap emulator **/
    std::vector<bool> freeBlocks;
    std::vector<int> inodeCounter;

    /** Current directory in travel */
    Directory currentDir;

    /** Map for bookkepping existed directory */
    std::vector<uint32_t> dirCounter;

    /**
     * @brief Create new empty inode.
     **/
    ssize_t createInode();

    /**
     * @brief Remove the inode has inumber.
     **/
    bool removeInode(size_t inumber);

    /**
     * @brief Check if the inode existed.
     **/
    ssize_t stat(size_t inumber);

    /**
     * @brief Read data from Data Block by inumber.
     **/
    ssize_t read(size_t inumber, char *data, int length, size_t offset);

    /**
     * @brief Write data to Data Block by inumber.
     **/
    ssize_t write(size_t inumber, char *data, int length, size_t offset);

    /**
     * @brief Get Inode Block by inumber.
     **/
    bool loadInode(size_t inumber, Inode* inode);

    /**
     * @brief Read data of Data Block with length size from offset.
     **/
    void readDataFromOffset(uint32_t blocknum, int offset, int *length, char **data, char **ptr);
    /**
     * @brief Write data to Data Block and return length data.
     **/
    ssize_t writeAndReturnSize(size_t inumber, Inode* node, int ret);
    
    /**
     * @brief Read data of Data Block with length size from offset.
     **/
    void readBuffer(int offset, int *read, int length, char *data, uint32_t blocknum);

    /**
     * @brief Check if inumber of Block is valid.
     **/
    bool checkAllocation(Inode *node, int read, int orig_offset, uint32_t &blocknum, bool 
            write_indirect, Block indirect);

    /**
     * @brief Allocate first empty block.
     **/
    uint32_t allocateBlock();

    /**
     * @brief Insert entry to directory.
     **/
    Directory addDirEntry(Directory dir, uint32_t inum, uint32_t type, char name[]);

    /**
     * @brief Save change of directory to Volume.
     **/
    void syncDirectory(Directory dir);

    /**
     * @brief Get directory from routing table.
     **/
    int dirLookup(Directory dir, char name[]);
    
    /**
     * @brief Read directory from offset.
     **/
    Directory readDirFromOffset(uint32_t offset);

    /**
     * @brief Remove specific directory by it's name.
     **/
    Directory removeDirectory(Directory parent, char name[]);
    
    /**
     * @brief Remove specific item by it's name.
     **/
    Directory remove(Directory parent, char name[]);

public:
    /**
     * @brief Format the volume.
     **/
    static bool format(Volume *disk);

    /**
     * @brief Mount the volume to File System.
     **/
    bool mount(Volume *disk);

    /**
     * @brief Create password for Volume.
     **/
    bool setPassword();

    /**
     * @brief Change password of Volume.
     **/
    bool changePassword();

    /**
     * @brief Remove password of Volume.
     **/
    bool removePassword();

    /**
     * @brief Creat password for file.
     **/
    bool setPasswordFile(char name[]);

    /**
     * @brief Change password of file.
     **/
    bool changePasswordFile(char name[]);

    /**
     * @brief Creat empty file in current directory.
     **/
    bool touch(char name[]);

    /**
     * @brief Creat new directory in current directory.
     **/
    bool mkdir(char name[]);

    /**
     * @brief Remove directory in current directory.
     **/
    bool rmdir(char name[]);

    /**
     * @brief Moving to directory in current directory.
     **/
    bool cd(char name[]);

    /**
     * @brief Listing all entities in current directory.
     **/
    bool ls();

    /**
     * @brief Remove file in current directory.
     **/
    bool rm(char name[]);

    /**
     * @brief Export a file in current directory to outside.
     **/
    bool outport(char name[],const char *path);

    /**
     * @brief Import file from outside to current directory.
     **/
    bool import(const char *path, char name[]);

    /**
     * @brief Just exit the MyFS.
     **/
    void exit();
};

#endif
