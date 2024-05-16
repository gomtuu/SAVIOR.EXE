/*
**  CRC.H - header file for SNIPPETS CRC and checksum functions
*/

// Adapted from:
// http://web.archive.org/web/20080303093609/http://c.snippets.org/snip_lister.php?fname=crc.h

#ifndef CRC32_H
#define CRC32_H

#include <stdlib.h>           /* For size_t                 */
typedef unsigned char BYTE;
typedef unsigned __int32 DWORD;

/*
**  File: CRC_32.C
*/

void precompute_tables();
void fput_crc(unsigned __int32 crc, FILE* fp);
unsigned __int32 fget_crc(FILE* fp);
int crc32file(char *name, DWORD *crc);
unsigned __int32 crc32buffer(void* buffer, unsigned int length, unsigned __int32 crc);

#endif /* CRC32_H */
