#ifndef DB_H
#define DB_H

#define TYPE_UNSET 0
#define TYPE_DIR 1
#define TYPE_SKIP 2
#define TYPE_FILE 4

// Size of hash table.
#define HASH_ENTRIES 3200

#define HASH_NOTFOUND 0

typedef struct {
    unsigned int parent_dir;
    char name[13];
    unsigned char type;
    unsigned __int32 crc;
} hash_entry_t;

typedef struct {
    hash_entry_t* data;
    unsigned int size;
    unsigned int max_size;
} db_t;

void init_db(db_t* db, unsigned int max_size);
unsigned int file_index(unsigned int parent_dir, char name[13]);
int find_leaf_parent(db_t* db, char* path, unsigned int* parent_dir, char name[13]);
unsigned int put_entry(db_t* db, unsigned int parent_dir, char name[13], unsigned char type, unsigned __int32 crc);
unsigned int get_entry(db_t* db, unsigned int parent_dir, char name[13], hash_entry_t** entry);

#endif /* DB_H */
