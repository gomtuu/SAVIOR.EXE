Compiler_options = -q -onatx -oh -oi -ei -mc -zp=8 -4
Linker_options = -q -4 -zl -zld

Object_files = savior.obj &
	crc32.obj &
	db.obj &
	filesys.obj &
	manifest.obj &
	walker.obj

savior.exe: $(Object_files)
	*wcl $(Linker_options) $(Object_files)

.c.obj:
	*wcc $(Compiler_options) $<

clean: .SYMBOLIC
	if exist *.obj del *.obj
	if exist savior.exe del savior.exe
