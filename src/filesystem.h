#pragma once
#include <string>

namespace Dir
{
	bool createDir(const std::string& path);
	bool dirExists(const std::string& strFilename);
	bool fileExists(const std::string& strFilename);
}
