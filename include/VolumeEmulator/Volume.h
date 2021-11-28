#ifndef VOLUME_H
#define VOLUME_H

#include <stdlib.h>
#include "DataStructure/Config.h"

class Volume {
private:
    int	    fileDescriptor; // File descriptor of disk image
    size_t  blocks;	        // Number of blocks in disk image
    size_t  mounts;	        // Number of mounts

    /**
     * @brief Check parameters
     * @param blocknum Block to operate on
     * @param data Buffer to operate on
     * @exception Throws invalid_argument exception on error.
    **/
    void sanityCheck(int blocknum, char *data);
public:
    
    Volume();
            
    ~Volume();

    /**
     * @brief Return the number of blocks that could be handled.
    **/
    size_t size() const { return blocks; }

    /**
     * @brief Check if the volume has been mounted.
    **/
    bool isMounted() const { return mounts > 0; }

    /**
     * @brief Mounting the volume.
    **/
    void mount() { ++mounts; }

    /**
     * @brief Unmounting the volume.
    **/
    void unmount() 
    { 
        if (mounts > 0) {
            --mounts; 
        }
    }

    /**
     * @brief Open disk image
     * @param path Path to disk image
     * @param blocks Number of blocks in disk image
     * @exception Throws runtime_error exception on error.
    **/
    void open(const char *path, size_t blocks);

    /**
     * @brief Read block from disk
     * @param blockNumber Block to read from
     * @param data Buffer to read into
    **/
    void readBlock(int blockNumber, char *data);
    
    /** 
     * @brief Write block to disk
     * @param blockNumber Block to write to
     * @param data Buffer to write from
    **/
    void writeBlock(int blockNumber, char *data);
};

#endif