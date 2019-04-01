#include "history.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct node {
    char* command;
    struct node* next;
} head = { NULL, NULL };

static struct node* last = &head;

void history(const char* cmd)
{
    char ch;
    if (sscanf(cmd, " %c", &ch) == 1) {
        puts("Invalid command.");
        return;
    }

    int i = 0;
    for (struct node* p = head.next; p; p = p->next) {
        printf("%d\t%s", ++i, p->command);
    }
}

void add_history(char const* command)
{
    last->next = malloc(sizeof(struct node));
    last = last->next;
    last->command = malloc(strlen(command) + 1);
    strcpy(last->command, command);
    last->next = NULL;
}

void free_history(void)
{
    for (struct node* p = head.next; p;) {
        struct node* next = p->next;
        free(p->command);
        free(p);
        p = next;
    }
}
