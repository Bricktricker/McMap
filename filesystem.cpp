#include "filesystem.h"

#ifdef MSVCP
#include <sys/stat.h>  
#include <stdio.h>
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <unistd.h>
#endif

#include <string>
#include <vector>
#include <locale>
#include <codecvt>
#include <fstream>

std::wstring stdStringToWstring(const std::string& str) {
	const std::ctype<wchar_t>& CType = std::use_facet<std::ctype<wchar_t> >(std::locale());
	std::vector<wchar_t> wideStringBuffer(str.length());
	CType.widen(str.data(), str.data() + str.length(), &wideStringBuffer[0]);
	return std::wstring(&wideStringBuffer[0], wideStringBuffer.size());
}

std::string wStringToStdString(const std::wstring& str) {
	return std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(str);
}

std::string wStringToStdString(const wchar_t* str) {
	return std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(str);
}

namespace Dir
{
	DIRHANDLE open(const std::string& path, myFile &file)
	{
		if (path.empty()) {
			return NULL;
		}
#ifdef MSVCP
		std::string buffer;
		std::wstring wbuffer(1000, 0);
		_WIN32_FIND_DATAW ffd;
		buffer = path + "/*";
		wbuffer = stdStringToWstring(buffer);
		HANDLE h = FindFirstFileW(wbuffer.c_str(), &ffd);
		if (h == INVALID_HANDLE_VALUE) {
			return NULL;
		}
		file.name = wStringToStdString(ffd.cFileName);
		file.isdir = ((ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY);
		file.size = ffd.nFileSizeLow;
#else
		DIR *h = opendir(path.c_str());
		if (h == NULL) {
			return NULL;
		}
		dirent *dirp = readdir(h);
		if (dirp == NULL) {
			closedir(h);
			return NULL;
		}
		std::string buffer = path + "/" + dirp->d_name;
		struct stat stDirInfo;
		if (stat(buffer.c_str(), &stDirInfo) < 0) {
			closedir(h);
			return NULL;
		}
		file.name = dirp->d_name;
		file.isdir = S_ISDIR(stDirInfo.st_mode);
		file.size = stDirInfo.st_size;
#endif
		return h;
	}

	bool next(DIRHANDLE handle, const std::string& path, myFile &file)
	{
#ifdef MSVCP
		_WIN32_FIND_DATAW ffd;
		bool ret = FindNextFileW(handle, &ffd) == TRUE;
		if (!ret) {
			return false;
		}
		file.name = wStringToStdString(ffd.cFileName);
		file.isdir = ((ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY);
		file.size = ffd.nFileSizeLow;
#else
		dirent *dirp = readdir(handle);
		if (dirp == NULL) {
			return false;
		}
		std::string buffer = path + "/" + dirp->d_name;
		struct stat stDirInfo;
		if (stat(buffer.c_str(), &stDirInfo) < 0) {
			return false;
		}
		file.name = dirp->d_name;
		file.isdir = S_ISDIR(stDirInfo.st_mode);
		file.size = stDirInfo.st_size;
#endif
		return true;
	}

	void close(DIRHANDLE handle)
	{
#ifdef MSVCP
		FindClose(handle);
#else
		closedir(handle);
#endif
	}

	bool createDir(const std::string& path)
	{
#ifdef MSVCP //or _WIN32
		const auto ret = CreateDirectory(stdStringToWstring(path).c_str(), NULL);
		return (ret != 0) || (GetLastError() == ERROR_ALREADY_EXISTS);
#else
		const int ret = mkdir(path.c_str(), 0755);
		return ret == 0;
#endif
	}

	bool dirExists(const std::string& strFilename)
	{
		struct stat info;

		if (stat(strFilename.c_str(), &info) != 0)
			return false;
		else if (info.st_mode & S_IFDIR)
			return true;
		else
			return false;
	}

	bool fileExists(const std::string& strFilename)
	{
		if (strFilename.empty()) return false;
		std::ifstream f(strFilename);
		return f.good();
	}

}
