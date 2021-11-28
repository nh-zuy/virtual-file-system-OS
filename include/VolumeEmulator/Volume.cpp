#include "Volume.h"
#include <stdexcept>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <string>

Volume::Volume() {
    fileDescriptor = 0; 
    blocks = 0; 
    mounts = 0;
}

Volume::~Volume() {
    close(fileDescriptor);
    fileDescriptor = 0;
}

void Volume::sanityCheck(int blockNumber, char *data) {
    char* error = NULL;

    if (blockNumber < 0) {
        strcpy(error, "Block number is negative!");
    } else if (blockNumber >= (int)blocks) {
        strcpy(error, "Block number is too big!");
    } else if (data == NULL) {
        strcpy(error, "null data pointer!");
    }

    if (error != NULL) {
        char what[BUFSIZ];
        snprintf(what, BUFSIZ, error);
        throw std::invalid_argument(what);
    }
}

void Volume::open(const char *path, size_t nblocks) {
    fileDescriptor = ::open(path, O_RDWR|O_CREAT, 0600);

    bool error = false;
    if (fileDescriptor < 0) {
    	error = true;
    } else if (ftruncate(fileDescriptor, nblocks * Config::BLOCK_SIZE) < 0) {
    	error = true;
    }

    if (error) {
        char what[BUFSIZ];
    	snprintf(what, BUFSIZ, "Unable to open %s: %s", path, strerror(errno));
    	throw std::runtime_error(what);
    }

    blocks = nblocks;
}

void Volume::readBlock(int blockNumber, char *data) {
    sanityCheck(blockNumber, data);

    char* error = NULL;
    if (lseek(fileDescriptor, blockNumber * Config::BLOCK_SIZE, SEEK_SET) < 0) {
        strcpy(error, "Unable to lseek %d: %s");
    } else if (::read(fileDescriptor, data, Config::BLOCK_SIZE) != Config::BLOCK_SIZE) {
        strcpy(error, "Unable to read %d: %s");
    }

    if (error != NULL) {
        char what[BUFSIZ];
    	snprintf(what, BUFSIZ, error, blockNumber, strerror(errno));
    	throw std::runtime_error(what);
    }
}

void Volume::writeBlock(int blockNumber, char *data) {
    sanityCheck(blockNumber, data);

    char* error = NULL;
    if (lseek(fileDescriptor, blockNumber * Config::BLOCK_SIZE, SEEK_SET) < 0) {
        strcpy(error, "Unable to lseek %d: %s");
    } else if (::write(fileDescriptor, data, Config::BLOCK_SIZE) != Config::BLOCK_SIZE) {
        strcpy(error, "Unable to write %d: %s");
    }

    if (error != NULL) {
        char what[BUFSIZ];
    	snprintf(what, BUFSIZ, error, blockNumber, strerror(errno));
    	throw std::runtime_error(what);
    }
}
