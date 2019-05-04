#include "dump.h"
#include "symtab.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NDEBUG

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
    } else if (cnt == 1) {
        end = start / 16 * 16 + 160;
    } else if (cnt == 3 && ch1 == ',') {
        end = end + 1;
    } else {
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
        } else {
            printf("   ");
        }

        if (i % 16 == 15) {
            printf("; ");

            int s = i / 16 * 16;
            for (int j = s; j < s + 16; ++j) {
                if (j >= start && isprint(mem[j])) {
                    printf("%c", mem[j]);
                } else {
                    printf(".");
                }
            }
            puts("");
        }
    }
}

void edit(const char* cmd)
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

void fill(const char* cmd)
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

void reset(const char* cmd)
{
    char ch;
    if (sscanf(cmd, " %c", &ch) == 1) {
        puts("Invalid command.");
        return;
    }

    memset(mem, 0, sizeof(mem));
}

static int progAddr = 0;
struct registers {
    int A;
    int X;
    int L;
    int PC;
    int B;
    int S;
    int T;
    int SW;
};
static struct registers reg = { 0, 0, 0, 0, 0, 0, 0, 0 };

void progaddr(const char* cmd)
{
    int addr;
    char ch;
    int cnt = sscanf(cmd, "%x %c", &addr, &ch);

    if (cnt != 1) {
        puts("Invalid command");
        return;
    }

    if (addr < 0 || addr >= (int)sizeof(mem)) {
        puts("Invalid address");
        return;
    }

    progAddr = addr;
}

struct extdef {
    char name[10];
    int addr;
};

struct extref {
    char name[10];
    int idx;
};

struct textrec {
    unsigned char text[32];
    int addr, len;
};

struct modify {
    int addr, len, idx;
    int sign;
};

struct sect {
    char name[10];
    int length;
    int nrefs;
    struct extref* refs;
    int ndefs;
    struct extdef* defs;
    int ntextrecs;
    struct textrec* textrecs;
    int nmodify;
    struct modify* modifys;
    int entry;
};

static int parse_obj(const char* filename, struct sect* sect)
{
    FILE* fp = fopen(filename, "r");
    if (!fp) {
        printf("Error: error opening file %s\n", filename);
        return -1;
    }

    if (fscanf(fp, "H%6s", sect->name) != 1) {
        printf("Error: error parsing program name\n");
        return -1;
    }

    if (fscanf(fp, "%x ", &sect->length) != 1) {
        printf("Error: error parsing program length\n");
        return -1;
    }

    char line[256];

    sect->nrefs = 0;
    sect->refs = NULL;
    sect->ndefs = 0;
    sect->defs = NULL;
    sect->ntextrecs = 0;
    sect->textrecs = NULL;
    sect->nmodify = 0;
    sect->modifys = NULL;
    sect->entry = -1;

    while (fgets(line, 256, fp)) {
        if (line[0] == 'R') {
            sect->nrefs = (int)((strlen(line) - 2 + 7) / 8);
            sect->refs = malloc(sizeof(struct extref) * sect->nrefs);
            int n = 1;
            for (int i = 0; i < sect->nrefs; ++i) {
                int m;
                if (sscanf(line + n, "%2x%6s%n", &sect->refs[i].idx, sect->refs[i].name, &m) != 2) {
                    printf("Error: cannot parse external reference record\n");
                    return -1;
                }
                n += m;
            }
        } else if (line[0] == 'D') {
            sect->ndefs = (int)((strlen(line) - 2 + 11) / 12);
            sect->defs = malloc(sizeof(struct extdef) * sect->ndefs);
            int n = 1;
            for (int i = 0; i < sect->ndefs; ++i) {
                int m;
                if (sscanf(line + n, "%6s%6x%n", sect->defs[i].name, &sect->defs[i].addr, &m) != 2) {
                    printf("Error: cannot parse external definition record\n");
                    return -1;
                }
                n += m;
            }
        } else if (line[0] == 'T') {
            int n = 9, i = 0;
            int hex;
            struct textrec curr;

            if (sscanf(line + 1, "%6x%2x", &curr.addr, &curr.len) != 2) {
                printf("Error: cannot parse text record\n");
                return -1;
            }

            while (sscanf(line + n, "%2x", &hex) == 1) {
                curr.text[i++] = hex & 0xff;
                n += 2;
            }

            // grow text record array
            sect->ntextrecs++;
            sect->textrecs = realloc(sect->textrecs, sizeof(struct textrec) * sect->ntextrecs);
            sect->textrecs[sect->ntextrecs - 1] = curr;

        } else if (line[0] == 'M') {
            struct modify curr;

            if (sscanf(line + 1, "%6x%2x+%2x", &curr.addr, &curr.len, &curr.idx) == 3) {
                curr.sign = 1;
            } else if (sscanf(line + 1, "%6x%2x-%2x", &curr.addr, &curr.len, &curr.idx) == 3) {
                curr.sign = -1;
            } else {
                printf("Error: cannot parse modification record\n");
                return -1;
            }

            sect->nmodify++;
            sect->modifys = realloc(sect->modifys, sizeof(struct modify) * sect->nmodify);
            sect->modifys[sect->nmodify - 1] = curr;

        } else if (line[0] == 'E') {
            sscanf(line + 1, "%6x", &sect->entry);
        }
    }

    fclose(fp);
    return 0;
}

static void free_obj(struct sect* prog)
{
    free(prog->refs);
    free(prog->defs);
    free(prog->textrecs);
    free(prog->modifys);
}

void loader(const char* cmd)
{
    char files[3][100];
    int cnt = sscanf(cmd, "%99s %99s %99s", files[0], files[1], files[2]);
    if (cnt == 0 || cnt == EOF) {
        puts("Error: Invalid command\n");
        return;
    }

    symtab tab = symtab_init();
    struct sect* sects = malloc(sizeof(struct sect) * cnt);

    // first pass
    int csaddr = progAddr;
    for (int i = 0; i < cnt; ++i) {
        if (parse_obj(files[i], &sects[i]) == -1) {
            goto cleanup;
        }

        if (symtab_find(tab, sects[i].name) != -1) {
            printf("Error: Control section '%s' already exists\n", sects[i].name);
            goto cleanup;
        }

        symtab_insert(tab, sects[i].name, csaddr);

        for (int j = 0; j < sects[i].ndefs; ++j) {
            if (symtab_find(tab, sects[i].defs[j].name) != -1) {
                printf("Error: Duplicate external symbol '%s'\n", sects[i].defs[j].name);
                goto cleanup;
            }
            symtab_insert(tab, sects[i].defs[j].name, csaddr + sects[i].defs[j].addr);
        }

        csaddr += sects[i].length;
    }

    if (csaddr > (int)sizeof(mem)) {
        printf("Error: Invalid load address\n");
        goto cleanup;
    }

    // second pass
    csaddr = progAddr;
    reg.PC = progAddr;
    puts("control   symbol    address   length");
    puts("secion    name");
    puts("-------------------------------------");

    for (int i = 0; i < cnt; ++i) {

        printf("%-10s%-10s%04X%6s%04X\n", sects[i].name, "", csaddr, "", sects[i].length);

        for (int j = 0; j < sects[i].ntextrecs; ++j) {
            memcpy(mem + csaddr + sects[i].textrecs[j].addr,
                sects[i].textrecs[j].text, sects[i].textrecs[j].len);
        }

        for (int j = 0; j < sects[i].nmodify; ++j) {
            int offset = csaddr + sects[i].modifys[j].addr;
            int orig = mem[offset] << 16 | mem[offset + 1] << 8 | mem[offset + 2];
            int val = -1;

            // reference number 01 = control section name
            if (sects[i].modifys[j].idx == 0x01) {
                val = csaddr;
            }
            for (int k = 0; k < sects[i].nrefs; ++k) {
                if (sects[i].refs[k].idx == sects[i].modifys[j].idx) {
                    val = symtab_find(tab, sects[i].refs[k].name);
                    break;
                }
            }
            if (val == -1) {
                printf("Error: cannot find reference #%d\n", sects[i].modifys[j].idx);
                goto cleanup;
            }
            orig += sects[i].modifys[j].sign * val;
            mem[offset] = (orig >> 16) & 0xff;
            mem[offset + 1] = (orig >> 8) & 0xff;
            mem[offset + 2] = orig & 0xff;
        }

        for (int j = 0; j < sects[i].ndefs; ++j) {
            printf("%-10s%-10s%04X\n", "", sects[i].defs[j].name, csaddr + sects[i].defs[j].addr);
        }

        if (sects[i].entry != -1) {
            reg.PC = csaddr + sects[i].entry;
        }

        csaddr += sects[i].length;
    }

    puts("-------------------------------------");
    printf("                 total length %04X\n", csaddr - progAddr);

cleanup:
    for (int i = 0; i < cnt; ++i) {
        free_obj(&sects[i]);
    }
    free(sects);

    symtab_free(tab);
}

static int nbreakpoints = 0;
static int* breakpoints = NULL;

void breakpoint(const char* cmd)
{
    int addr;
    char ch;
    int cnt = sscanf(cmd, "%x %c", &addr, &ch);
    if (cnt == 1) {
        if (addr < 0 || addr >= (int)sizeof(mem)) {
            printf("Error: Address out of range\n");
        }

        nbreakpoints++;
        breakpoints = realloc(breakpoints, sizeof(int) * nbreakpoints);
        breakpoints[nbreakpoints - 1] = addr;

        printf("\t[ok] create breakpoint %04X\n", addr);
    } else if (cnt == EOF) {
        printf("\tbreakpoint\n");
        printf("\t----------\n");
        for (int i = 0; i < nbreakpoints; ++i) {
            printf("\t%04X\n", breakpoints[i]);
        }
    } else {
        char clear[10];
        cnt = sscanf(cmd, "%9s %c", clear, &ch);
        if (cnt == 1 && strcmp(clear, "clear") == 0) {
            free_breakpoints();
            printf("\t[ok] clear all breakpoints\n");
        } else {
            printf("Error: Invalid command\n");
            return;
        }
    }
}

void free_breakpoints()
{
    nbreakpoints = 0;
    free(breakpoints);
    breakpoints = NULL;
}

static void print_registers()
{
    printf("\tA : %06X X : %06X\n", reg.A & 0xffffff, reg.X & 0xffffff);
    printf("\tL : %06X PC: %06X\n", reg.L & 0xffffff, reg.PC & 0xffffff);
    printf("\tB : %06X S : %06X\n", reg.B & 0xffffff, reg.S & 0xffffff);
    printf("\tT : %06X\n", reg.T & 0xffffff);
}

static int set_memory(int addr, int val)
{
    if (addr < 0 || addr >= (int)sizeof(mem)) {
        return -1;
    }
    mem[addr] = (val >> 16) & 0xff;
    mem[addr + 1] = (val >> 8) & 0xff;
    mem[addr + 2] = val & 0xff;

#ifndef NDEBUG
    printf("set memory at %06X to %06X\n", addr, val);
#endif
    return 0;
}

static int* get_register(int n)
{
    switch (n) {
    case 0:
        return &reg.A;
    case 1:
        return &reg.X;
    case 2:
        return &reg.L;
    case 8:
        return &reg.PC;
    case 9:
        return NULL; // SW
    case 3:
        return &reg.B;
    case 4:
        return &reg.S;
    case 5:
        return &reg.T;
    case 6:
        return NULL; // F
    }
    return NULL;
}

static int run_format_1()
{
    return -1;
}

static int compare(int a, int b)
{
    return a < b ? -1 : (a == b ? 0 : 1);
}

static int run_format_2()
{
    int opcode = mem[reg.PC] & 0xfc;
    int r1 = (mem[reg.PC + 1] >> 4) & 0x0f;
    int r2 = mem[reg.PC + 1] & 0x0f;
    int* p1 = get_register(r1);
    int* p2 = get_register(r2);

    if (!p1 || !p2) {
        printf("Error: Invalid register.\n");
        return -1;
    }

    // sign-extend
    if (*p1 & 0x800000) {
        *p1 |= 0xff000000;
    }
    if (*p2 & 0x800000) {
        *p2 |= 0xff000000;
    }

    reg.PC += 2;

    switch (opcode) {
    // ADDR
    case 0x90:
        *p2 = *p2 + *p1;
        break;

    // CLEAR
    case 0xb4:
        *p1 = 0;
        break;

    // COMPR
    case 0xa0:
        reg.SW = compare(*p1, *p2);
        break;

    // DIVR
    case 0x9c:
        *p2 = *p2 / *p1;
        break;

    // MULR
    case 0x98:
        *p2 = *p2 * *p1;
        break;

    // RMO
    case 0xac:
        *p2 = *p1;
        break;

    // SHIFTL
    case 0xa4:
        return -1;

    // SHIFTR
    case 0xa8:
        return -1;

    // SUBR
    case 0x94:
        *p2 = *p2 - *p1;
        break;

    // SVC
    case 0xb0:
        return -1;

    // TIXR
    case 0xb8:
        reg.X++;
        reg.SW = compare(reg.X, *p1);
        break;
    }
    return 0;
}

static int read_count = 0;

static int run_format_3_4()
{
    int opcode = mem[reg.PC] & 0xfc;
    int n, i, x, b, p, e, disp, val, addr;
    n = (mem[reg.PC] & 0x02) != 0;
    i = (mem[reg.PC] & 0x01) != 0;

    if (!n && !i) {

        printf("Error: SIC compat instruction\n");
        return -1;

        /*
        x = (mem[reg.PC + 1] & 0x80) != 0;
        b = p = e = 0;
        disp = (mem[reg.PC + 1] & 0x7f) << 8 | mem[reg.PC + 2];
        addr = disp;

        reg.PC += 3;

        if (addr < 0 || addr >= (int)sizeof(mem)) {
            printf("Error: Address out of range\n");
            return -1;
        }
        val = mem[addr];
        */

    } else {
        x = (mem[reg.PC + 1] & 0x80) != 0;
        b = (mem[reg.PC + 1] & 0x40) != 0;
        p = (mem[reg.PC + 1] & 0x20) != 0;
        e = (mem[reg.PC + 1] & 0x10) != 0;

        if (e == 0) {
            disp = (mem[reg.PC + 1] & 0x0f) << 8 | mem[reg.PC + 2];
            if (disp & 0x800) {
                disp |= 0xfffff000;
            }
            reg.PC += 3;
        } else {
            disp = (mem[reg.PC + 1] & 0x0f) << 16 | mem[reg.PC + 2] << 8 | mem[reg.PC + 3];
            if (disp & 0x80000) {
                disp |= 0xfff00000;
            }
            reg.PC += 4;
        }

        addr = disp;

        if (x) {
            if (reg.X & 0x800000) {
                reg.X |= 0xff000000;
            }
            addr += reg.X;
        }

        if (p) {
            addr += reg.PC;
        }

        if (b) {
            addr += reg.B;
        }

        if (n && i) {
            // simple addressing
            if (addr < 0 || addr >= (int)sizeof(mem)) {
                printf("Error: Address out of range\n");
                return -1;
            }
            val = (mem[addr] << 16) | (mem[addr + 1] << 8) | mem[addr + 2];
        } else if (n && !i) {
            // indirect addressing
            if (addr < 0 || addr >= (int)sizeof(mem)) {
                printf("Error: Address out of range\n");
                return -1;
            }
            addr = (mem[addr] << 16) | (mem[addr + 1] << 8) | mem[addr + 2];
            if (addr < 0 || addr >= (int)sizeof(mem)) {
                printf("Error: Address out of range\n");
                return -1;
            }
            val = (mem[addr] << 16) | (mem[addr + 1] << 8) | mem[addr + 2];
        } else {
            // immediate addressing
            val = addr;
        }
    }

    if (val & 0x800000) {
        val |= 0xff000000;
    }

#ifndef NDEBUG
    printf("n = %d, i = %d, x = %d, b = %d, p = %d, e = %d\n", n, i, x, b, p, e);
    printf("disp = %06X\n", disp);
    printf("addr = %06X, val = %06X\n", addr, val);
#endif

    switch (opcode) {
    // ADD
    case 0x18:
        reg.A = reg.A + val;
        break;

    // ADDF
    case 0x58:
        return -1;

    // AND
    case 0x40:
        reg.A = reg.A & val;
        break;

    // COMP
    case 0x28:
        reg.SW = compare(reg.A, val);
        break;

    // COMPF
    case 0x88:
        return -1;

    // DIV
    case 0x24:
        reg.A = reg.A / val;
        break;

    // DIVF
    case 0x64:
        return -1;

    // J
    case 0x3c:
        reg.PC = addr;
        break;

    // JEQ
    case 0x30:
        if (reg.SW == 0) {
            reg.PC = addr;
        }
        break;

    // JGT
    case 0x34:
        if (reg.SW > 0) {
            reg.PC = addr;
        }
        break;

    // JLT
    case 0x38:
        if (reg.SW < 0) {
            reg.PC = addr;
        }
        break;

    // JSUB
    case 0x48:
        reg.L = reg.PC;
        reg.PC = addr;
        break;

    // LDA
    case 0x00:
        reg.A = val;
        break;

    // LDB
    case 0x68:
        reg.B = val;
        break;

    // LDCH
    case 0x50:
        reg.A &= ~0xff;
        reg.A |= (val >> 16) & 0xff;
        break;

    // LDF
    case 0x70:
        return -1;

    // LDL
    case 0x08:
        reg.L = val;
        break;

    // LDS
    case 0x6c:
        reg.S = val;
        break;

    // LDT
    case 0x74:
        reg.T = val;
        break;

    // LDX
    case 0x04:
        reg.X = val;
        break;

    // LPS
    case 0xd0:
        return -1;

    // MUL
    case 0x20:
        reg.A = reg.A * val;
        break;

    // MULF
    case 0x60:
        return -1;

    // OR
    case 0x44:
        reg.A = reg.A | val;
        break;

    // RD
    case 0xd8:
        // TODO: read data from _somewhere_
        if (read_count < 100) {
            reg.A &= ~0xff;
            reg.A |= 0xff;
            read_count++;
        } else {
            reg.A &= ~0xff;
            reg.A |= 0x00;
        }
        break;

    // RSUB
    case 0x4c:
        reg.PC = reg.L;
        break;

    // SSK
    case 0xec:
        return -1;

    // STA
    case 0x0c:
        if (set_memory(addr, reg.A) == -1) {
            return -1;
        }
        break;

    // STB
    case 0x78:
        if (set_memory(addr, reg.B) == -1) {
            return -1;
        }
        break;

    // STCH
    case 0x54:
        if (addr < 0 || addr >= (int)sizeof(mem)) {
            printf("Error: Address out of range\n");
            return -1;
        }
        mem[addr] = reg.A & 0xff;
        break;

    // STF
    case 0x80:
        return -1;

    // STI
    case 0xd4:
        return -1;

    // STL
    case 0x14:
        if (set_memory(addr, reg.L) == -1) {
            return -1;
        }
        break;

    // STS
    case 0x7c:
        if (set_memory(addr, reg.S) == -1) {
            return -1;
        }
        break;

    // STSW
    case 0xe8:
        return -1;

    // STT
    case 0x84:
        if (set_memory(addr, reg.T) == -1) {
            return -1;
        }
        break;

    // STX
    case 0x10:
        if (set_memory(addr, reg.X) == -1) {
            return -1;
        }
        break;

    // SUB
    case 0x1c:
        reg.A = reg.A - val;
        break;

    // SUBF
    case 0x5c:
        return -1;

    // TD
    case 0xe0:
        reg.SW = -1; // <
        break;

    // TIX
    case 0x2c:
        reg.X++;
        reg.SW = compare(reg.X, val);
        break;

    // WD
    case 0xdc:
        // TODO: write better
        printf("Write: %02X\n", reg.A & 0xff);
        break;
    }
    return 0;
}

static int run_instr()
{
    int opcode = mem[reg.PC] & 0xfc;

#ifndef NDEBUG
    printf("%06X %02X\n", reg.PC, opcode);
    print_registers();
#endif
    switch (opcode) {

    // format 1
    case 0xc4:
    case 0xc0:
    case 0xf4:
    case 0xc8:
    case 0xf0:
    case 0xf8:
        if (run_format_1() == -1) {
            printf("Error: Error while running instruction %02X\n", opcode);
            return -1;
        }
        break;

    // format 2
    case 0x90:
    case 0xb4:
    case 0xa0:
    case 0x9c:
    case 0x98:
    case 0xac:
    case 0xa4:
    case 0xa8:
    case 0x94:
    case 0xb0:
    case 0xb8:
        if (run_format_2() == -1) {
            printf("Error: Error while running instruction %02X\n", opcode);
            return -1;
        }
        break;

    // format 3
    case 0x18:
    case 0x58:
    case 0x40:
    case 0x28:
    case 0x88:
    case 0x24:
    case 0x64:
    case 0x3c:
    case 0x30:
    case 0x34:
    case 0x38:
    case 0x48:
    case 0x00:
    case 0x68:
    case 0x50:
    case 0x70:
    case 0x08:
    case 0x6c:
    case 0x74:
    case 0x04:
    case 0xd0:
    case 0x20:
    case 0x60:
    case 0x44:
    case 0xd8:
    case 0x4c:
    case 0xec:
    case 0x0c:
    case 0x78:
    case 0x54:
    case 0x80:
    case 0xd4:
    case 0x14:
    case 0x7c:
    case 0xe8:
    case 0x84:
    case 0x10:
    case 0x1c:
    case 0x5c:
    case 0xe0:
    case 0x2c:
    case 0xdc:
        if (run_format_3_4() == -1) {
            printf("Error: Error while running instruction %02X\n", opcode);
            return -1;
        }
        break;

    default:
        printf("Error: unsupported instruction %02X\n", opcode);
        return -1;
    }
    return 0;
}

void run(const char* cmd)
{
    char ch;
    if (sscanf(cmd, " %c", &ch) == 1) {
        printf("Invalid command.\n");
        return;
    }

    read_count = 0;

    for (;;) {
        if (run_instr() == -1) {
            return;
        }
        for (int i = 0; i < nbreakpoints; ++i) {
            if (breakpoints[i] == (int)reg.PC) {
                print_registers();
                printf("Stop at checkpoint [%04X]\n", (int)reg.PC);
                return;
            }
        }
    }
}
