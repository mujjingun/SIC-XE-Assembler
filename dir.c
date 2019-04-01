#include "dir.h"

#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>

void dir(const char* cmd)
{
    char ch;
    if (sscanf(cmd, " %c", &ch) == 1) {
        puts("Invalid command.");
        return;
    }

    DIR* dir = opendir(".");
    if (dir) {
        struct dirent* ent;
        for (int i = 0; (ent = readdir(dir)); ++i) {
            if (i > 0 && i % 4 == 0) {
                puts("");
            }
            if (ent->d_type == DT_DIR) {
                printf("%s/\t", ent->d_name);
            } else {
                int fd = open(ent->d_name, O_RDONLY);
                struct stat stat;
                fstat(fd, &stat);
                if ((stat.st_mode & S_IXUSR) || (stat.st_mode & S_IXGRP) || (stat.st_mode & S_IXOTH)) {
                    printf("%s*\t", ent->d_name);
                } else {
                    printf("%s\t", ent->d_name);
                }
                close(fd);
            }
        }
        puts("");
        closedir(dir);
    } else {
        puts("Error opening directory.");
    }
}
