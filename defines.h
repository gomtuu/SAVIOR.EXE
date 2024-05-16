#ifndef DEFINES_H
#define DEFINES_H

#define DEBUG 0
#define debug_print(fmt, ...) \
    do { if (DEBUG) printf(fmt, __VA_ARGS__); } while (0)

#define FAIL0 0
#define FAIL1 1
#define SUCCESS0 0
#define SUCCESS1 1

#define FILE_IS_UNKNOWN 0
#define FILE_IS_SKIP 1
#define FILE_IS_SAME 2
#define FILE_IS_NEW 3
#define DIR_IS_NEW 4
#define FILE_IS_CHANGED 5
#define FILE_IS_FROM_SAME_SOURCE 6
#define FILE_IS_FROM_DIFFERENT_SOURCE 7

#endif /* DEFINES_H */
