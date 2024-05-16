#ifndef FILESYS_H
#define FILESYS_H

void remove_trailing_backslashes(char* path, unsigned char min_length);
unsigned char join_path(char* out, char* in1, char* in2);
unsigned char paths_are_nested(char* path1, char* path2);
void mkdir_p(char* path, unsigned char make_leaf);
unsigned char should_ignore_file(char* name);
unsigned char copy_file(char* from_path, char* to_path);

#endif /* FILESYS.H */
