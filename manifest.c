#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "crc32.h"
#include "db.h"
#include "defines.h"
#include "manifest.h"

#define PARSE_DONE 0
#define PARSE_ERROR 1
#define PARSE_GOOD 2

// crc + space + path + comment + \x00
#define MAX_LINE_LENGTH 8 + 1 + _MAX_PATH + 80 + 1

int parse_line(FILE* fp, char** line, unsigned char* type, unsigned __int32* crc, char** path) {
    char* comment;
    char* cursor;
    char* endptr;
    unsigned char length;

    *line = fgets(*line, MAX_LINE_LENGTH, fp);
    if (*line == NULL) {
        return PARSE_DONE;
    }

    // Ignore comments
    comment = strchr(*line, ';');
    if (comment != NULL) {
        *comment = 0;
    }
    cursor = *line;
    length = strlen(cursor);
    if (length < 10) {
        return PARSE_ERROR;
    }
    if (cursor[8] != ' ') {
        return PARSE_ERROR;
    }
    *(cursor + 8) = '\0';
    if (strcmp(cursor, "_____DIR") == 0) {
        *type = TYPE_DIR;
        *crc = 0;
    } else if (strcmp(cursor, "____SKIP") == 0) {
        *type = TYPE_SKIP;
        *crc = 0;
    } else {
        *type = TYPE_FILE;
        *crc = strtoul(cursor, &endptr, 16);
        if (*endptr != 0) {
            return PARSE_ERROR;
        }
    }
    cursor += 9;
    // Remove trailing whitespace
    length = strlen(cursor);
    while (length > 0 && (cursor[length - 1] == ' ' || cursor[length - 1] == '\n')) {
        cursor[length - 1] = 0;
        length -= 1;
    }
    if (length == 0) {
        return PARSE_ERROR;
    }
    *path = cursor;
    return PARSE_GOOD;
}

void add_to_manifest(FILE* new_manifest, struct _find_t* file_info, char* full_path, char* rel_path) {
    unsigned __int32 crc;

    if (file_info->attrib & _A_SUBDIR) {
        fputs("_____DIR ", new_manifest);
    } else {
        crc32file(full_path, &crc);
        fput_crc(crc, new_manifest);
        fputc(' ', new_manifest);
    }
    fputs(rel_path, new_manifest);
    fputc('\n', new_manifest);
}

FILE* create_manifest(char* manifest_path) {
    FILE* fp;
    fp = fopen(manifest_path, "w");
    if (fp == NULL) {
        fputs("ERROR: Could not create manifest file.\n", stderr);
    }
    return fp;
}

unsigned char read_manifest(db_t* db, char* manifest_path) {
    FILE* fp;
    char* line;
    unsigned char type;
    unsigned __int32 crc;
    char* path;
    int temp;
    unsigned int parent_dir;
    char name[13];

    fp = fopen(manifest_path, "r");
    if (fp == NULL) {
        return FAIL0;
    }
    line = malloc(MAX_LINE_LENGTH);
    temp = parse_line(fp, &line, &type, &crc, &path);
    for (; temp != PARSE_DONE; temp = parse_line(fp, &line, &type, &crc, &path)) {
        if (temp == PARSE_ERROR) {
            continue;
        }
        //printf("CHECK %d %lu %s\n", type, crc, path);
        if (find_leaf_parent(db, path, &parent_dir, name)) {
            put_entry(db, parent_dir, name, type, crc);
        }
        //printf("LEAF %u %s\n", parent_dir, name);
    }
    free(line);
    fclose(fp);
    return SUCCESS1;
}
