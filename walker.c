#include <dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "defines.h"
#include <direct.h>
#include "filesys.h"
#include "walker.h"

void init_walker(walker_t* walker, char* path) {
    uint8_t i;

    strcpy(walker->full_path, path);
    remove_trailing_backslashes(walker->full_path, 2);
    walker->path_tail = walker->full_path + strlen(walker->full_path);
    walker->rel_path = walker->path_tail + 1;
    for (i = 0; i < 100; i += 1) {
        walker->data[i] = NULL;
    }
    walker->current = NULL;
    walker->depth = 0;
    walker->destructive = 0;
}

struct find_t* expand_walker(walker_t* walker) {
    struct find_t* new_item;

    if (walker->depth >= 100) {
        return NULL;
    }
    new_item = malloc(sizeof(struct find_t));
    if (new_item) {
        walker->data[walker->depth] = new_item;
        walker->current = new_item;
        walker->depth += 1;
    }
    return new_item;
}

void shrink_walker(walker_t* walker) {
    _dos_findclose(walker->current);
    free(walker->current);
    walker->depth -= 1;
    if (walker->depth > 0) {
        walker->current = walker->data[walker->depth - 1];
    } else {
        walker->current = NULL;
    }
}

void free_walker(walker_t* walker) {
    while (walker->depth > 0) {
        shrink_walker(walker);
    }
}

uint8_t walker_next(walker_t* walker) {
    struct find_t* file_info;
    unsigned int find_result;

    file_info = walker->current;
    if (walker->depth == 0 || (file_info->attrib & _A_SUBDIR)) {
        file_info = expand_walker(walker);
        if (file_info != NULL) {
            if (strlen(walker->full_path) <= _MAX_PATH - 4) {
                strcpy(walker->path_tail, "\\*.*");
                find_result = _dos_findfirst(walker->full_path, _A_NORMAL | _A_HIDDEN | _A_SYSTEM | _A_SUBDIR, file_info);
                walker->path_tail[0] = '\0';
            } else {
                fputs("ERROR: Path too long: ", stderr);
                fputs(walker->full_path, stderr);
                fputs("\n", stderr);
                find_result = FAIL1;
            }
        } else {
            fputs("ERROR: Couldn't recurse into directory ", stderr);
            fputs(walker->full_path, stderr);
            fputs("\n", stderr);
            find_result = FAIL1;
        }
        if (find_result == SUCCESS0 && strcmp(file_info->name, ".") == 0) {
            find_result = _dos_findnext(file_info);
        }
        if (find_result == SUCCESS0 && strcmp(file_info->name, "..") == 0) {
            find_result = _dos_findnext(file_info);
        }
    } else {
        find_result = _dos_findnext(file_info);
    }
    while (find_result != SUCCESS0 && walker->depth > 1) {
        shrink_walker(walker);
        file_info = walker->current;
        if (walker->destructive) {
            walker->path_tail[0] = '\0';
            _rmdir(walker->full_path);
        }
        walker->path_tail -= strlen(file_info->name);
        walker->path_tail -= 1;
        walker->path_tail[0] = '\0';
        find_result = _dos_findnext(file_info);
    }
    if (find_result == SUCCESS0) {
        walker->path_tail[0] = '\\';
        strcpy(walker->path_tail + 1, file_info->name);
        if (file_info->attrib & _A_SUBDIR) {
            walker->path_tail += 1;
            walker->path_tail += strlen(file_info->name);
        }
        return SUCCESS1;
    }
    return FAIL0;
}
