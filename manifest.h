#include <dos.h>
#include "db.h"

#ifndef MANIFEST_H
#define MANIFEST_H

#define MANIFEST_TMP "MANIFEST.VI0"
#define MANIFEST_NAME "MANIFEST.VIO"
#define INJECTED_NAME "INJECTED.VIO"

FILE* create_manifest(char* manifest_path);
void add_to_manifest(FILE* new_manifest, struct _find_t* file_info, char* full_path, char* rel_path);
unsigned char read_manifest(db_t* db, char* manifest_path);

#endif /* MANIFEST_H */
