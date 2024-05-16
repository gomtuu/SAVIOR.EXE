#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "crc32.h"
#include "db.h"
#include "defines.h"

void init_db(db_t* db, unsigned int max_size) {
    unsigned char* byte;
    unsigned int length;

    length = sizeof(hash_entry_t) * max_size;
    db->data = malloc(length);
    if (!db->data) {
        fputs("Could not allocate database.", stdout);
        exit(1);
    }
    byte = (unsigned char*)db->data;
    while (length > 0) {
        *byte = 0;
        byte += 1;
        length -= 1;
    }
    db->size = 0;
    db->max_size = max_size;
}

unsigned int file_index(unsigned int parent_dir, char name[13]) {
    char key_string[2 + 13];
    unsigned char fill_zeroes;
    unsigned char i;
    unsigned __int32 key_crc;

    *(unsigned int*)key_string = parent_dir;
    fill_zeroes = 0;
    for (i = 0; i < 13; i += 1) {
        key_string[i + 2] = (fill_zeroes ? 0 : name[i]);
        if (name[i] == 0) {
            fill_zeroes = 1;
        }
    }
    key_crc = crc32buffer(key_string, 2 + 13, 0);
    return key_crc % HASH_ENTRIES;
}

int find_leaf_parent(db_t* db, char* path, unsigned int* parent_dir, char name[13]) {
    unsigned char is_leaf;
    char* cursor1;
    char* cursor2;
    hash_entry_t* entry;

    *parent_dir = 0xffff;
    is_leaf = 0;
    cursor1 = path;
    while (!is_leaf) {
        cursor2 = cursor1;
        while (*cursor2 != '\\' && *cursor2 != '\0') {
            cursor2 += 1;
        }
        if (*cursor2 == '\0') {
            is_leaf = 1;
        }
        *cursor2 = '\0';
        strcpy(name, cursor1);
        if (!is_leaf) {
            *cursor2 = '\\';
            *parent_dir = get_entry(db, *parent_dir, name, &entry);
            if (*parent_dir == HASH_NOTFOUND) {
                return 0;
            }
            if (entry == NULL || entry->type != TYPE_DIR) {
                return 0;
            }
            cursor1 = cursor2 + 1;
        }
    }
    return 1;
}

unsigned int put_entry(db_t* db, unsigned int parent_dir, char name[13], unsigned char type, unsigned __int32 crc) {
    unsigned int n;
    hash_entry_t* current;
    unsigned char same_file;
    unsigned char i;

    if (db->size >= db->max_size) {
        fputs("ERROR: Failed to add entry to file database.", stderr);
        free(db->data);
        exit(1);
    }
    n = file_index(parent_dir, name);
    //printf("put_entry file_index %p\n", n);
    current = db->data + n;
    // We want to be able to overwrite an entry with a later one,
    // so we don't treat identical files as collisions.
    same_file = current->parent_dir == parent_dir && strcmp(current->name, name) == 0;
    while ((*current->name > 0 && !same_file) || n == HASH_NOTFOUND) {
#if DEBUG
        fputs("Warning: hash collision\n", stderr);
#endif
        n += 1;
        while (n >= HASH_ENTRIES) {
            n -= HASH_ENTRIES;
        }
        current = db->data + n;
        same_file = current->parent_dir == parent_dir && strcmp(current->name, name) == 0;
    }
    //printf("Saving at %p\n", current);
    current->parent_dir = parent_dir;
    strcpy(current->name, name);
    current->type = type;
    current->crc = crc;
    db->size += 1;
    return n;
}

unsigned int get_entry(db_t* db, unsigned int parent_dir, char name[13], hash_entry_t** entry) {
    unsigned int n;
    hash_entry_t* current;
    unsigned int entries_searched;
    unsigned char found;
    unsigned char i;

    n = file_index(parent_dir, name);
    //printf("get_entry file_index %p\n", n);
    entries_searched = 0;
    found = 0;
    do {
        if (n == HASH_NOTFOUND) {
            n += 1;
        }
        current = db->data + n;
        if (current->type == TYPE_UNSET) {
            entries_searched += 1;
            break;
        }
        //printf("Looking at %p\n", current);
        if (current->parent_dir == parent_dir) {
            i = 0;
            while (!found && current->name[i] == name[i]) {
                //printf("Comparing %c to %c\n", current->name[i], name[i]);
                if (i >= (13 - 1) || name[i] == 0) {
                    found = 1;
                }
                i += 1;
            }
        }
        entries_searched += 1;
        if (!found) {
            n += 1;
            while (n >= HASH_ENTRIES) {
                n -= HASH_ENTRIES;
            }
        }
    } while (!found && entries_searched < HASH_ENTRIES);
    //printf("Entries searched: %u\n", entries_searched);
    if (found) {
        *entry = current;
        return n;
    }
    *entry = NULL;
    return HASH_NOTFOUND;
}
