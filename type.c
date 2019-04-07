#include "type.h"

#include <stdio.h>

void type(const char* cmd)
{
    char ch, file[100];
    if (sscanf(cmd, "%99s %c", file, &ch) != 1) {
        printf("Invalid command.\n");
        return;
    }

    FILE* fp = fopen(file, "r");
    if (!fp) {
        printf("Cannot open file %s.\n", file);
        return;
    }

    char line[4096];
    while (fgets(line, sizeof(line), fp)) {
        printf("%s", line);
    }

    fclose(fp);
}
