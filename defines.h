#ifndef _DEFINES_
#define _DEFINES_
#include <cstdint>
#include <cassert>
#include <array>

// Just in case these ever change
#define CHUNKSIZE_Z 16
#define CHUNKSIZE_X 16
#define CHUNKSIZE_Y 256 //old 128
#define SECTION_Y 16
#define SECTION_Y_SHIFT 4
#define CHUNKS_PER_BIOME_FILE 32
#define REGIONSIZE 32
// Some macros for easier array access
// First: Block array
#define BLOCKAT(x,y,z) Global::terrain[(y) + ((z) + ((x) * Global::MapsizeZ)) * Global::MapsizeY] //Sortierung: Erst Y, dann Z, dann X
#define BLOCKEAST(x,y,z) Global::terrain[(y) + ((Global::MapsizeZ - ((x) + 1)) + ((z) * Global::MapsizeZ)) * Global::MapsizeY]
#define BLOCKWEST(x,y,z) Global::terrain[(y) + ((x) + ((Global::MapsizeX - ((z) + 1)) * Global::MapsizeZ)) * Global::MapsizeY]
#define BLOCKNORTH(x,y,z) Global::terrain[(y) + ((z) + ((x) * Global::MapsizeZ)) * Global::MapsizeY]
#define BLOCKSOUTH(x,y,z) Global::terrain[(y) + ((Global::MapsizeZ - ((z) + 1)) + ((Global::MapsizeX - ((x) + 1)) * Global::MapsizeZ)) * Global::MapsizeY]
// Same for lightmap
#define GETLIGHTAT(x,y,z) ((Global::light[((y) / 2) + ((z) + ((x) * Global::MapsizeZ)) * ((Global::MapsizeY + 1) / 2)] >> (((y) % 2) * 4)) & 0xF)
#define SETLIGHTEAST(x,y,z) Global::light[((y) / 2) + ((Global::MapsizeZ - ((x) + 1)) + ((z) * Global::MapsizeZ)) * ((Global::MapsizeY + 1) / 2)]
#define SETLIGHTWEST(x,y,z) Global::light[((y) / 2) + ((x) + ((Global::MapsizeX - ((z) + 1)) * Global::MapsizeZ)) * ((Global::MapsizeY + 1) / 2)]
#define SETLIGHTNORTH(x,y,z) Global::light[((y) / 2) + ((z) + ((x) * Global::MapsizeZ)) * ((Global::MapsizeY + 1) / 2)]
#define SETLIGHTSOUTH(x,y,z) Global::light[((y) / 2) + ((Global::MapsizeZ - ((z) + 1)) + ((Global::MapsizeX - ((x) + 1)) * Global::MapsizeZ)) * ((Global::MapsizeY + 1) / 2)]
// Heightmap array
#define HEIGHTAT(x,z) Global::heightMap[(z) + ((x) * Global::MapsizeZ)]

//determinate 64Bit or 32Bit
#if defined(_WIN32) || defined(_WIN64)
	#if defined(_WIN64)
		#define NUM_BITS 64
	#else
		#define NUM_BITS 32
	#endif
#elif defined __GNUC__
	#if defined(__x86_64__) || defined(__ppc64__)
		#define NUM_BITS 64
	#else
		#define NUM_BITS 32
	#endif
#else
	#error "Missing feature-test macro for 32/64-bit on this compiler."
#endif

#define UNDEFINED 0x7FFFFFFF //TDOD: remove it
#define VERSION "beta 3.0.6"
#define AIR 0

//Data structures for color
using Channel = uint8_t;
using StateID_t = uint16_t;
struct Pixel
{
	Channel r, g, b, a;
};

static_assert(sizeof(Channel) * 4 == sizeof(Pixel), "Ups!");

struct Color_t {
	Channel r, g, b, a;
	uint8_t noise, brightness;

	Color_t()
		: r(0), g(0), b(0), a(0), noise(0), brightness(0)
	{}

	Color_t(const Channel _r, const Channel _g, const Channel _b, const Channel _a, const uint8_t _n, const uint8_t _bright)
		: r(_r), g(_g), b(_b), a(_a), noise(_n), brightness(_bright)
	{}

};

struct ColorArray {
	std::array<Color_t, 2> colors;
	StateID_t length;

	ColorArray()
		: colors(), length(0)
	{}

	void addColor(const Color_t& color) {
		assert(length < colors.size());
		colors[length++] = color;
	}

	const Color_t& operator[](const size_t index) const {
		assert(index < length);
		return colors[index];
	}

};

struct Model_t {
	const uint64_t drawMode;
	const bool isSolidBlock;
	ColorArray colors;

	Model_t(const uint64_t _drawMode, const bool _isFullBlock, const ColorArray& _colors)
		: drawMode(_drawMode), isSolidBlock(_isFullBlock), colors(_colors)
	{}
};

#endif // !_DEFINES_
