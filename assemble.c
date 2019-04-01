#include "assemble.h"
#include "opcode.h"
#include "symtab.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define TEXT_RECORD_SIZE 0x1F

struct parse_result {
    enum {
        PARSE_RESULT_COMMENT,
        PARSE_RESULT_VALID,
        PARSE_RESULT_ERROR = -1
    } result;
    char label[10];
    char op_prefix;
    char opcode[10];
    char operands[100];
};

static struct parse_result parse_line(const char* line)
{
    struct parse_result res;
    int n;

    if (line[0] == 0) {
        res.result = PARSE_RESULT_ERROR;
        return res;
    }
    if (line[0] == '.') {
        res.result = PARSE_RESULT_COMMENT;
        return res;
    }

    if (line[0] == ' ') {
        // no label
        res.label[0] = 0;
    } else {
        sscanf(line, "%9s%n", res.label, &n);
        line += n;
    }

    // scan opcode
    res.op_prefix = 0;

    char prefix;
    if (sscanf(line, " %c%n", &prefix, &n) == 1) {
        if (prefix == '+') {
            res.op_prefix = prefix;
            line += n;
        }
    }

    sscanf(line, "%9s%n", res.opcode, &n);
    line += n;

    // scan operand 0
    res.operands[0] = 0;
    sscanf(line, " %99[^\n]", res.operands);

    res.result = PARSE_RESULT_VALID;

    return res;
}

struct im_parse_result {
    int address;
    int pc;
    struct parse_result p;
};

struct im_parse_result parse_im_line(const char* line)
{
    struct im_parse_result res;
    int n;
    if (sscanf(line, "%d|%d|%n", &res.address, &res.pc, &n) != 2) {
        res.address = -1;
        return res;
    }
    res.p = parse_line(line + n);
    return res;
}

static int parse_int(const char* str, int* result)
{
    if (sscanf(str, "%d", result) != 1) {
        return -1;
    }
    return 0;
}

static void switch_extension(const char* orig, const char* ext, char* result)
{
    int i = (int)strlen(orig) - 1;
    strcpy(result, orig);
    while (i >= 0 && result[i] != '.') {
        i--;
    }
    if (i > 0) {
        result[i] = 0;
    }
    strcat(result, ext);
}

static int parse_byte_string(const char* str, char* result, int max_size)
{
    int i = 0;
    while (str[i] && isspace(str[i])) {
        ++i;
    }

    if (str[i] == 'X' && str[i + 1] == '\'') {
        // parse hex string

        i += 2;
        int j;
        for (j = 0; j / 2 < max_size && isxdigit(str[i]) && (isdigit(str[i]) || isupper(str[i])); ++i, ++j) {
            int hex = isdigit(str[i]) ? str[i] - '0' : str[i] - 'A' + 10;
            if (j % 2 == 0) {
                result[j / 2] = (hex << 4) & 0xff;
            } else {
                result[j / 2] |= hex & 0xff;
            }
        }

        if (j % 2 == 1) {
            return -1;
        }

        if (str[i] != '\'') {
            return -1;
        }

        return j / 2;

    } else if (str[i] == 'C' && str[i + 1] == '\'') {
        // parse char string

        i += 2;
        int j = 0;
        while (j < max_size && str[i] && str[i] != '\'') {
            result[j++] = str[i++];
        }

        if (str[i] != '\'') {
            return -1;
        }

        return j;
    }

    return -1;
}

static int calc_ins_length(int lineno, struct parse_result* parse)
{
    if (strcmp(parse->opcode, "WORD") == 0) {
        int word;
        if (parse_int(parse->operands, &word)) {
            printf("%d: Error: cannot parse number\n", lineno);
            return -1;
        }
        if (word > 0xffffff || word < -0x800000) {
            printf("%d: Error: integer out of range\n", lineno);
            return -1;
        }
        return 3;
    } else if (strcmp(parse->opcode, "RESW") == 0) {
        int num_words;
        if (parse_int(parse->operands, &num_words)) {
            printf("%d: Error: cannot parse number\n", lineno);
            return -1;
        }
        return 3 * num_words;
    } else if (strcmp(parse->opcode, "RESB") == 0) {
        int num_bytes;
        if (parse_int(parse->operands, &num_bytes)) {
            printf("%d: Error: cannot parse number\n", lineno);
            return -1;
        }
        return num_bytes;
    } else if (strcmp(parse->opcode, "BYTE") == 0) {
        char str[4096];
        int len = parse_byte_string(parse->operands, str, sizeof(str));
        if (len < 0) {
            printf("%d: Error: cannot parse byte string\n", lineno);
            return -1;
        }
        return len;
    } else {
        int opcode = find_opcode(parse->opcode);
        if (opcode == -1) {
            printf("%d: Error: invalid opcode '%s'\n", lineno, parse->opcode);
            return -1;
        }

        switch (find_op_format(parse->opcode)) {
        case FORMAT_1:
            return 1;
        case FORMAT_2:
            return 2;
        case FORMAT_3_4:
            if (parse->op_prefix == '+') {
                return 4;
            } else {
                return 3;
            }
        default:
            printf("%d: Error: invalid opcode '%s'\n", lineno, parse->opcode);
            return -1;
        }
    }
}

static int first_pass(const char* file, FILE* tmp, symtab symbols)
{
    FILE* fp = fopen(file, "r");
    if (!fp) {
        printf("Cannot open file %s.\n", file);
        return -1;
    }

    int starting_address = -1;
    int loc_ctr = 0;

    char line[4096];
    int first_real_line = 1;
    for (int lineno = 1; fgets(line, sizeof(line), fp); lineno++) {
        if (!strchr(line, '\n')) {
            printf("%d: Error: line too long\n", lineno);
            goto error;
        }

        struct parse_result parse = parse_line(line);

        if (parse.result == PARSE_RESULT_ERROR) {
            printf("%d: Error: parse error\n", lineno);
            goto error;
        }

        if (parse.result == PARSE_RESULT_COMMENT) {
            fprintf(tmp, "%s", line);
            continue;
        }

        if (first_real_line) {
            first_real_line = 0;
            if (strcmp(parse.opcode, "START") != 0) {
                printf("%d: Error: file does not begin with START directive.\n", lineno);
                goto error;
            }

            if (parse_int(parse.operands, &starting_address)) {
                printf("%d: Error: cannot parse number\n", lineno);
                goto error;
            }
            loc_ctr = starting_address;

            fprintf(tmp, "%d|%d|%s", loc_ctr, loc_ctr, line);
            continue;
        } else if (strcmp(parse.opcode, "END") == 0) {
            fprintf(tmp, "%d|%d|%s", loc_ctr, loc_ctr, line);
            break;
        } else if (strcmp(parse.opcode, "BASE") == 0) {
            fprintf(tmp, "%d|%d|%s", loc_ctr, loc_ctr, line);
            continue;
        }

        // if label exists
        if (parse.label[0]) {
            if (symtab_find(symbols, parse.label) != -1) {
                printf("%d: Error: duplicate symbol '%s'\n", lineno, parse.label);
                goto error;
            }
            symtab_insert(symbols, parse.label, loc_ctr);
        }

        // calculate instruction/directive length
        int len = calc_ins_length(lineno, &parse);
        if (len < 0) {
            goto error;
        }

        loc_ctr += len;

        fprintf(tmp, "%d|%d|%s", loc_ctr - len, loc_ctr, line);
    }

    fclose(fp);

    rewind(tmp);

    return loc_ctr - starting_address;

error:
    fclose(fp);

    return -1;
}

static void write_listing(FILE* lst, int lineno, struct im_parse_result* parse, unsigned char* code, int len)
{
    fprintf(lst, "%4d   ", lineno * 5);
    fprintf(lst, "%04X   ", parse->address);
    fprintf(lst, "%-10s", parse->p.label);
    if (parse->p.op_prefix) {
        fprintf(lst, "%c", parse->p.op_prefix);
        fprintf(lst, "%-9s", parse->p.opcode);
    } else {
        fprintf(lst, "%-10s", parse->p.opcode);
    }
    fprintf(lst, "%-20s", parse->p.operands);

    for (int i = 0; i < len; ++i) {
        fprintf(lst, "%02X", code[i]);
    }

    fprintf(lst, "\n");
}

struct text_record {
    int start_address;
    int len;
    unsigned char text[TEXT_RECORD_SIZE];
};

static void flush_text_record(FILE* obj, struct text_record* rec, int start_address)
{
    // no need to write 0-byte text records
    if (rec->len == 0) {
        rec->start_address = start_address;
        rec->len = 0;
        return;
    }

    fprintf(obj, "T%06X%02X", rec->start_address, rec->len);
    for (int i = 0; i < rec->len; ++i) {
        fprintf(obj, "%02X", rec->text[i]);
    }
    fprintf(obj, "\n");

    rec->start_address = start_address;
    rec->len = 0;
}

static int get_reg_num(const char* r)
{
    if (strcmp(r, "A") == 0) {
        return 0;
    } else if (strcmp(r, "X") == 0) {
        return 1;
    } else if (strcmp(r, "L") == 0) {
        return 2;
    } else if (strcmp(r, "PC") == 0) {
        return 8;
    } else if (strcmp(r, "SW") == 0) {
        return 9;
    } else if (strcmp(r, "B") == 0) {
        return 3;
    } else if (strcmp(r, "S") == 0) {
        return 4;
    } else if (strcmp(r, "T") == 0) {
        return 5;
    } else if (strcmp(r, "F") == 0) {
        return 6;
    }
    return -1;
}

struct ins_context {
    int lineno;
    int base_addr;
    symtab symbols;
    int pc;
    int opcode;
    char op_prefix;
    enum op_format fmt;
    const char* operands;
};

static int assemble_ins(struct ins_context* ctx, unsigned char* output)
{
    if (ctx->fmt == FORMAT_1) {
        output[0] = ctx->opcode & 0xff;

        return 1;

    } else if (ctx->fmt == FORMAT_2) {
        char r1[100], r2[100];
        int cnt;

        output[0] = ctx->opcode & 0xff;

        if ((cnt = sscanf(ctx->operands, "%99[^, ] , %99s", r1, r2)) >= 1) {
            if (cnt == 1) {
                int nr1 = get_reg_num(r1);

                if (nr1 == -1) {
                    printf("%d: Error: no such register\n", ctx->lineno);
                    return -1;
                }

                output[1] = (nr1 << 4) & 0xff;
            } else if (cnt == 2) {
                int nr1 = get_reg_num(r1);
                int nr2 = get_reg_num(r2);

                if (nr1 == -1 || nr2 == -1) {
                    printf("%d: Error: no such register\n", ctx->lineno);
                    return -1;
                }

                output[1] = (nr1 << 4 | nr2) & 0xff;
            }
        } else {
            printf("%d: Error: incorrect format 2 operands\n", ctx->lineno);
            return -1;
        }

        return 2;

    } else if (ctx->fmt == FORMAT_3_4) {
        // Format 3/4
        char m[100], ch;
        int cnt;
        int is_simple = 0, is_immediate = 0, is_indirect = 0;
        int is_extended = ctx->op_prefix == '+';

        output[0] = ctx->opcode & 0xfc;

        if ((cnt = sscanf(ctx->operands, "%99[^#@, ] , %c", m, &ch)) >= 1) {
            // simple addressing
            is_simple = 1;
        } else if ((cnt = sscanf(ctx->operands, "#%99[^#@, ] , %c", m, &ch)) >= 1) {
            // immediate addressing
            is_immediate = 1;
        } else if ((cnt = sscanf(ctx->operands, "@%99[^#@, ] , %c", m, &ch)) >= 1) {
            // indirect addressing
            is_indirect = 1;
        } else if (sscanf(ctx->operands, " %c", &ch) != 1) {
            output[0] |= 0x03;
            output[1] = 0;
            output[2] = 0;
            return 3;
        } else {
            printf("%d: Error: unrecognized format 3 operands\n", ctx->lineno);
            return -1;
        }

        // simple addressing
        int addr = symtab_find(ctx->symbols, m);

        int is_absolute = is_extended;
        if (addr == -1) {
            if (is_immediate) {
                if (parse_int(m, &addr)) {
                    printf("%d: Error: symbol not found or cannot parse int '%s'\n", ctx->lineno, m);
                    return -1;
                }
                is_absolute = 1;
            } else {
                printf("%d: Error: symbol '%s' not found\n", ctx->lineno, m);
                return -1;
            }
        }

        if (is_simple) {
            // n i = 1 1
            output[0] |= 0x03;
        } else if (is_immediate) {
            // n i = 0 1
            output[0] |= 0x01;
        } else if (is_indirect) {
            // n i = 1 0
            output[0] |= 0x02;
        }

        // calculate relative address
        int rel;

        output[1] = 0x00;

        if (is_absolute) {
            rel = addr;
        } else {
            int pc_rel = addr - ctx->pc;
            int base_rel = addr - ctx->base_addr;

            if (pc_rel >= -0x800 && pc_rel <= 0x7ff) {
                rel = pc_rel;
                output[1] |= 0x20;
            } else if (base_rel >= 0 && base_rel <= 0xfff) {
                rel = base_rel;
                output[1] |= 0x40;
            } else {
                printf("%d: Error: cannot be addressed in format 3\n", ctx->lineno);
                return -1;
            }
        }

        // format 4
        if (is_extended) {
            output[1] |= 0x10;
        }

        // indexed addressing
        if (cnt == 2 && ch == 'X') {
            output[1] |= 0x80;
        }

        if (is_extended) {
            // x b p e disp[0..3]
            output[1] |= (addr >> 16) & 0x0f;

            // disp
            output[2] = (addr >> 8) & 0xff;
            output[3] = addr & 0xff;

            return 4;
        } else {
            // x b p e disp[0..3]
            output[1] |= (rel >> 8) & 0x0f;

            // disp
            output[2] = rel & 0xff;

            return 3;
        }
    }

    printf("%d: Error: unrecognized instruction format\n", ctx->lineno);
    return -1;
}

static int second_pass(const char* file, int program_length, FILE* tmp, symtab symbols)
{
    char lstfile[104];
    switch_extension(file, ".lst", lstfile);
    FILE* lst = fopen(lstfile, "w");
    if (!lst) {
        printf("Cannot open %s for writing.\n", lstfile);
        return -1;
    }

    char objfile[104];
    switch_extension(file, ".obj", objfile);
    FILE* obj = fopen(objfile, "w");
    if (!obj) {
        printf("Cannot open %s for writing.\n", objfile);
        fclose(lst);
        return -1;
    }

    int starting_address = 0;
    int base_addr = -1;

    struct text_record rec = { 0, 0, "" };

    char line[4096];
    int first_real_line = 1;
    for (int lineno = 1; fgets(line, sizeof(line), tmp); lineno++) {
        if (!strchr(line, '\n')) {
            printf("%d: Error: line too long\n", lineno);
            goto error;
        }

        struct im_parse_result parse = parse_im_line(line);

        // comment
        if (parse.address == -1) {
            fprintf(lst, "%4d%10s%s", lineno * 5, "", line);
            continue;
        }

        if (first_real_line) {
            first_real_line = 0;
            if (strcmp(parse.p.opcode, "START") != 0) {
                printf("%d: Error: file does not begin with START directive.\n", lineno);
                goto error;
            }

            write_listing(lst, lineno, &parse, NULL, 0);

            if (parse_int(parse.p.operands, &starting_address)) {
                printf("%d: Error: cannot parse number\n", lineno);
                goto error;
            }

            fprintf(obj, "H%6s%06X%06X\n", parse.p.label, starting_address, program_length);

            flush_text_record(obj, &rec, starting_address);
            continue;

        } else if (strcmp(parse.p.opcode, "BASE") == 0) {
            char sym[100];
            if (sscanf(parse.p.operands, "%99s", sym) != 1) {
                printf("%d: Error: invalid BASE operand\n", lineno);
                goto error;
            }

            int addr = symtab_find(symbols, sym);
            if (addr == -1) {
                printf("%d: Error: no such symbol '%s'", lineno, sym);
                goto error;
            }

            base_addr = addr;

            fprintf(lst, "%4d%20s%-10s%-20s\n", lineno * 5, "", parse.p.opcode, parse.p.operands);

            continue;
        } else if (strcmp(parse.p.opcode, "END") == 0) {
            flush_text_record(obj, &rec, 0);
            fprintf(obj, "E%06X\n", starting_address);

            // TODO: find first executable line
            fprintf(lst, "%4d%20s%-10s%-20s\n", lineno * 5, "", parse.p.opcode, parse.p.operands);

            break;
        }

        if (strcmp(parse.p.opcode, "BYTE") == 0) {
            char str[4096];

            int len = parse_byte_string(parse.p.operands, str, sizeof(str));
            if (len < 0) {
                printf("%d: Error: cannot parse byte string\n", lineno);
                goto error;
            }

            // write to text record
            if (rec.len + len >= TEXT_RECORD_SIZE) {
                flush_text_record(obj, &rec, parse.address);
            }

            if (len > TEXT_RECORD_SIZE) {
                printf("%d: Error: byte string exceeds text record size\n", lineno);
                goto error;
            }

            for (int i = 0; i < len; ++i) {
                rec.text[rec.len++] = str[i];
            }

            write_listing(lst, lineno, &parse, rec.text + rec.len - len, len);

        } else if (strcmp(parse.p.opcode, "WORD") == 0) {
            int word;
            if (parse_int(parse.p.operands, &word)) {
                printf("%d: Error: cannot parse number\n", lineno);
                return -1;
            }
            if (word > 0xffffff || word < -0x800000) {
                printf("%d: Error: integer out of range\n", lineno);
                return -1;
            }

            // write to text record
            if (rec.len + 3 >= TEXT_RECORD_SIZE) {
                flush_text_record(obj, &rec, parse.address);
            }

            rec.text[rec.len++] = (word >> 16) & 0xff;
            rec.text[rec.len++] = (word >> 8) & 0xff;
            rec.text[rec.len++] = word & 0xff;

            write_listing(lst, lineno, &parse, rec.text + rec.len - 3, 3);

        } else if (strcmp(parse.p.opcode, "RESW") == 0) {

            // flush text record
            flush_text_record(obj, &rec, parse.pc);

            write_listing(lst, lineno, &parse, NULL, 0);

        } else if (strcmp(parse.p.opcode, "RESB") == 0) {

            flush_text_record(obj, &rec, parse.pc);

            write_listing(lst, lineno, &parse, NULL, 0);

        } else {
            struct ins_context ctx;
            ctx.lineno = lineno;
            ctx.base_addr = base_addr;
            ctx.symbols = symbols;
            ctx.pc = parse.pc;
            ctx.opcode = find_opcode(parse.p.opcode);

            if (ctx.opcode == -1) {
                printf("%d: Error: invalid opcode '%s'\n", lineno, parse.p.opcode);
                goto error;
            }

            ctx.op_prefix = parse.p.op_prefix;
            ctx.fmt = find_op_format(parse.p.opcode);
            ctx.operands = parse.p.operands;

            unsigned char instruction[4];
            int len = assemble_ins(&ctx, instruction);

            if (len == -1) {
                goto error;
            }

            if (rec.len + len >= TEXT_RECORD_SIZE) {
                flush_text_record(obj, &rec, parse.address);
            }

            for (int i = 0; i < len; ++i) {
                rec.text[rec.len++] = instruction[i];
            }

            write_listing(lst, lineno, &parse, instruction, len);
        }
    }

    fclose(lst);
    fclose(obj);

    return 0;

error:
    fclose(lst);
    fclose(obj);

    return -1;
}

static symtab symbols = NULL;

void assemble(const char* cmd)
{
    char ch, file[100];
    if (sscanf(cmd, "%99s %c", file, &ch) != 1) {
        printf("Invalid command.\n");
        return;
    }

    if (symbols) {
        symtab_free(symbols);
    }
    symbols = symtab_init();

    FILE* tmp = tmpfile();
    if (!tmp) {
        printf("Cannot open temporary file.\n");
        symtab_free(symbols);
        return;
    }

    int program_length = first_pass(file, tmp, symbols);
    if (program_length == -1) {
        goto cleanup;
    }

    if (second_pass(file, program_length, tmp, symbols) == -1) {
        goto cleanup;
    }

cleanup:
    fclose(tmp);
}

void symbol(const char* cmd)
{
    char ch;
    if (sscanf(cmd, " %c", &ch) == 1) {
        printf("Invalid command.\n");
        return;
    }

    if (!symbols) {
        printf("No symbols.\n");
        return;
    }

    print_symtab_list_sorted(symbols);
}

void free_symbols(void)
{
    if (symbols) {
        symtab_free(symbols);
    }
}
