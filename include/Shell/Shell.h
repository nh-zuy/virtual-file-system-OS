#ifndef SHELL_H
#define SHELL_H

#include <sstream>
#include <string>
#include <stdexcept>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "VolumeEmulator/Volume.h"
#include "FileSystem/MyFS.h"

class Shell {
private:
    Volume disk;
    MyFS fileSystem;

public:
    /** Inversion of Control */
    Shell(Volume& disk, MyFS& fileSystem) {
        this->disk = disk;
        this->fileSystem = fileSystem;
    }

    bool format() {
        return fileSystem.format(&disk);
    }

    bool mount() {
        return fileSystem.mount(&disk);
    }

    bool changePassword() {
        return fileSystem.changePassword();
    }

    bool setPassword() {
        return fileSystem.setPassword();
    }

    bool removePassword() {
        return fileSystem.removePassword();
    }

    bool changePasswordFile(char name[]) {
        return fileSystem.changePasswordFile(name);
    }

    bool setPasswordFile(char name[]) {
        return fileSystem.setPasswordFile(name);
    }
    
    bool mkdir(char* dirName) {
        return fileSystem.mkdir(dirName);
    }

    bool rmdir(char* dirName) {
        return fileSystem.rmdir(dirName);
    }

    bool touch(char* name) {
        return fileSystem.touch(name);
    }

    bool rm(char* name) {
        return fileSystem.rm(name);
    }

    bool outport(char* fileName, char* path) {
        return fileSystem.outport(fileName, path);
    }

    bool import(char* path, char* fileName) {
        return fileSystem.import(path, fileName);
    }

    bool cd(char* dirName) {
        return fileSystem.cd(dirName);
    }

    bool ls() {
        return fileSystem.ls();
    }
};

#endif