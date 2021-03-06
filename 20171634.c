#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "assemble.h"
#include "dir.h"
#include "dump.h"
#include "history.h"
#include "opcode.h"
#include "type.h"

static void help(const char* cmd)
{
    (void)cmd;

    puts("h[elp]");
    puts("d[ir]");
    puts("q[uit]");
    puts("hi[story]");
    puts("du[mp] [start, end]");
    puts("e[dit] address, value");
    puts("f[ill] start, end, value");
    puts("reset");

    // project 2
    puts("opcode mnemonic");
    puts("opcodelist");
    puts("assemble filename");
    puts("type filename");
    puts("symbol");

    // project 3
    puts("progaddr [address]");
    puts("loader [object filename1] [object filename2] [...]");
    puts("run");
    puts("bp [address]");
    puts("bp clear");
    puts("bp");
}

int main()
{
    initialize_hash_table();

    while (1) {
        printf("sicsim> ");

        char input[256], cmd[256];
        if (!fgets(input, 256, stdin)) {
            return 0;
        }

        // clear input buffer
        if (!strchr(input, '\n')) {
            scanf("%*[^\n]");
            scanf("%*c");
        }

        if (sscanf(input, "%255s", cmd) != 1) {
            continue;
        }

        void (*f)(const char* cmd);

        if (strcmp(cmd, "help") == 0 || strcmp(cmd, "h") == 0) {
            f = help;
        } else if (strcmp(cmd, "dir") == 0 || strcmp(cmd, "d") == 0) {
            f = dir;
        } else if (strcmp(cmd, "quit") == 0 || strcmp(cmd, "q") == 0) {
            break;
        } else if (strcmp(cmd, "history") == 0 || strcmp(cmd, "hi") == 0) {
            f = history;
        } else if (strcmp(cmd, "dump") == 0 || strcmp(cmd, "du") == 0) {
            f = dump;
        } else if (strcmp(cmd, "edit") == 0 || strcmp(cmd, "e") == 0) {
            f = edit;
        } else if (strcmp(cmd, "fill") == 0 || strcmp(cmd, "f") == 0) {
            f = fill;
        } else if (strcmp(cmd, "opcode") == 0) {
            f = opcode;
        } else if (strcmp(cmd, "opcodelist") == 0) {
            f = opcodelist;
        } else if (strcmp(cmd, "reset") == 0) {
            f = reset;
        } else if (strcmp(cmd, "type") == 0) {
            f = type;
        } else if (strcmp(cmd, "assemble") == 0) {
            f = assemble;
        } else if (strcmp(cmd, "symbol") == 0) {
            f = symbol;
        } else if (strcmp(cmd, "progaddr") == 0) {
            f = progaddr;
        } else if (strcmp(cmd, "loader") == 0) {
            f = loader;
        } else if (strcmp(cmd, "run") == 0) {
            f = run;
        } else if (strcmp(cmd, "bp") == 0) {
            f = breakpoint;
        } else {
            puts("No such comamnd.");
            continue;
        }

        add_history(input);
        f(input + strlen(cmd));
    }

    free_opcode_table();
    free_history();
    free_symbols();
    free_breakpoints();

    return 0;
}
