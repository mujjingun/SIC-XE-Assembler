#include "symtab.h"

#define SYMTAB_SIZE 20

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct node {
    char* label;
    int address;
    struct node* next;
};

struct symtab_impl {
    struct table {
        struct node* first;
        struct node* last;
    } table[SYMTAB_SIZE];
};

symtab symtab_init(void)
{
    symtab table = malloc(sizeof(struct symtab_impl));
    for (int i = 0; i < SYMTAB_SIZE; ++i) {
        table->table[i].first = table->table[i].last = NULL;
    }

    return table;
}

static int hash(const char* string)
{
    unsigned long long hash = 0x9522ff583c788efe;
    for (int i = 0; string[i]; ++i) {
        hash += string[i];
        hash ^= hash << 13;
        hash ^= hash >> 7;
        hash ^= hash << 17;
    }
    return hash % SYMTAB_SIZE;
}

void symtab_insert(symtab tab, const char* label, int address)
{
    struct table* h = &tab->table[hash(label)];
    for (struct node* p = h->first; p; p = p->next) {
        if (strcmp(p->label, label) == 0) {
            // string already in table
            // update address
            p->address = address;
            return;
        }
    }

    struct node* node = malloc(sizeof(struct node));
    node->label = malloc(strlen(label) + 1);
    strcpy(node->label, label);
    node->address = address;
    node->next = NULL;

    if (!h->first) {
        h->last = h->first = node;
    } else {
        h->last->next = node;
        h->last = node;
    }
}

int symtab_find(const symtab tab, const char* label)
{
    struct table* h = &tab->table[hash(label)];
    for (struct node* p = h->first; p; p = p->next) {
        if (strcmp(p->label, label) == 0) {
            return p->address;
        }
    }

    return -1;
}

void symtab_free(symtab tab)
{
    for (int i = 0; i < SYMTAB_SIZE; ++i) {
        for (struct node* p = tab->table[i].first; p;) {
            struct node* next = p->next;
            free(p->label);
            free(p);
            p = next;
        }
    }

    free(tab);
}

struct symbol_info {
    char* label;
    int address;
};

static struct symbol_info** symtab_list(symtab tab, int* n)
{
    int size = 0;
    for (int i = 0; i < SYMTAB_SIZE; ++i) {
        for (struct node* p = tab->table[i].first; p; p = p->next) {
            size++;
        }
    }

    struct symbol_info** list = malloc(sizeof(struct symbol_info*) * (size + 1));
    int j = 0;
    for (int i = 0; i < SYMTAB_SIZE; ++i) {
        for (struct node* p = tab->table[i].first; p; p = p->next) {
            list[j] = malloc(sizeof(struct symbol_info));
            list[j]->label = malloc(strlen(p->label) + 1);
            strcpy(list[j]->label, p->label);
            list[j]->address = p->address;
            j++;
        }
    }

    list[size] = NULL;

    *n = size;

    return list;
}

static void symtab_list_free(struct symbol_info** list)
{
    for (int i = 0; list[i]; ++i) {
        free(list[i]->label);
        free(list[i]);
    }
    free(list);
}

static int compare_symbol_infos(const void* a, const void* b)
{
    struct symbol_info* const* pa = a;
    struct symbol_info* const* pb = b;

    return strcmp((*pa)->label, (*pb)->label);
}

void print_symtab_list_sorted(symtab tab)
{
    int size;
    struct symbol_info** list = symtab_list(tab, &size);

    qsort(list, size, sizeof(struct symbol_info*), compare_symbol_infos);

    for (int i = 0; i < size; ++i) {
        printf("\t%s\t%04X\n", list[i]->label, list[i]->address);
    }

    symtab_list_free(list);
}
