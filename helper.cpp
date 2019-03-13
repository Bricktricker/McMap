#include "helper.h"
#include "filesystem.h"
#include <fstream>
#include <regex>
#include <cassert>
#include <limits>

//clamps value between 0 and 255
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

bool isNumeric(const std::string& str)
{
	return std::regex_match(str, std::regex("(\\+|\\-)?\\d+(\\.\\d+)?")); //old: "[(-|+)|][0-9]+"
}

bool isAlphaWorld(const std::string& path)
{
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

size_t getVal(const std::vector<uint64_t>& arr, const size_t index, const size_t lengthOfOne) {
#ifdef _DEBUG
	const size_t maxObj = (arr.size() * numBits<uint64_t>()) / lengthOfOne;
	if (maxObj <= index)
		throw std::out_of_range("out of range");
#endif

	size_t startBit = index * lengthOfOne;
	size_t endBit = (startBit + lengthOfOne) - 1;
	if ((startBit / numBits<uint64_t>()) != (endBit / numBits<uint64_t>())) {
		uint64_t lowByte = swap_endian(arr[startBit / numBits<uint64_t>()]);
		uint64_t upByte = swap_endian(arr[endBit / numBits<uint64_t>()]);

		size_t bitsLow = numBits<uint64_t>() - (startBit % numBits<uint64_t>()); //((index + 1) * lengthOfOne) - startBit - 1;
		size_t bitsUp = lengthOfOne - bitsLow;

		const uint64_t mask = ~(~static_cast<uint64_t>(0) << bitsUp);

		upByte &= mask;
		upByte = upByte << bitsLow;

		lowByte = lowByte >> (numBits<uint64_t>() - bitsLow);

		return static_cast<size_t>(upByte | lowByte);
	}
	else {
		//on same index in arr
		const uint64_t norm = arr[startBit / numBits<uint64_t>()];
		uint64_t val = swap_endian(norm);
		const auto m = startBit & (~0x3F); //((startBit / numBits<uint64_t>()) * numBits<uint64_t>());

		val >>= (startBit - m);
		const uint64_t mask = ~(~static_cast<uint64_t>(0) << lengthOfOne);
		val &= mask;
		return static_cast<size_t>(val);
	}

}

//Fuctions to determinate certain blocks
bool inRange(const SpecialBlocks blockType, const StateID_t bID)
{
	const auto& ranges = Global::specialBlockMap[blockType];
	for (const auto range : ranges) {
		if (bID >= range.first && bID <= range.second) {
			return true;
		}
	}

	return false;
}
