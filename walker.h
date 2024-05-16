#ifndef WALKER_H
#define WALKER_H

#include <sys/types.h>

typedef struct {
    char full_path[_MAX_PATH];
    char* path_tail;
    char* rel_path;
    struct find_t* data[100];
    struct find_t* current;
    unsigned char depth;
    unsigned char destructive;
} walker_t;

void init_walker(walker_t* walker, char* path);
struct find_t* expand_walker(walker_t* walker);
void shrink_walker(walker_t* walker);
void free_walker(walker_t* walker);
uint8_t walker_next(walker_t* walker);

#endif /* WALKER_H */
