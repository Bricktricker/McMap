#ifndef _FILESYSTEM_H_
#define _FILESYSTEM_H_

#if defined(_WIN32) && !defined(__GNUC__)
#	define MSVCP
#	include <windows.h>
typedef HANDLE DIRHANDLE;
#else
#	include <dirent.h>
typedef DIR *DIRHANDLE;
#endif

#include <string>

struct myFile {
	std::string name;
	bool isdir;
	unsigned long size;
};

namespace Dir
{
	DIRHANDLE open(const std::string& path, myFile &file);
	bool next(DIRHANDLE handle, const std::string& path, myFile &file);
	void close(DIRHANDLE handle);
}

#endif
