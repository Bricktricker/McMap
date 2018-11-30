#ifndef _HELPER_H_
#define _HELPER_H_

// Just in case these ever change
#define CHUNKSIZE_Z 16
#define CHUNKSIZE_X 16
#define CHUNKSIZE_Y 256 //old 128
#define SECTION_Y 16
#define SECTION_Y_SHIFT 4
// Some macros for easier array access
// First: Block array
#define BLOCKAT(x,y,z) Global::terrain[(y) + ((z) + ((x) * Global::MapsizeZ)) * Global::MapsizeY] //Sortierung: Erst Y, dann Z, dann X
//#define BLOCKDATA(x,y,z) Global::terrain[(y) + ((z) + ((x) * Global::MapsizeZ)) * Global::MapsizeY + Global::Terrainsize]
#define BLOCKEAST(x,y,z) Global::terrain[(y) + ((Global::MapsizeZ - ((x) + 1)) + ((z) * Global::MapsizeZ)) * Global::MapsizeY]
#define BLOCKWEST(x,y,z) Global::terrain[(y) + ((x) + ((Global::MapsizeX - ((z) + 1)) * Global::MapsizeZ)) * Global::MapsizeY]
#define BLOCKNORTH(x,y,z) Global::terrain[(y) + ((z) + ((x) * Global::MapsizeZ)) * Global::MapsizeY]
#define BLOCKSOUTH(x,y,z) Global::terrain[(y) + ((Global::MapsizeZ - ((z) + 1)) + ((Global::MapsizeX - ((x) + 1)) * Global::MapsizeZ)) * Global::MapsizeY]
//#define BLOCKAT(x,y,z) Global::terrain[(x) + ((z) + ((y) * Global::MapsizeZ)) * Global::MapsizeX]
//#define BLOCKEAST(x,y,z) Global::terrain[(z) + ((Global::MapsizeZ - ((x) + 1)) + ((y) * Global::MapsizeZ)) * Global::MapsizeX]
// Same for lightmap
#define GETLIGHTAT(x,y,z) ((Global::light[((y) / 2) + ((z) + ((x) * Global::MapsizeZ)) * ((Global::MapsizeY + 1) / 2)] >> (((y) % 2) * 4)) & 0xF)
#define SETLIGHTEAST(x,y,z) Global::light[((y) / 2) + ((Global::MapsizeZ - ((x) + 1)) + ((z) * Global::MapsizeZ)) * ((Global::MapsizeY + 1) / 2)]
#define SETLIGHTWEST(x,y,z) Global::light[((y) / 2) + ((x) + ((Global::MapsizeX - ((z) + 1)) * Global::MapsizeZ)) * ((Global::MapsizeY + 1) / 2)]
#define SETLIGHTNORTH(x,y,z) Global::light[((y) / 2) + ((z) + ((x) * Global::MapsizeZ)) * ((Global::MapsizeY + 1) / 2)]
#define SETLIGHTSOUTH(x,y,z) Global::light[((y) / 2) + ((Global::MapsizeZ - ((z) + 1)) + ((Global::MapsizeX - ((x) + 1)) * Global::MapsizeZ)) * ((Global::MapsizeY + 1) / 2)]
// Biome array
#define BIOMEAT(x,z) Global::biomeMap[(z) + ((x) * Global::MapsizeZ)]
#define BIOMEEAST(x,z) Global::biomeMap[(Global::MapsizeZ - ((x) + 1)) + ((z) * Global::MapsizeZ)]
#define BIOMEWEST(x,z) Global::biomeMap[(x) + ((Global::MapsizeX - ((z) + 1)) * Global::MapsizeZ)]
#define BIOMENORTH(x,z) Global::biomeMap[(z) + ((x) * Global::MapsizeZ)]
#define BIOMESOUTH(x,z) Global::biomeMap[(Global::MapsizeZ - ((z) + 1)) + ((Global::MapsizeX - ((x) + 1)) * Global::MapsizeZ)]
// Heightmap array
#define HEIGHTAT(x,z) Global::heightMap[(z) + ((x) * Global::MapsizeZ)]

#include <string>
#include <vector>
#include <iterator>
#include <sstream>
#include <limits> // needed for g++

// Difference between MSVC++ and gcc/others
#if defined(_WIN32) && !defined(__GNUC__)
	#define MSVCP
#else
#	include <unistd.h>
#endif

uint8_t clamp(int32_t val);
void printProgress(const size_t current, const size_t max);
bool isNumeric(const std::string& str);
bool isAlphaWorld(const std::string& path);
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
T swap_endian(const T u)
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

//Optimization for Windows
#ifdef MSVCP
#include <intrin.h>

template <>
inline uint64_t swap_endian<uint64_t>(const uint64_t u)
{
	static_assert (std::numeric_limits<unsigned char>::digits == 8, "CHAR_BIT != 8");
	static_assert ('ABCD' == 0x41424344UL, "Endiness not supported");
	return _byteswap_uint64(u);
}

template <>
inline uint32_t swap_endian<uint32_t>(const uint32_t u)
{
	static_assert (std::numeric_limits<unsigned char>::digits == 8, "CHAR_BIT != 8");
	static_assert ('ABCD' == 0x41424344UL, "Endiness not supported");
	return _byteswap_ulong(u);
}
#endif

size_t getVal(const std::vector<uint64_t>& arr, const size_t index, const size_t lengthOfOne);
std::vector<std::string> strSplit(const std::string &s, char delim);

//Fuctions to determinate certain blocks
//TODO: move into BlockIDs.json file
inline constexpr bool inRange(const uint16_t bID, uint16_t lowLim, uint16_t upLim)
{
	return bID >= lowLim && bID <= upLim;
}

inline bool isGrass(const uint16_t bID)
{
	return inRange(bID, 8, 9);
}

inline bool isWater(const uint16_t bID)
{
	return inRange(bID, 34, 49);
}

inline bool isLava(const uint16_t bID)
{
	return inRange(bID, 50, 65);
}

inline bool isLeave(const uint16_t bID)
{
	return inRange(bID, 144, 227);
}

inline bool isTorch(const uint16_t bID)
{
	return inRange(bID, 1131, 1135);
}

inline bool isSnow(const uint16_t bID)
{
	return inRange(bID, 3416, 3423) || bID == 3425;
}


#endif
