#ifndef DUMP_H
#define DUMP_H

void dump(const char* cmd);
void edit(const char* cmd);
void fill(const char* cmd);
void reset(const char* cmd);

void progaddr(const char* cmd);
void loader(const char* cmd);
void run(const char* cmd);
void breakpoint(const char* cmd);
void free_breakpoints(void);

#endif
