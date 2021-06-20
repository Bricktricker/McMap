#include <fstream>
#include <regex>
#include <cassert>
#include <limits>
#include "helper.h"
#include "filesystem.h"

namespace helper {
	//clamps value between 0 and 255
	uint8_t clamp(int32_t val){
		if (val < 0) {
			return 0;
		}
		if (val > 255) {
			return 255;
		}
		return static_cast<uint8_t>(val);
	}

	void printProgress(const size_t current, const size_t max){
		static float lastp = -10;
		static time_t lastt = 0;
		if (current == 0) { // Reset
			lastp = -10;
			lastt = 0;
		}
		const time_t now = time(NULL);
		if (now > lastt || current == max) {
			const float proc = (float(current) / float(max)) * 100.0f;
			if (proc > lastp + 0.99f || current == max) {
				// Keep user updated but don't spam the console
				printf("[%.2f%%]\r", static_cast<double>(proc));
				fflush(stdout);
				lastt = now;
				lastp = proc;
			}
		}
	}

	bool isNumeric(const std::string& str){
		return std::regex_match(str, std::regex("(\\+|\\-)?\\d+(\\.\\d+)?")); //old: "[(-|+)|][0-9]+"
	}

	bool isWorld(const std::string& path){
		std::string pathToFile = path + "/level.dat";
		return Dir::fileExists(pathToFile);
	}

	bool strEndsWith(std::string const &fullString, std::string const &ending) {
		if (fullString.length() >= ending.length()) {
			return (0 == fullString.compare(fullString.length() - ending.length(), ending.length(), ending));
		}
		else {
			return false;
		}
	}

	std::vector<std::string> strSplit(const std::string &s, char delim) {
		std::vector<std::string> elems;
		strSplit<decltype(std::back_inserter(elems))>(s, delim, std::back_inserter(elems));
		return elems;
	}

}