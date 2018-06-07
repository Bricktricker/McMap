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
#define BLOCKDATA(x,y,z) Global::terrain[(y) + ((z) + ((x) * Global::MapsizeZ)) * Global::MapsizeY + Global::Terrainsize]
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


#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) < (b) ? (a) : (b))
//#define RIGHTSTRING(x,y) ((x) + strlen(x) - ((y) > strlen(x) ? strlen(x) : (y)))
#define RIGHTSTRING(x,y) (strlen(x) >= (y) ? (x) + strlen(x) - (y) : (x))

#include <string>
#include <vector>
#include <iterator>
#include <sstream>

// Difference between MSVC++ and gcc/others
#if defined(_WIN32) && !defined(__GNUC__)
#	include <windows.h>
#	define usleep(x) Sleep((x) / 1000);
#else
#	include <unistd.h>
#endif

// For fseek
#if defined(_WIN32) && !defined(__GNUC__)
// MSVC++
#	define fseek64 _fseeki64
#elif defined(__APPLE__)
#	define fseek64 fseeko
#elif defined(__FreeBSD__)
#   define fseek64 fseeko
#else
#	define fseek64 fseeko64
#endif

// Differently named
#if defined(_WIN32) && !defined(__GNUC__)
#	define snprintf _snprintf
#  define mkdir _mkdir
#endif


// If this is missing for you in Visual Studio: See http://en.wikipedia.org/wiki/Stdint.h#External_links
#include <stdint.h>

std::string base36(int val);
int base10(const std::string& val);
uint8_t clamp(int32_t val);
void printProgress(const size_t current, const size_t max);
bool fileExists(const std::string& strFilename);
bool dirExists(const std::string& strFilename);
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

std::vector<std::string> strSplit(const std::string &s, char delim);
#endif
