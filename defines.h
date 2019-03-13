#ifndef _DEFINES_
#define _DEFINES_
#include <cstdint>

//Data structures for color
using Channel = uint8_t;
struct Pixel
{
	Channel r, g, b, a;
};

static_assert(sizeof(Channel) * 4 == sizeof(Pixel), "Ups!");

struct Color_t {
	Channel r, g, b, a;
	uint8_t noise, brightness, blockType;

	Color_t()
		: r(0), g(0), b(0), a(0), noise(0), brightness(0), blockType(0)
	{}

	Color_t(const Channel _r, const Channel _g, const Channel _b, const Channel _a, const Channel _n, const Channel _bright, const Channel _bType)
		: r(_r), g(_g), b(_b), a(_a), noise(_n), brightness(_bright), blockType(_bType)
	{}

};

using StateID_t = uint16_t;

#endif // !_DEFINES_
