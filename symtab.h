#ifndef SYMTAB_H
#define SYMTAB_H

struct symtab_impl;
typedef struct symtab_impl* symtab;

symtab symtab_init(void);
void symtab_insert(symtab tab, const char* label, int address);
int symtab_find(const symtab tab, const char* label);
void symtab_free(symtab tab);
void print_symtab_list_sorted(symtab tab);

#endif // SYMTAB_H
