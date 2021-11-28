#include <iostream>
#include <string>

#include "VolumeEmulator/Volume.h"
#include "FileSystem/MyFS.h"
#include "Shell/Shell.h"
#include "Shell/CommandType.h"

Command convertToCommand(char* cmd);
bool startUpDisk(Volume& disk, const char* imagePath, const int& blocks);
bool handlePassword(Shell& shell, char* flag);
bool handlePassword(Shell& shell, char* flag, char* file);

int main(int argc, char* argv[]) {
    Volume disk;
    MyFS fileSystem;

    if (argc != 3) {
        fprintf(stderr, "[?] Format: %s <file> <blocks>\n", argv[0]);
    	return EXIT_FAILURE;
    }

    if (!startUpDisk(disk, argv[1], std::atoi(argv[2]))) {
        return EXIT_FAILURE;
    }

    Shell shell(disk, fileSystem);
    bool status = true;
    while (status) {
        char line[BUFSIZ], 
            cmd[BUFSIZ], 
            arg1[BUFSIZ], 
            arg2[BUFSIZ];

        fprintf(stderr, "3d> ");
    	fflush(stderr);

        if (fgets(line, BUFSIZ, stdin) == NULL) {
    	    break;
    	}

        int args = sscanf(line, "%s %s %s", cmd, arg1, arg2);
    	if (args == 0) {
    	    continue;
	    }

        Command command = convertToCommand(cmd);
        switch(command) {
            case FORMAT:
                if (shell.format()) {
                    std::cout << "[*] Formatted" << std::endl;
                } else {
                    std::cout << "[!] Error: Unable format disk!" << std::endl;
                }
                break;

            case MOUNT:
                if (shell.mount()) {
                    std::cout << "[*] Mounted -> Volume: root." << std::endl;
                } else {
                    std::cout << "[!] Error: Unable mount disk!" << std::endl;
                }
                break;

            case PASSWORD:
                if (handlePassword(shell, arg1)) {
                    std::cout << "[*] Successfully." << std::endl;
                } else {
                    std::cout << "[!] Error." << std::endl;
                }
                break;

            case MKDIR:
                if (shell.mkdir(arg1)) {
                    std::cout << "[*] Created." << std::endl;
                } else {
                    std::cout << "[!] Unable to create." << std::endl;
                }
                break;

            case RMDIR:
                if (shell.rmdir(arg1)) {
                    std::cout << "[*] Deleted." << std::endl;
                } else {
                    std::cout << "[!] Unable to delete." << std::endl;
                }
                break;

            case TOUCH:
                if (shell.touch(arg1)) {
                    std::cout << "[*] Created." << std::endl;
                } else {
                    std::cout << "[!] Unable to create." << std::endl;
                }
                break;

            case RM:
                if (shell.rm(arg1)) {
                    std::cout << "[*] Deleted." << std::endl;
                } else {
                    std::cout << "[!] Unable to delete." << std::endl;
                }
                break;

            case CD:
                if (!shell.cd(arg1)) {
                    std::cout << "[!] No directory.";
                }
                break;

            case LS:
                shell.ls();
                break;

            case OUTPORT:
                if (shell.outport(arg1, arg2)) {
                    std::cout << "[*] Successfully." << std::endl;
                } else {
                    std::cout << "[!] Error." << std::endl;
                }
                break;

            case IMPORT:
                if (shell.import(arg1, arg2)) {
                    std::cout << "[*] Successfully." << std::endl;
                } else {
                    std::cout << "[!] Error." << std::endl;
                }
                break;

            case EXIT:
                status = false;
                break;
                
            default:
                std::cout << "[!] Unknown command." << std::endl;
        };
    }

    return 0;
}

Command convertToCommand(char* cmd) {
    if (strcmp(cmd, "format") == 0) {
	    return FORMAT;
	} else if (strcmp(cmd, "mount") == 0) {
	    return MOUNT;
	} else if (strcmp(cmd, "password") == 0) {
	    return PASSWORD;
	} else if (strcmp(cmd, "mkdir")== 0) {
	    return MKDIR;
	} else if (strcmp(cmd, "rmdir")== 0) {
	    return RMDIR;
	} else if (strcmp(cmd, "touch")== 0) {
        return TOUCH;
	} else if (strcmp(cmd, "rm")== 0) {
        return RM;
	} else if (strcmp(cmd, "cd")== 0) {
        return CD;
	} else if (strcmp(cmd, "ls")== 0) {
        return LS;
	} else if (strcmp(cmd, "outport")== 0) {
        return OUTPORT;
	} else if (strcmp(cmd, "import")== 0) {
        return IMPORT;
	} else if (strcmp(cmd, "exit")== 0 || strcmp(cmd, "quit")== 0) {
	    return EXIT;
	};

    return WAITING;
}

bool startUpDisk(Volume& disk, const char* imagePath, const int& blocks) {
    try {
        disk.open(imagePath, blocks);
    } catch(std::runtime_error &e) {
        fprintf(stderr, "[!] Error: Cannot open disk %s / %s\n", imagePath, e.what());
    	return false;
    }

    return true;
}

bool handlePassword(Shell& shell, char* flag) {
    if (strcmp(flag, "-s") == 0) {
        return shell.setPassword();
    } else if (strcmp(flag, "-c") == 0) {
        return shell.changePassword();
    } else if (strcmp(flag, "-r") == 0) {
        return shell.removePassword();
    }

    return false;
}

bool handlePassword(Shell& shell, char* flag, char* file) {
    if (strcmp(flag, "-s") == 0) {
        return shell.setPasswordFile(file);
    } else if (strcmp(flag, "-c") == 0) {
        return shell.changePasswordFile(file);
    }

    return false;
}