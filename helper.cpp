#include "helper.h"
#include <cstring>
#include <ctime>
#include <cstdio>
#include <sys/types.h>
#include <sys/stat.h>
#ifndef S_ISREG
#define	S_ISREG(m)	(((m) & S_IFMT) == S_IFREG)
#endif
#ifndef S_ISDIR
#define	S_ISDIR(m)	(((m) & S_IFMT) == S_IFDIR)
#endif

#include <fstream>

uint8_t clamp(int32_t val)
{
	if (val < 0) {
		return 0;
	}
	if (val > 255) {
		return 255;
	}
	return (uint8_t)val;
}

std::string base36(int val)
{
	if (val < 0) {
		return std::string("-") + base36(-val);
	}
	if (val / 36 == 0) {
		if (val < 10) {
			char x = '0' + val;
			return std::string(&x, 1);
		}
		char x = 'a' + (val - 10);
		return std::string(&x, 1);
	}
	return base36(val / 36) + base36(val % 36);
}

int base10(const std::string& val)
{
	return atoi(val.c_str());
	/*
	//printf("Turning %s into ", val);
	int res = 0;
	bool neg = false;
	if (*val == '-') {
		neg = true;
		++val;
	}
	for (;;) {
		if (*val >= '0' && *val <= '9') {
			res = res * 36 + (*val++ - '0');
			continue;
		}
		if (*val >= 'a' && *val <= 'z') {
			res = res * 36 + 10 + (*val++ - 'a');
			continue;
		}
		if (*val >= 'A' && *val <= 'Z') {
			res = res * 36 + 10 + (*val++ - 'A');
			continue;
		}
		break;
	}
	if (neg) {
		res *= -1;
	}
	//printf("%d\n", res);
	return res;
	*/
}

void printProgress(const size_t current, const size_t max)
{
	static float lastp = -10;
	static time_t lastt = 0;
	if (current == 0) { // Reset
		lastp = -10;
		lastt = 0;
	}
	time_t now = time(NULL);
	if (now > lastt || current == max) {
		float proc = (float(current) / float(max)) * 100.0f;
		if (proc > lastp + 0.99f || current == max) {
			// Keep user updated but don't spam the console
			printf("[%.2f%%]\r", proc);
			fflush(stdout);
			lastt = now;
			lastp = proc;
		}
	}
}

bool fileExists(const std::string& strFilename)
{
	if (strFilename.empty()) return false;
	std::ifstream f(strFilename);
	return f.good();

	/*
	struct stat stFileInfo;
	int ret;
	ret = stat(strFilename, &stFileInfo);
	if(ret == 0) {
		return S_ISREG(stFileInfo.st_mode);
	}
	return false;
	*/
}

bool dirExists(const std::string& strFilename)
{
	struct stat stFileInfo;
	int ret;
	ret = stat(strFilename.c_str(), &stFileInfo);
	if(ret == 0) {
		return S_ISDIR(stFileInfo.st_mode);
	}
	return false;
}

bool isNumeric(const std::string& str)
{
	return isdigit(str.c_str()[0]);
}

bool isAlphaWorld(const std::string& path)
{
	std::string pathToFile = path + "/level.dat";
	return fileExists(pathToFile);
}

bool strEndsWith(std::string const &fullString, std::string const &ending) {
	if (fullString.length() >= ending.length()) {
		return (0 == fullString.compare(fullString.length() - ending.length(), ending.length(), ending));
	}
	else {
		return false;
	}
}