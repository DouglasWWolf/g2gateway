//=================================================================================================
// filesys.h - provides filesystem support functions
//=================================================================================================
#pragma once
#include "cppstring.h"

// Remount the file-system read/write
void    remount_rw();

// Remount the file-system read-only
void    remount_ro();

// Returns true if the specified file exists
bool    file_exists(const char* filename);

// Returns the parent directory of a file
PString parent_dir(const char* filename);

// Copies a file, optionally stripping off a standard package header
bool    copy_file(const char* source_fn, const char* dest_fn, bool strip=false);

// Find the name of the current working directory
PString get_cwd();
