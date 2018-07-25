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
	return std::stoi(val);
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
	return std::regex_match(str, std::regex("[(-|+)|][0-9]+"));
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

bool inRange(const uint16_t bID, uint16_t lowLim, uint16_t upLim)
{
	return bID >= lowLim && bID <= upLim;
}

bool isWater(const uint16_t bID)
{
	return inRange(bID, 34, 49);
}

bool isLava(const uint16_t bID)
{
	return inRange(bID, 50, 65);
}

bool isGrass(const uint16_t bID)
{
	return inRange(bID, 8, 9);
}

bool isLeave(const uint16_t bID)
{
	return inRange(bID, 144, 227);
}

bool isSnow(const uint16_t bID)
{
	return inRange(bID, 3415, 3422) || bID == 3424;
}

bool isTorch(const uint16_t bID)
{
	return inRange(bID, 1130, 1134);
}

template <typename T, typename std::enable_if_t<std::is_integral<T>::value>* = nullptr>
T swap_endian(T u)
{
	static_assert (std::numeric_limits<unsigned char>::digits == 8, "CHAR_BIT != 8");

	union
	{
		T u;
		unsigned char u8[sizeof(T)];
	} source, dest;

	source.u = u;

	for (size_t k = 0; k < sizeof(T); k++)
		dest.u8[k] = source.u8[sizeof(T) - k - 1];

	return dest.u;
}

size_t getZahl(const std::vector<uint64_t>& arr, const size_t index, const size_t lengthOfOne) {
	const size_t maxObj = (arr.size() * numBits<uint64_t>()) / lengthOfOne;
	if (maxObj <= index)
		throw std::out_of_range("out of range");

	size_t startBit = index * lengthOfOne;
	size_t endBit = (startBit + lengthOfOne) - 1;
	if ((startBit / numBits<uint64_t>()) != (endBit / numBits<uint64_t>())) {
		uint64_t lowByte = swap_endian(arr[startBit / numBits<uint64_t>()]);
		uint64_t upByte = swap_endian(arr[endBit / numBits<uint64_t>()]);

		size_t bitsLow = numBits<uint64_t>() - (startBit % numBits<uint64_t>()); //((index + 1) * lengthOfOne) - startBit - 1;
		size_t bitsUp = lengthOfOne - bitsLow;

		upByte &= ~(~0 << bitsUp);
		upByte = upByte << bitsLow;

		lowByte = lowByte >> (numBits<uint64_t>() - bitsLow);

		assert((upByte|lowByte)==(upByte+lowByte));
		return static_cast<size_t>(upByte | lowByte);
	}
	else {
		//on same index in arr
		const uint64_t norm = arr[startBit / numBits<uint64_t>()];
		uint64_t val = swap_endian(norm);
		const auto m = ((startBit / numBits<uint64_t>()) * numBits<uint64_t>());
		val = val >> (startBit - m);
		val &= ~(~0 << lengthOfOne);
		return static_cast<size_t>(val);
	}

}
