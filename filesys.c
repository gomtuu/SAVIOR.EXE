#include <direct.h>
#include <dos.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "defines.h"
#include "filesys.h"
#include "manifest.h"

void remove_trailing_backslashes(char* path, unsigned char min_length) {
    unsigned char length;

    length = strlen(path);
    while (length > min_length) {
        length -= 1;
        if (path[length] == '\\') {
            path[length] = '\0';
        } else {
            break;
        }
    }
}

unsigned char join_path(char* out, char* in1, char* in2) {
    unsigned char part;
    unsigned char length;
    char sep[2] = "\\";
    char* in_cursor;
    char* out_cursor;
    char* backslash_cursor;

    part = 0;
    length = 0;
    in_cursor = in1;
    out_cursor = out;
    backslash_cursor = out;
    while (part < 3) {
        *out_cursor = *in_cursor;
        if (*in_cursor == '\0') {
            if (part == 0) {
                length = backslash_cursor - out;
                out_cursor = backslash_cursor;
                in_cursor = &sep;
            } else if (part == 1) {
                in_cursor = in2;
            }
            part += 1;
        } else {
            if (*in_cursor != '\\') {
                backslash_cursor = out_cursor + 1;
            }
            in_cursor += 1;
            out_cursor += 1;
            length += 1;
            if (length > _MAX_PATH) {
                fputs("Couldn't join paths: ", stderr);
                fputs(in1, stderr);
                fputs(" and ", stderr);
                fputs(in2, stderr);
                fputs("\n", stderr);
                return FAIL0;
            }
        }
    }
    return SUCCESS1;
}

unsigned char paths_are_nested(char* path1, char* path2) {
    while (*path1 != '\0' && *path2 != '\0') {
        if (*path1 != *path2) {
            return 0;
        }
        path1 += 1;
        path2 += 1;
    }
    return 1;
}

void mkdir_p(char* path, unsigned char make_leaf) {
    unsigned char is_leaf;
    char* cursor2;

    is_leaf = 0;
    cursor2 = path + 3;
    while (!is_leaf) {
        while (*cursor2 != '\\' && *cursor2 != '\0') {
            cursor2 += 1;
        }
        if (*cursor2 == '\0') {
            is_leaf = 1;
        }
        *cursor2 = '\0';
        if (!is_leaf || make_leaf) {
            _mkdir(path);
        }
        if (!is_leaf) {
            *cursor2 = '\\';
            cursor2 += 1;
        }
    }
}

unsigned char should_ignore_file(char* name) {
    return strcmp(name, MANIFEST_TMP) == 0
        || strcmp(name, MANIFEST_NAME) == 0
        || strcmp(name, INJECTED_NAME) == 0
        || strcmp(name, "BOOTLOG.TXT") == 0
        || strcmp(name, "BOOTLOG.PRV") == 0;
}

unsigned char copy_file(char* from_path, char* to_path) {
    unsigned int status;
    unsigned int from_attrib;
    int from_handle;
    unsigned int from_date;
    unsigned int from_time;
    int to_handle;
    char* buffer;
    unsigned int bytes_read;
    unsigned int bytes_written;

    // TODO: Make this less verbose?
    mkdir_p(to_path, 0);
    status = _dos_getfileattr(from_path, &from_attrib);
    if (status != SUCCESS0) {
        fputs("_dos_getfileattr ", stderr);
        perror(from_path);
        return FAIL0;
    }
    status = _dos_open(from_path, O_RDONLY, &from_handle);
    if (status != SUCCESS0) {
        fputs("_dos_open ", stderr);
        perror(from_path);
        return FAIL0;
    }
    status = _dos_getftime(from_handle, &from_date, &from_time);
    if (status != SUCCESS0) {
        fputs("_dos_getftime ", stderr);
        _dos_close(from_handle);
        perror(from_path);
        return FAIL0;
    }
    status = _dos_creat(to_path, from_attrib, &to_handle);
    if (status != SUCCESS0) {
        fputs("_dos_creat ", stderr);
        _dos_close(from_handle);
        perror(to_path);
        return FAIL0;
    }
    buffer = malloc(32768);
    do {
        status = _dos_read(from_handle, buffer, 32768, &bytes_read);
        if (status != SUCCESS0) {
            fputs("_dos_read ", stderr);
            _dos_close(from_handle);
            _dos_close(to_handle);
            perror(from_path);
            return FAIL0;
        }
        status = _dos_write(to_handle, buffer, bytes_read, &bytes_written);
        if (status != SUCCESS0) {
            fputs("_dos_write ", stderr);
            _dos_close(from_handle);
            _dos_close(to_handle);
            perror(to_path);
            return FAIL0;
        }
    } while (bytes_read);
    free(buffer);
    status = _dos_close(from_handle);
    if (status != SUCCESS0) {
        fputs("_dos_close ", stderr);
        _dos_close(to_handle);
        perror(from_path);
        return FAIL0;
    }
    status = _dos_setftime(to_handle, from_date, from_time);
    if (status != SUCCESS0) {
        fputs("_dos_setftime ", stderr);
        _dos_close(to_handle);
        perror(to_path);
        return FAIL0;
    }
    status = _dos_close(to_handle);
    if (status != SUCCESS0) {
        fputs("_dos_close ", stderr);
        perror(to_path);
        return FAIL0;
    }
    status = _dos_setfileattr(to_path, from_attrib);
    if (status != SUCCESS0) {
        fputs("_dos_setfileattr ", stderr);
        perror(to_path);
        return FAIL0;
    }
    return SUCCESS1;
}
