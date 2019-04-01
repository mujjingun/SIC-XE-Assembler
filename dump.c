#include <stdio.h>
#include <ctype.h>
#include <string.h>

static unsigned char mem[16 * 65536];
static int lastAddr = 0;

void dump(const char* cmd)
{
    int start, end;
    char ch1, ch2;
    int cnt = sscanf(cmd, "%x %c %x %c", &start, &ch1, &end, &ch2);

    if (cnt == EOF) {
        start = lastAddr;
        end = lastAddr / 16 * 16 + 160;
    }
    else if (cnt == 1) {
        end = start / 16 * 16 + 160;
    }
    else if (cnt == 3 && ch1 == ',') {
        end = end + 1;
    }
    else{
        puts("Invalid command.");
        return;
    }

    if (start < 0 || end < 0 || end <= start) {
        puts("Invalid command.");
        return;
    }

    lastAddr = end;

    if (end > (int)sizeof(mem)) {
        end = sizeof(mem);
    }

    for (int i = start / 16 * 16; i < (end + 15) / 16 * 16; ++i) {
        if (i % 16 == 0) {
            printf("%05X ", i);
        }

        if (i >= start && i < end) {
            printf("%02X ", mem[i]);
        }
        else {
            printf("   ");
        }

        if (i % 16 == 15) {
            printf("; ");

            int s = i / 16 * 16;
            for (int j = s; j < s + 16; ++j) {
                if (j >= start && isprint(mem[j])) {
                    printf("%c", mem[j]);
                }
                else {
                    printf(".");
                }
            }
            puts("");
        }
    }
}

void edit(const char *cmd)
{
    int addr, val;
    char ch;
    int cnt = sscanf(cmd, "%x , %x %c", &addr, &val, &ch);

    if (cnt != 2) {
        puts("Invalid command.");
        return;
    }

    if (addr < 0 || addr >= (int)sizeof(mem)) {
        puts("Invalid address.");
        return;
    }

    if (val < 0 || val > 0xff) {
        puts("Invalid value.");
        return;
    }

    mem[addr] = (unsigned char)val;
}

void fill(const char *cmd)
{
    int start, end, val;
    char ch;
    int cnt = sscanf(cmd, "%x , %x , %x %c", &start, &end, &val, &ch);

    if (cnt != 3) {
        puts("Invalid command.");
        return;
    }

    if (start < 0 || end < 0) {
        puts("Invalid command.");
        return;
    }

    end = end + 1;

    if (end > (int)sizeof(mem) || start >= (int)sizeof(mem) || end <= start) {
        puts("Invalid range");
        return;
    }

    if (val < 0 || val > 0xff) {
        puts("Invalid value.");
        return;
    }

    for (int i = start; i < end; ++i) {
        mem[i] = (unsigned char)val;
    }
}

void reset(const char *cmd)
{
    char ch;
    if (sscanf(cmd, " %c", &ch) == 1) {
        puts("Invalid command.");
        return;
    }

    memset(mem, 0, sizeof(mem));
}
