#ifndef OPCODE_H
#define OPCODE_H

void initialize_hash_table(void);

void opcodelist(const char* cmd);

void opcode(const char* cmd);

int find_opcode(const char* mnemonic);

enum op_format {
    FORMAT_1,
    FORMAT_2,
    FORMAT_3_4,
    FORMAT_NOT_FOUND = -1
};
enum op_format find_op_format(const char* mnemonic);

void free_opcode_table(void);

#endif
