#include <conio.h>
#include <ctype.h>
#include <dos.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "crc32.h"
#include "db.h"
#include "defines.h"
#include "filesys.h"
#include "manifest.h"
#include "walker.h"

#define HANDLE_SIGNAL 0

#if HANDLE_SIGNAL
#include <signal.h>
void handle_sigint(int junk) {
    exit(1);
}
#endif

static struct {
    unsigned char skip_if_archive_clear;
} config;

static char* src_path;
static char* dest_path;
static db_t db;

void print_help() {
//         12345678901234567890123456789012345678901234567890123456789012345678901234567890
    fputs("Usage: SAVIOR.EXE <source_dir> [backup_dir] [options]\n", stdout);
    fputs("\n", stdout);
    fputs("  source_dir: The directory that contains the files you want to scan.\n", stdout);
    fputs("\t      Files will be compared to MANIFEST.VIO in the same directory.\n", stdout);
    fputs("\t      If no manifest exists, one will be created.\n", stdout);
    fputs("\t      Otherwise, new or modified files will be backed up.\n", stdout);
    fputs("\t      If no files are new or modified, a restore will be attempted.\n", stdout);
    fputs("\n", stdout);
    fputs("  backup_dir: The directory where new or modified files will be backed up.\n", stdout);
    fputs("\t      If omitted, no backup or restore will take place.\n", stdout);
    fputs("\t      Instead, a new manifest file will be created in source_dir,\n", stdout);
    fputs("\t      overwriting any existing manifest.\n", stdout);
    fputs("\n", stdout);
    fputs("Options:\n", stdout);
    fputs("  /a    If a file's archive bit is not set, assume it hasn't changed,\n", stdout);
    fputs("\tand don't back it up. This mode is faster because CRC-32 hashes\n", stdout);
    fputs("\twon't need to be computed for any such files, but it might be less\n", stdout);
    fputs("\treliable in weird situations.\n", stdout);
}

unsigned char check_path(char* path) {
    unsigned int dest_attrib;

    if (path == NULL) {
        return FAIL0;
    }
    if (_dos_getfileattr(path, &dest_attrib) != SUCCESS0) {
        return FAIL0;
    }
    if (!(dest_attrib & _A_SUBDIR)) {
        return FAIL0;
    }
    return SUCCESS1;
}

void create_dest_subdirs() {
    char tmp_path[_MAX_PATH];

    if (join_path(tmp_path, dest_path, "CRC")) {
        mkdir_p(tmp_path, 1);
    }
    if (join_path(tmp_path, dest_path, "BACKUP")) {
        mkdir_p(tmp_path, 1);
    }
    if (join_path(tmp_path, dest_path, "INJECT")) {
        mkdir_p(tmp_path, 1);
    }
}

unsigned char build_manifest(char* manifest_path) {
    unsigned char path_length;
    char tmp_path[_MAX_PATH];
    FILE* new_manifest;
    walker_t walker;

    // Change extension from VIO to VI0 for temporary file.
    strcpy(tmp_path, manifest_path);
    path_length = strlen(tmp_path);
    tmp_path[path_length - 1] = '0';
    // Create temporary manifest file.
    new_manifest = create_manifest(tmp_path);
    if (!new_manifest) {
        return FAIL0;
    }
    // Populate the manifest file.
    init_walker(&walker, src_path);
    while (walker_next(&walker)) {
        if (should_ignore_file(walker.current->name)) {
            continue;
        }
        add_to_manifest(new_manifest, walker.current, walker.full_path, walker.rel_path);
        _dos_setfileattr(walker.full_path, walker.current->attrib & ~_A_ARCH);
    }
    free_walker(&walker);
    fclose(new_manifest);
    // Change name to final manifest name.
    remove(manifest_path);
    if (rename(tmp_path, manifest_path) != SUCCESS0) {
        perror(manifest_path);
        return FAIL0;
    }
    return SUCCESS1;
}

unsigned char inject_crc(char* flag, unsigned __int32 crc, char* rel_path) {
    char injected_path[_MAX_PATH];
    FILE* fp;

    if (!join_path(injected_path, src_path, INJECTED_NAME)) {
        return FAIL0;
    }
    fp = fopen(injected_path, "a");
    if (fp == NULL) {
        perror(injected_path);
        return FAIL0;
    }
    if (*flag == '\0') {
        fput_crc(crc, fp);
    } else {
        fputs(flag, fp);
    }
    fputs(" ", fp);
    fputs(rel_path, fp);
    fputs("\n", fp);
    fclose(fp);
    return SUCCESS1;
}

unsigned char get_walker_db_entry(walker_t* walker, hash_entry_t** db_entry) {
    unsigned char found;
    unsigned int parent_dir;
    char name[13];

    found = find_leaf_parent(&db, walker->rel_path, &parent_dir, name);
    found = found && get_entry(&db, parent_dir, name, db_entry);
    return found;
}

unsigned char decide_status(walker_t* walker, hash_entry_t* db_entry) {
    unsigned __int32 crc;

    if (db_entry == NULL) {
        if (walker->current->attrib & _A_SUBDIR) {
            return DIR_IS_NEW;
        }
        return FILE_IS_NEW;
    }
    if (db_entry->type == TYPE_SKIP) {
        return FILE_IS_SKIP;
    }
    if (config.skip_if_archive_clear && !(walker->current->attrib & _A_ARCH)) {
        return FILE_IS_SAME;
    }
    if (db_entry->type == TYPE_FILE && !(walker->current->attrib & _A_SUBDIR)) {
        crc32file(walker->full_path, &crc);
        if (db_entry->crc == crc) {
            return FILE_IS_SAME;
        } else {
            return FILE_IS_CHANGED;
        }
    }
    return FILE_IS_UNKNOWN;
}

unsigned char decide_status_restore(walker_t* walker, hash_entry_t* db_entry, unsigned __int32* return_crc) {
    char crc_path[_MAX_PATH];
    unsigned int status;
    int crc_handle;
    char crc_buffer[9];
    unsigned int bytes_read;
    unsigned __int32 crc;
    char* endptr;

    if (db_entry == NULL) {
        if (walker->current->attrib & _A_SUBDIR) {
            return DIR_IS_NEW;
        }
        return FILE_IS_NEW;
    }
    if (db_entry->type == TYPE_SKIP) {
        return FILE_IS_FROM_DIFFERENT_SOURCE;
    }
    if (db_entry->type == TYPE_FILE && !(walker->current->attrib & _A_SUBDIR)) {
        status = join_path(crc_path, dest_path, "CRC")
            && join_path(crc_path, crc_path, walker->rel_path);
        if (status == FAIL0) {
            return FILE_IS_UNKNOWN;
        }
        status = (_dos_open(crc_path, O_RDONLY, &crc_handle) == SUCCESS0)
            && (_dos_read(crc_handle, crc_buffer, 8, &bytes_read) == SUCCESS0)
            && (_dos_close(crc_handle) == SUCCESS0);
        if (status == SUCCESS1) {
            crc_buffer[bytes_read] = '\0';
            crc = strtoul(crc_buffer, &endptr, 16);
            if (*endptr != '\0') {
                status = FAIL0;
            }
        }
        if (status == FAIL0) {
            // The CRC file was written incorrectly or is corrupted, or
            // there's no CRC file, or it couldn't be opened. Likely causes:
            // A save file was included in the new manifest accidentally
            // or was injected into the VHD and not subsequently backed up.
            // In case of the latter, we'll check the file's actual CRC.
            fputs("WARNING: Couldn't read CRC-32 hash from ", stderr);
            fputs(crc_path, stderr);
            fputs("\n", stderr);
            crc32file(walker->full_path, &crc);
        }
        *return_crc = crc;
        if (db_entry->crc == crc) {
            return FILE_IS_FROM_SAME_SOURCE;
        } else {
            return FILE_IS_FROM_DIFFERENT_SOURCE;
        }
    }
    // The manifest and BACKUP directory don't agree on whether this is
    // a directory or a file.
    return FILE_IS_UNKNOWN;
}

unsigned char save_crc(hash_entry_t* db_entry, char* to_path) {
    unsigned int status;
    int to_handle;
    char buffer[9];
    unsigned int bytes_written;

    mkdir_p(to_path, 0);
    status = _dos_creat(to_path, _A_NORMAL, &to_handle);
    if (status != SUCCESS0) {
        fputs("_dos_creat ", stderr);
        perror(to_path);
        _dos_close(to_handle);
        return FAIL0;
    }
    ultoa(db_entry->crc, buffer, 16); // About 150 bytes
    status = _dos_write(to_handle, buffer, 8, &bytes_written);
    if (status != SUCCESS0) {
        fputs("_dos_write ", stderr);
        perror(to_path);
        _dos_close(to_handle);
        return FAIL0;
    }
    return SUCCESS1;
}

unsigned long inject_tree() {
    unsigned long injected_count;
    char tmp_path[_MAX_PATH];
    walker_t walker;
    unsigned __int32 crc;

    injected_count = 0;
    if (!join_path(tmp_path, dest_path, "INJECT")) {
        return injected_count;
    }
    init_walker(&walker, tmp_path);
    walker.destructive = 1;
    while (walker_next(&walker)) {
        if (!join_path(tmp_path, src_path, walker.rel_path)) {
            continue;
        }
        if (walker.current->attrib & _A_SUBDIR) {
            fputs("Injecting directory ", stdout);
            fputs(walker.rel_path, stdout);
            fputs(" into ", stdout);
            fputs(src_path, stdout);
            fputs(" ...\n", stdout);
            mkdir_p(tmp_path, 1);
            inject_crc("_____DIR", 0, walker.rel_path);
        } else {
            fputs("Injecting file ", stdout);
            fputs(walker.rel_path, stdout);
            fputs(" into ", stdout);
            fputs(src_path, stdout);
            fputs(" ...\n", stdout);
            copy_file(walker.full_path, tmp_path);
            _dos_setfileattr(tmp_path, walker.current->attrib & ~_A_ARCH);
            crc32file(walker.full_path, &crc);
            inject_crc("", crc, walker.rel_path);
            remove(walker.full_path);
        }
        injected_count += 1;
    }
    free_walker(&walker);
    return injected_count;
}

unsigned long compare_src_to_manifest() {
    walker_t walker;
    hash_entry_t* db_entry;
    unsigned char item_status;
    char tmp_path[_MAX_PATH];
    unsigned long new_count;

    new_count = 0;
    init_walker(&walker, src_path);
    while (walker_next(&walker)) {
        if (should_ignore_file(walker.current->name)) {
            continue;
        }
        get_walker_db_entry(&walker, &db_entry);
        item_status = decide_status(&walker, db_entry);
        if (item_status == FILE_IS_UNKNOWN || item_status == FILE_IS_SKIP) {
            continue;
        }
        if (item_status != FILE_IS_SAME) {
            if (!join_path(tmp_path, dest_path, "BACKUP")
                    || !join_path(tmp_path, tmp_path, walker.rel_path)) {
                continue;
            }
            new_count += 1;
        }
        if (item_status == FILE_IS_NEW) {
            fputs("Copying new file ", stdout);
            fputs(walker.rel_path, stdout);
            fputs(" to ", stdout);
            fputs(dest_path, stdout);
            fputs(" ...\n", stdout);
            copy_file(walker.full_path, tmp_path);
        } else if (item_status == FILE_IS_CHANGED) {
            fputs("Copying modified file ", stdout);
            fputs(walker.rel_path, stdout);
            fputs(" to ", stdout);
            fputs(dest_path, stdout);
            fputs(" ...\n", stdout);
            copy_file(walker.full_path, tmp_path);
            if (join_path(tmp_path, dest_path, "CRC")
                    && join_path(tmp_path, tmp_path, walker.rel_path)) {
                save_crc(db_entry, tmp_path);
            }
        } else if (item_status == DIR_IS_NEW) {
            mkdir_p(tmp_path, 1);
        }
        _dos_setfileattr(walker.full_path, walker.current->attrib & ~_A_ARCH);
    }
    free_walker(&walker);
    return new_count;
}

void restore_backups() {
    walker_t walker;
    unsigned char explained;
    hash_entry_t* db_entry;
    unsigned char item_status;
    unsigned __int32 original_crc;
    char backup_path[_MAX_PATH];
    char tmp_path[_MAX_PATH];
    unsigned char should_restore;
    int choice;

    if (!join_path(backup_path, dest_path, "BACKUP")) {
        return;
    }
    init_walker(&walker, backup_path);
    explained = 0;
    while (walker_next(&walker)) {
        if (should_ignore_file(walker.current->name)) {
            continue;
        }
        get_walker_db_entry(&walker, &db_entry);
        item_status = decide_status_restore(&walker, db_entry, &original_crc);
        //printf("%u %s\n", item_status, walker.rel_path);
        should_restore = 0;
        if (item_status == FILE_IS_NEW || item_status == FILE_IS_FROM_SAME_SOURCE) {
            should_restore = 1;
        } else if (item_status == FILE_IS_FROM_DIFFERENT_SOURCE) {
            if (!explained) {
//                     12345678901234567890123456789012345678901234567890123456789012345678901234567890
                fputs("\n", stdout);
                fputs("While attempting to restore your save or configuration files, SAVIOR.EXE found\n", stdout);
                fputs("at least one conflicted file. A conflict is caused by this sequence of events:\n", stdout);
                fputs(" 1. A game is distributed with version A of a file.\n", stdout);
                fputs(" 2. The game modifies that file to save your progress and/or settings.\n", stdout);
                fputs(" 3. SAVIOR.EXE creates a backup copy of that file because it was modified.\n", stdout);
                fputs(" 4. You install a new copy of the game that includes version B of the file.\n", stdout);
                fputs("\n", stdout);
                fputs("You may choose to restore your backed-up copy of the file or use version B.\n", stdout);
                fputs("By choosing to restore, you can keep your progress and/or settings, but you\n", stdout);
                fputs("might miss out on improvements made in version B of the file, and there's a\n", stdout);
                fputs("chance your restored file won't be compatible with the new copy of the game.\n", stdout);
                fputs("\n", stdout);
                fputs("If you choose not to restore, your backed-up copy will be left in the backup\n", stdout);
                fputs("directory. However, it will be overwritten if the game modifies version B\n", stdout);
                fputs("of the file and then SAVIOR.EXE backs up that modified file.\n", stdout);
                fputs("\n", stdout);
                explained = 1;
            }
            while (1) {
                fputs(walker.rel_path, stdout);
                fputs(" is conflicted. Restore it? (y/n) ", stdout);
                fflush(stdout);
                choice = getche();
                if (choice != '\r' && choice != '\n') {
                    fputs("\n", stdout);
                }
                if (choice == 'Y' || choice == 'y') {
                    should_restore = 1;
                    inject_crc("", original_crc, walker.rel_path);
                    break;
                } else if (choice == 'N' || choice == 'n') {
                    fputs("You chose not to restore this file.\n", stdout);
                    break;
                }
            }
        }
        if (should_restore) {
            if (!join_path(tmp_path, src_path, walker.rel_path)) {
                continue;
            }
            fputs("Restoring file ", stdout);
            fputs(walker.rel_path, stdout);
            fputs(" to ", stdout);
            fputs(tmp_path, stdout);
            fputs(" ...\n", stdout);
            copy_file(walker.full_path, tmp_path);
            _dos_setfileattr(tmp_path, walker.current->attrib & ~_A_ARCH);
        }
    }
    free_walker(&walker);
}

int main(int argc, char** argv) {
    unsigned char first_run;
    char manifest_path[_MAX_PATH];
    unsigned char has_manifest;
    unsigned char requested_help;
    unsigned long new_count;
    int a;

#if HANDLE_SIGNAL
    signal(SIGINT, handle_sigint);
#endif
    fputs("SAVIOR.EXE v0.9 beta - A backup & restore utility for save & config files.\n", stdout);
    config.skip_if_archive_clear = 0;
    first_run = 0;
    requested_help = 0;
    src_path = NULL;
    dest_path = NULL;
    for (a = 1; a < argc; a += 1) {
        if (argv[a][0] == '/') {
            if (argv[a][1] == 'a' || argv[a][1] == 'A') {
                config.skip_if_archive_clear = 1;
            } else if (argv[a][1] == '?') {
                requested_help = 1;
            }
            continue;
        }
        if (strlen(argv[a]) + 1 + 12 + 1 > _MAX_PATH) {
            // This might never happen.
            // The maximum length of argv[a] seems to be 123.
            fputs("ERROR: Given path is too long.\n", stderr);
            return FAIL1;
        }
        if (src_path == NULL) {
            src_path = _fullpath(NULL, argv[a], _MAX_PATH);
        } else {
            dest_path = _fullpath(NULL, argv[a], _MAX_PATH);
        }
    }
    if (src_path == NULL || requested_help) {
        print_help();
        return FAIL1;
    }
    remove_trailing_backslashes(src_path, 3);
    if (!check_path(src_path)) {
        fputs("ERROR: Couldn't open the source path.\n", stderr);
        return FAIL1;
    }
    if (dest_path != NULL) {
        remove_trailing_backslashes(dest_path, 3);
        if (paths_are_nested(src_path, dest_path)) {
            fputs("ERROR: Source can't be inside destination or vice-versa.\n", stderr);
            return FAIL1;
        }
        mkdir_p(dest_path, 1);
        if (!check_path(dest_path)) {
            fputs("ERROR: Couldn't open the destination path.\n", stderr);
            return FAIL1;
        }
        create_dest_subdirs();
    }
    precompute_tables();
    if (!join_path(manifest_path, src_path, MANIFEST_NAME)) {
        free(dest_path);
        free(src_path);
        return FAIL1;
    }
    if (dest_path == NULL) {
        fputs("Creating ", stdout);
        fputs(manifest_path, stdout);
        fputs(" ...\n", stdout);
        if (!build_manifest(manifest_path)) {
            fputs("ERROR: Couldn't build manifest.\n", stderr);
            free(dest_path);
            free(src_path);
            return FAIL1;
        }
    } else {
        init_db(&db, HASH_ENTRIES);
        has_manifest = read_manifest(&db, manifest_path);
        if (!has_manifest) {
            first_run = 1;
            fputs("Creating ", stdout);
            fputs(manifest_path, stdout);
            fputs(" ...\n", stdout);
            if (!build_manifest(manifest_path)) {
                free(dest_path);
                free(src_path);
                free(db.data);
                return FAIL1;
            }
            has_manifest = read_manifest(&db, manifest_path);
            if (!has_manifest) {
                perror(manifest_path);
                free(dest_path);
                free(src_path);
                free(db.data);
                return FAIL1;
            }
        }
        inject_tree();
        if (!join_path(manifest_path, src_path, INJECTED_NAME)) {
            free(db.data);
            free(dest_path);
            free(src_path);
            return FAIL1;
        }
        read_manifest(&db, manifest_path);
        if (!first_run) {
            new_count = compare_src_to_manifest();
            first_run = (new_count == 0);
        }
        if (first_run) {
            restore_backups();
        }
        free(db.data);
    }
    free(dest_path);
    free(src_path);
    return SUCCESS0;
}
