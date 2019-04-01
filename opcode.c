#include "opcode.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HASH_TABLE_SIZE 20

struct node {
    char mnemonic[10];
    int opcode;
    enum op_format format;
    struct node* next;
};

static struct head {
    struct node* first;
    struct node* last;
} hash_table[HASH_TABLE_SIZE];

static int hash(const char* string)
{
    unsigned long long hash = 0x9522ff583c788efe;
    for (int i = 0; string[i]; ++i) {
        hash += string[i];
        hash ^= hash << 13;
        hash ^= hash >> 7;
        hash ^= hash << 17;
    }
    return hash % HASH_TABLE_SIZE;
}

static void add_to_hash_table(const char* mnemonic, int opcode, enum op_format format)
{
    struct head* h = &hash_table[hash(mnemonic)];
    for (struct node* p = h->first; p; p = p->next) {
        if (strcmp(p->mnemonic, mnemonic) == 0) {
            // string already in table
            return;
        }
    }

    struct node* node = malloc(sizeof(struct node));
    strcpy(node->mnemonic, mnemonic);
    node->opcode = opcode;
    node->format = format;
    node->next = NULL;

    if (!h->first) {
        h->last = h->first = node;
    } else {
        h->last->next = node;
        h->last = node;
    }
}

void initialize_hash_table(void)
{
    FILE* fp = fopen("opcode.txt", "r");
    if (!fp) {
        printf("Error: Cannot open opcode.txt\n");
        exit(1);
    }

    int opcode;
    char mnemonic[10], format[10];
    while (fscanf(fp, "%x %9s %9s", &opcode, mnemonic, format) == 3) {
        enum op_format fmt;
        if (strcmp(format, "1") == 0) {
            fmt = FORMAT_1;
        } else if (strcmp(format, "2") == 0) {
            fmt = FORMAT_2;
        } else if (strcmp(format, "3/4") == 0) {
            fmt = FORMAT_3_4;
        } else {
            printf("Invalid format: %s\n", format);
            break;
        }

        add_to_hash_table(mnemonic, opcode, fmt);
    }
    fclose(fp);
}

int find_opcode(const char* mnemonic)
{
    struct head* h = &hash_table[hash(mnemonic)];
    for (struct node* p = h->first; p; p = p->next) {
        if (strcmp(p->mnemonic, mnemonic) == 0) {
            return p->opcode;
        }
    }

    return -1;
}

enum op_format find_op_format(const char* mnemonic)
{
    struct head* h = &hash_table[hash(mnemonic)];
    for (struct node* p = h->first; p; p = p->next) {
        if (strcmp(p->mnemonic, mnemonic) == 0) {
            return p->format;
        }
    }

    return FORMAT_NOT_FOUND;
}

void opcode(const char* cmd)
{
    char ch, mnemonic[10];
    if (sscanf(cmd, "%9s %c", mnemonic, &ch) != 1) {
        printf("Invalid command.\n");
        return;
    }

    int result = find_opcode(mnemonic);
    if (result < 0) {
        printf("No such instruction.\n");
        return;
    }
    printf("opcode is %02X\n", result);
}

void opcodelist(const char* cmd)
{
    char ch;
    if (sscanf(cmd, " %c", &ch) == 1) {
        printf("Invalid command.\n");
        return;
    }

    for (int i = 0; i < HASH_TABLE_SIZE; ++i) {
        printf("%d : ", i);

        int flag = 0;
        for (struct node* p = hash_table[i].first; p; p = p->next) {
            if (flag) {
                printf(" -> ");
            }
            flag = 1;
            printf("[%s, %02X]", p->mnemonic, p->opcode);
        }

        puts("");
    }
}

void free_opcode_table(void)
{
    for (int i = 0; i < HASH_TABLE_SIZE; ++i) {
        for (struct node* p = hash_table[i].first; p;) {
            struct node* next = p->next;
            free(p);
            p = next;
        }
    }
}

