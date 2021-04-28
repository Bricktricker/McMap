#include <string>
#include <filesystem>
#include "filesystem.h"

namespace Dir
{
	bool createDir(const std::string& path)
	{
		return std::filesystem::create_directory(path);
	}

	bool dirExists(const std::string& strFilename)
	{
		return std::filesystem::exists(strFilename) && std::filesystem::is_directory(strFilename);
	}
	bool fileExists(const std::string& strFilename)
	{
		return std::filesystem::exists(strFilename) && !std::filesystem::is_directory(strFilename);
	}
}
