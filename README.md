# SAVIOR.EXE
A general, automatic solution to the problem of backing up and restoring save and config files for 0MHz-style VHD games for the AO486 MiSTer FPGA core.

## Introduction

Here's a summary of how it works.

A DOS program called `SAVIOR.EXE` is added to `AUTOEXEC.BAT` or `RUNGAME.BAT` (along with `MISTERFS.EXE`), both before and after the game runs.

The first time you boot a VHD, this program makes a list of all the files in the VHD, along with their CRC-32 hashes. It stores this list as `MANIFEST.VIO` in the VHD. It then checks for previously backed-up save files and config files on a MiSTerFS drive and attempts to restore them.

Then, whenever you boot that same VHD *and* whenever you exit the game, `SAVIOR.EXE` searches the VHD for files not listed in the manifest and files with CRC-32 hashes that differ from those in the manifest, and it backs them up to a MiSTerFS drive.

## Details

### Usage Example

In `AUTOEXEC.BAT`:
```
MISTER\MISTERFS.EXE S /q
SAVIOR.EXE \ S:\SAVIOR\KEEN1 /a
MISTER\MISTERFS.EXE /u /q
CD KEEN1
CALL RUNGAME.BAT
CD ..
MISTER\MISTERFS.EXE S /q
SAVIOR.EXE \ S:\SAVIOR\KEEN1 /a
MISTER\MISTERFS.EXE /u /q
```

Or in `RUNGAME.BAT`:
```
MISTER\MISTERFS.EXE S /q
SAVIOR.EXE \ S:\SAVIOR\KEEN1 /a
MISTER\MISTERFS.EXE /u /q
KEEN1.EXE
MISTER\MISTERFS.EXE S /q
SAVIOR.EXE \ S:\SAVIOR\KEEN1 /a
MISTER\MISTERFS.EXE /u /q
```

This will cause `SAVIOR.EXE` to scan the VHD's root directory `\` and create `MANIFEST.VIO` in the same directory, or load `MANIFEST.VIO` if it already exists. It will save backups to (and potentially restore them from) `S:\SAVIOR\KEEN1`.

### Before and After

Why does `SAVIOR.EXE` run before the game starts *and* after the game exits?

- It runs before the game so that it can restore any existing backups before you start playing. (So if you've downloaded a new VHD of the game, you can pick up where you left off.) Also, running before the game means your save and config files can get backed up eventually, even if you never cleanly exit the game.
- It runs after the game so that save and config files are backed up as soon as possible after they're written. If you cleanly exit the game, you shouldn't have to start it again just to back up your data.

### Backing Up Files

#### Save Files

A save file is a file that did not exist when the manifest was created. Save files are always backed up.

#### Configuration Files and Self-Modifying Executables

Some VHDs include files that are necessary for the game to function but can be modified by the game. The most common examples of this are probably configuration files, but there are others, such as Starflight's self-modifying executables. This is why `SAVIOR.EXE` checks CRC-32 hashes instead of just checking for files not listed in the manifest. By checking every file for changes, it can identify all the files that need to be backed up in order to restore the game's state.

`SAVIOR.EXE` could use modification dates and file sizes instead, but it's possible for the contents of a file to change without the date or size changing, and vice-versa. For example, if a new VHD is released for a game, all of its files might have more recent modification dates than they do in the older VHD, even though they're the same files. (To be fair, CRC-32 hashes aren't 100% reliable either, but the probability of failure is about one in four billion.)

### Archive Bit

In FAT filesystems, each file has an "archive bit" that can be set or unset. DOS sets a file's archive bit whenever a file is written to. Backup programs can clear a file's archive bit after making a backup copy of that file. The archive bit thus allows backup programs to detect files that have been changed since the last backup.

When `SAVIOR.EXE` backs up a file, or when it determines that a file's CRC-32 hash matches the hash in the manifest, it clears the file's archive bit.

If you run `SAVIOR.EXE` with the `/a` option, it will skip files with unset archive bits when determining whether a file needs to be backed up. This means it won't bother computing their CRC-32 hashes, which will save time.

It should be safe to use the `/a` option in most cases, but if a game messes with its own archive bits for some reason, then this option might cause `SAVIOR.EXE` to skip a file that should be backed up.

VHD authors who use the `/a` option might want to clear the archive bit of all the files in their VHDs before distributing them. This will speed up the initial boot if they also bundle a manifest.

### Restoring Files

When `SAVIOR.EXE` runs, it checks to see if it should restore files. There are two conditions that will cause it to restore files.

1. `MANIFEST.VIO` doesn't exist because the VHD hasn't been booted before. In this case, `SAVIOR.EXE` will first create a manifest, then attempt to restore files.
2. The manifest exists (perhaps because the VHD author included one), but no files in the VHD are found to be new or modified. This would indicate a clean install of a game, and if you have files backed up, you'd probably want to restore them.

### Resolving Conflicts

If a clean (freshly downloaded) VHD contains a configuration file, and the game modifies it at some point, and the modified version gets backed up, what should happen if the first VHD is replaced with a second clean VHD that includes a different version of the same config file? While `SAVIOR.EXE` is attempting to restore backed-up files, it will detect such a conflict and ask you whether you really want to restore your backup, which is based on an older version of the file.

When `SAVIOR.EXE` saves a backup, it also stores the CRC-32 hash of the original file (i.e., its hash from the manifest). That way, it can tell whether your backed-up copy is a customized version of the *same* file, which would **not** be a conflict, or a customized version of a *different* file, which would be a conflict.

If you choose to restore your config file, the backed-up CRC-32 hash (i.e., the one that was copied from the first VHD's manifest) and the filename are saved to `INJECTED.VIO`. Entries in this file act like entries in `MANIFEST.VIO` but supersede them. This means subsequent backups of the file will retain the original CRC-32 hash from the first VHD's manifest, so you will be prompted to upgrade again if your replace the VHD again (even if the file in the third VHD is the same as the file in the second VHD).

### Injecting Files

If you want to force `SAVIOR.EXE` to restore a file, you can inject it into the VHD by placing it in a special MiSTerFS directory (e.g. `S:\SAVIOR\MYGAME\INJECT`). The file will be moved into place in the VHD next time it's booted. Its hash (computed upon injection) and name will be stored in `INJECTED.VIO`.

One interesting way this could be used is to inject alternate config files, such as for MT-32 support. Instead of distributing two whole copies of a game with different sound settings, a VHD author could package one with Sound Blaster support enabled, and another that contains only an MT-32 version of the config file that unzips to `games/AO486/shared/SAVIOR/MYGAME/INJECT/SOUND.CFG`.

MT-32 users would just have to download and unzip both files. `SAVIOR.EXE` would move the MT-32 config file into place the first time it ran.

### Built-In Manifests

VHD authors *may* include a manifest in their VHD images. You can force `SAVIOR.EXE` to generate a manifest (and do nothing else) by running it without a destination path.

Manifests can contain extra information about each file, such as a flag that means "this is known to be a static file, not a save or config file that we might want to back up, so don't bother computing its hash." Using this flag will make `SAVIOR.EXE` run more quickly. But this is just an optimization step and isn't mandatory.

Comments are also allowed in the manifest file. VHD authors can use comments to explain what various files are for. (Example: if the VHD contains a trainer, that file can be labeled as a custom addition to the VHD.) Any part of a line that follows a semicolon is considered a comment.

### Shared Directory Layout

The MiSTerFS shared directory for a particular game is specified as the second path passed to `SAVIOR.EXE` (e.g. `S:\SAVIOR\KEEN1`). Within that directory, you'll find these subdirectories:

- `BACKUP` (used for storing the config and save files themselves)
- `CRC` (used for storing the CRC-32 hashes copied from the manifest)
- `INJECT` (files placed here will be injected into the VHD next time it's booted)

Each subdirectory internally uses the same tree structure, which mirrors the structure of the VHD. So all of the following files would correspond to each other:

- `C:\DATA\CONFIG.DAT` (the config file in the VHD image)
- `S:\SAVIOR\MYGAME\BACKUP\DATA\CONFIG.DAT` (backup of the config file itself)
- `S:\SAVIOR\MYGAME\CRC\DATA\CONFIG.DAT` (stores this file's CRC-32 copied from the manifest)
- `S:\SAVIOR\MYGAME\INJECT\DATA\CONFIG.DAT` (will overwrite `C:\DATA\CONFIG.DAT` upon next boot)

### Building

This application was developed using the Open Watcom 1.9 C compiler. Use `WMAKE.EXE` to build it.

### Limitations

`SAVIOR.EXE` runs in real mode so the executable can be smaller. This means it can currently only keep track of 3200 files due to RAM limitations.

If `SAVIOR.EXE` runs after the CPU has been throttled by `SYSCTL.EXE`, then computing the CRC-32 hashes of all the files on the VHD might take a long time. On the other hand, games that require a low CPU speed probably don't have a lot of big files.

A typical implementation of `SAVIOR.EXE` relies on MiSTerFS, which requires DOS 5.00 or greater.

`SAVIOR.EXE` only supports short (8.3) filenames.

CRC-32 is fast, which is good given the speed of the AO486 core, but it's not the best hashing algorithm. It will have more false matches than other algorithms would.

### Acknowledgements

This program uses the slicing-by-4 CRC-32 algorithm, published by Intel.
