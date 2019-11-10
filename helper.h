#ifndef _HELPER_H_
#define _HELPER_H_

#include <string>
#include <vector>
#include <iterator>
#include <sstream>
#include <limits> // needed for g++
#include "defines.h"
#include "globals.h" //SpecialBlocks

// Difference between MSVC++ and gcc/others
#if defined(_WIN32) && !defined(__GNUC__)
	#define MSVCP
#else
#	include <unistd.h>
#endif

namespace helper {
	uint8_t clamp(int32_t val);
	void printProgress(const size_t current, const size_t max);
	bool isNumeric(const std::string& str);
	bool isWorld(const std::string& path);
	bool strEndsWith(std::string const &fullString, std::string const &ending);

	template<typename Out>
	void strSplit(const std::string &s, char delim, Out result) {
		std::stringstream ss(s);
		std::string item;
		while (std::getline(ss, item, delim)) {
			*(result++) = item;
		}
	}

	template<typename T>
	constexpr size_t numBits() {
		return sizeof(T) * 8;
	}

	/*
	 Converts big endian values to host-architecture
	*/
	template <typename T> //, typename std::enable_if_t<std::is_integral<T>::value>* = nullptr
	T swap_endian(const T u){
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

	//Optimization for Windows
	#ifdef MSVCP
	#include <intrin.h>

	template <>
	inline uint64_t swap_endian<uint64_t>(const uint64_t u){
		static_assert (std::numeric_limits<unsigned char>::digits == 8, "CHAR_BIT != 8");
		static_assert ('ABCD' == 0x41424344UL, "Endiness not supported");
		return _byteswap_uint64(u);
	}

	template <>
	inline uint32_t swap_endian<uint32_t>(const uint32_t u){
		static_assert (std::numeric_limits<unsigned char>::digits == 8, "CHAR_BIT != 8");
		static_assert ('ABCD' == 0x41424344UL, "Endiness not supported");
		return _byteswap_ulong(u);
	}
	#endif

	std::vector<std::string> strSplit(const std::string &s, char delim);

	//Fuctions to determinate certain blocks
	bool inRange(const SpecialBlocks blockType, const StateID_t bID);

	inline bool isGrass(const StateID_t bID){
		return inRange(SpecialBlocks::GRASS_BLOCK, bID);
	}

	inline bool isWater(const StateID_t bID){
		return inRange(SpecialBlocks::WATER, bID);
	}

	inline bool isLava(const StateID_t bID){
		return inRange(SpecialBlocks::LAVA, bID);
	}

	inline bool isLeave(const StateID_t bID){
		return inRange(SpecialBlocks::LEAVES, bID);
	}

	inline bool isTorch(const StateID_t bID){
		return inRange(SpecialBlocks::TORCH, bID);
	}

	inline bool isSnow(const StateID_t bID){
		return inRange(SpecialBlocks::SNOW, bID);
	}
}
#endif
