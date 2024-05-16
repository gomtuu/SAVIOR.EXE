#include <dos.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include "crc32.h"

// Adapted from:
// http://web.archive.org/web/20100818224146/http://c.snippets.org/snip_lister.php?fname=crc_32.c
/* Copyright (C) 1986 Gary S. Brown.  You may use this program, or
   code or tables extracted from it, as desired without restriction.*/
// and:
// https://create.stephan-brumme.com/crc32/#slicing-by-8-overview

unsigned __int32 crc_32_tab[4][256];
const char* hex = "0123456789abcdef";

void precompute_tables() {
    static unsigned __int32 crc_polynomial = 0xedb88320;
    unsigned int i, j;
    for (i = 0; i < 256; i += 1) {
        unsigned __int32 crc = i;
        for (j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (-(crc & 1) & crc_polynomial);
        }
        crc_32_tab[0][i] = crc;
    }
    for (i = 0; i < 256; i += 1) {
        crc_32_tab[1][i] = (crc_32_tab[0][i] >> 8) ^ crc_32_tab[0][crc_32_tab[0][i] & 0xff];
        crc_32_tab[2][i] = (crc_32_tab[1][i] >> 8) ^ crc_32_tab[0][crc_32_tab[1][i] & 0xff];
        crc_32_tab[3][i] = (crc_32_tab[2][i] >> 8) ^ crc_32_tab[0][crc_32_tab[2][i] & 0xff];
    }
}

void fput_crc(unsigned __int32 crc, FILE* fp) {
    int n;
    unsigned char nybble;

    for (n = 28; n >= 0; n -= 4) {
        nybble = (crc >> n) & 0xf;
        fputc(hex[nybble], fp);
    }
}

int crc32file(char *name, DWORD *final_crc) {
    unsigned bytes_read;
    int handle;
    unsigned __int32* buffer;
    register DWORD crc;

    crc = 0;
    if (_dos_open(name, O_RDONLY, &handle) != 0) {
        perror(name);
        return 0;
    }
    buffer = malloc(16384);
    do {
        _dos_read(handle, buffer, 16384, &bytes_read);
        crc = crc32buffer(buffer, bytes_read, crc);
    } while (bytes_read);
    free(buffer);
    _dos_close(handle);
    *final_crc = crc;
    return 1;
}

unsigned __int32 crc32buffer(void* buffer, unsigned int length, unsigned __int32 crc) {
    unsigned __int32* current = (unsigned __int32*)buffer;
    unsigned char* current_char;

    crc = ~crc;
    while (length >= 4) {
        crc ^= *current;
        crc = crc_32_tab[3][crc & 0xff] ^
            crc_32_tab[2][(crc >> 8) & 0xff] ^
            crc_32_tab[1][(crc >> 16) & 0xff] ^
            crc_32_tab[0][crc >> 24];
        current += 1;
        length -= 4;
    }
    current_char = (unsigned char*)current;
    while (length > 0) {
        crc = (crc >> 8) ^ crc_32_tab[0][(crc & 0xff) ^ *current_char];
        current_char += 1;
        length -= 1;
    }
    return ~crc;
}
