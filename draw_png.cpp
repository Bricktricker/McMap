/**
 * This file contains functions to draw the world in to an image buffer
 */
#include <cstring> //memcpy (for g++)

#include "draw_png.h"
#include "colors.h"
#include "globals.h"


//#define PIXEL(x,y) (gImageBuffer[((x) + gOffsetX) * CHANSPERPIXEL + ((y) + gOffsetY) * gPngLocalLineWidthChans])
#define CHANSPERPIXEL PNGWriter::CHANSPERPIXEL
namespace
{

	inline void assignBiome(uint8_t* const color, const uint8_t biome, const uint16_t block);
	inline void blend(uint8_t* const destination, const Color_t& source); //Blend color to pixel
	//inline void blend(uint8_t* const destination, const uint8_t* const source); //Blend to pixel
	Color_t modColor(const Color_t& color, const int mod);
	inline void modColor(uint8_t* const pos, const int mod);
	inline void setColor(uint8_t* const pos, const Color_t& color);

	// Split them up so setPixel won't be one hell of a mess
	void setSnow(const size_t x, const size_t y, const Color_t& color, PNGWriter* pngWriter);
	void setTorch(const size_t x, const size_t y, const Color_t& color, PNGWriter* pngWriter);
	void setFlower(const size_t x, const size_t y, const Color_t& color, PNGWriter* pngWriter);
	void setRedwire(const size_t x, const size_t y, const Color_t& color, PNGWriter* pngWriter);
	void setFire(const size_t x, const size_t y, const Color_t& color, const Color_t& light, const Color_t& dark, PNGWriter* pngWriter);
	void setGrass(const size_t x, const size_t y, const Color_t& color, const Color_t& light, const Color_t& dark, const int sub, PNGWriter* pngWriter);
	void setFence(const size_t x, const size_t y, const Color_t& color, PNGWriter* pngWriter);
	void setStep(const size_t x, const size_t y, const Color_t& color, const Color_t& light, const Color_t& dark, PNGWriter* pngWriter);
	void setUpStep(const size_t x, const size_t y, const Color_t& color, const Color_t& light, const Color_t& dark, PNGWriter* pngWriter);
# define setRailroad setSnowBA

	// Then make duplicate copies so it is one hell of a mess (BlendAll functions)
	// ..but hey, its for speeeeeeeeed!
	void setSnowBA(const size_t x, const size_t y, const Color_t& color, PNGWriter* pngWriter);
	void setTorchBA(const size_t x, const size_t y, const Color_t& color, PNGWriter* pngWriter);
	void setFlowerBA(const size_t x, const size_t y, const Color_t& color, PNGWriter* pngWriter);
	void setGrassBA(const size_t x, const size_t y, const Color_t& color, const Color_t& light, const Color_t& dark, const int sub, PNGWriter* pngWriter);
	void setStepBA(const size_t x, const size_t y, const Color_t& color, const Color_t& light, const Color_t& dark, PNGWriter* pngWriter);
	void setUpStepBA(const size_t x, const size_t y, const Color_t& color, const Color_t& light, const Color_t& dark, PNGWriter* pngWriter);
}

uint64_t calcImageSize(const int mapChunksX, const int mapChunksZ, const size_t mapHeight, int &pixelsX, int &pixelsY, const bool tight)
{
	pixelsX = (mapChunksX * CHUNKSIZE_X + mapChunksZ * CHUNKSIZE_Z) * 2 + (tight ? 3 : 10);
	pixelsY = (mapChunksX * CHUNKSIZE_X + mapChunksZ * CHUNKSIZE_Z + int(mapHeight) * Global::OffsetY) + (tight ? 3 : 10);
	return uint64_t(pixelsX) * PNGWriter::BYTESPERPIXEL * uint64_t(pixelsY);
}

/*
fsub: brightnessAdjustment
*/
void setPixel(const size_t x, const size_t y, const uint16_t stateID, const float fsub, PNGWriter* pngWriter)
{
	// Sets pixels around x,y where A is the anchor
	// T = given color, D = darker, L = lighter
	// A T T T
	// D D L L
	// D D L L
	//	  D L
	// First determine how much the color has to be lightened up or darkened
	Color_t currentColor = colorMap[stateID];
	int sub = static_cast<int>(fsub * (static_cast<float>(currentColor.brightness) / 323.0f + 0.21f));  // The brighter the color, the stronger the impact
	//uint8_t L[CHANSPERPIXEL], D[CHANSPERPIXEL], c[CHANSPERPIXEL];
	Color_t L, D;
	currentColor = modColor(currentColor, sub); //set brightness

	//if (g_UseBiomes && g_WorldFormat == 2) assignBiome(c, biome, color); //dropped biom support
	const uint8_t colortype = currentColor.blockType % BLOCKBIOME;

	if (Global::settings.blendAll) {
		// Then check the block type, as some types will be drawn differently
		switch (colortype)
		{
		case BLOCKFLAT:
			setSnowBA(x, y, currentColor, pngWriter);
			return;
		case BLOCKTORCH:
			setTorchBA(x, y, currentColor, pngWriter);
			return;
		case BLOCKFLOWER:
			setFlowerBA(x, y, currentColor, pngWriter);
			return;
		case BLOCKFENCE:
			setFence(x, y, currentColor, pngWriter);
			return;
		case BLOCKWIRE:
			setRedwire(x, y, currentColor, pngWriter);
			return;
		case BLOCKRAILROAD:
			setRailroad(x, y, currentColor, pngWriter);
			return;
		}
		// All the above blocks didn't need the shaded down versions of the color, so we only calc them here
		// They are for the sides of blocks
		L = modColor(currentColor, -17);
		D = modColor(currentColor, -27);

		// A few more blocks with special handling... Those need the two colors we just mixed
		switch (colortype)
		{
		case BLOCKGRASS:
			if (!Global::settings.connGrass) {
				setGrassBA(x, y, currentColor, L, D, sub, pngWriter);
				return;
			}
			break;
		case BLOCKFIRE:
			setFire(x, y, currentColor, L, D, pngWriter);
			return;
		case BLOCKSTEP:
			setStepBA(x, y, currentColor, L, D, pngWriter);
			return;
		case BLOCKUPSTEP:
			setUpStepBA(x, y, currentColor, L, D, pngWriter);
			return;
		}
	} else {
		// Then check the block type, as some types will be drawn differently
		switch (colortype)
		{
		case BLOCKFLAT:
			setSnow(x, y, currentColor, pngWriter);
			return;
		case BLOCKTORCH:
			setTorch(x, y, currentColor, pngWriter);
			return;
		case BLOCKFLOWER:
			setFlower(x, y, currentColor, pngWriter);
			return;
		case BLOCKFENCE:
			setFence(x, y, currentColor, pngWriter);
			return;
		case BLOCKWIRE:
			setRedwire(x, y, currentColor, pngWriter);
			return;
		case BLOCKRAILROAD:
			setRailroad(x, y, currentColor, pngWriter);
			return;
		}
		// All the above blocks didn't need the shaded down versions of the color, so we only calc them here
		// They are for the sides of blocks
		L = modColor(currentColor, -17);
		D = modColor(currentColor, -27);
		// A few more blocks with special handling... Those need the two colors we just mixed
		switch (colortype)
		{
		case BLOCKGRASS:
			if (!Global::settings.connGrass) {
				setGrass(x, y, currentColor, L, D, sub, pngWriter);
				return;
			}
			break;
		case BLOCKFIRE:
			setFire(x, y, currentColor, L, D, pngWriter);
			return;
		case BLOCKSTEP:
			setStep(x, y, currentColor, L, D, pngWriter);
			return;
		case BLOCKUPSTEP:
			setUpStep(x, y, currentColor, L, D, pngWriter);
			return;
		}
	}
	// In case the user wants noise, calc the strength now, depending on the desired intensity and the block's brightness
	int noise = 0;
	if (Global::settings.noise && currentColor.noise) {
		noise = static_cast<int>(static_cast<float>(Global::settings.noise * currentColor.noise) * (static_cast<float>(currentColor.brightness + 10) / 2650.0f));
		//noise = int(float(Global::settings.noise * colors[color][NOISE]) * (float(GETBRIGHTNESS(c) + 10) / 2650.0f));
	}
	// Ordinary blocks are all rendered the same way
	if (currentColor.a == 255) { // Fully opaque - faster
		// Top row
		uint8_t *pos = pngWriter->getPixel(x, y); //&PIXEL(x, y);
		for (size_t i = 0; i < 4; ++i, pos += CHANSPERPIXEL) {
			setColor(pos, currentColor);
			if (noise) {
				modColor(pos, rand() % (noise * 2) - noise);
			}
		}
		// Second row (going down)
		pos = pngWriter->getPixel(x, y+1);
		for (size_t i = 0; i < 4; ++i, pos += CHANSPERPIXEL) {
			setColor(pos, (i < 2 ? D : L));
			// The weird check here is to get the pattern right, as the noise should be stronger
			// every other row, but take into account the isometric perspective
			if (noise) {
				modColor(pos, rand() % (noise * 2) - noise * (i == 0 || i == 3 ? 1 : 2));
			}
		}
		// Third row (going down)
		pos = pngWriter->getPixel(x, y + 2);
		for (size_t i = 0; i < 4; ++i, pos += CHANSPERPIXEL) {
			setColor(pos, (i < 2 ? D : L));
			if (noise) {
				modColor(pos, rand() % (noise * 2) - noise * (i == 0 || i == 3 ? 2 : 1));
			}
		}
		// Last row (going down)
		pos = pngWriter->getPixel(x, y + 3);
		for (size_t i = 0; i < 4; ++i, pos += CHANSPERPIXEL) {
			setColor(pos, (i < 2 ? D : L));
			// The weird check here is to get the pattern right, as the noise should be stronger
			// every other row, but take into account the isometric perspective
			if (noise) {
				modColor(pos, rand() % (noise * 2) - noise * (i == 0 || i == 3 ? 1 : 2));
			}
		}
	} else { // Not opaque, use slower blending code
		// Top row
		uint8_t *pos = pngWriter->getPixel(x, y);
		for (size_t i = 0; i < 4; ++i, pos += CHANSPERPIXEL) {
			blend(pos, currentColor);
			if (noise) {
				modColor(pos, rand() % (noise * 2) - noise);
			}
		}
		// Second row
		pos = pngWriter->getPixel(x, y + 1);
		for (size_t i = 0; i < 4; ++i, pos += CHANSPERPIXEL) {
			blend(pos, (i < 2 ? D : L));
			if (noise) {
				modColor(pos, rand() % (noise * 2) - noise * (i == 0 || i == 3 ? 1 : 2));
			}
		}
		// Third row
		pos = pngWriter->getPixel(x, y + 2);
		for (size_t i = 0; i < 4; ++i, pos += CHANSPERPIXEL) {
			blend(pos, (i < 2 ? D : L));
			if (noise) {
				modColor(pos, rand() % (noise * 2) - noise * (i == 0 || i == 3 ? 2 : 1));
			}
		}
		// Last row
		pos = pngWriter->getPixel(x, y + 3);
		for (size_t i = 0; i < 4; ++i, pos += CHANSPERPIXEL) {
			blend(pos, (i < 2 ? D : L));
			if (noise) {
				modColor(pos, rand() % (noise * 2) - noise * (i == 0 || i == 3 ? 1 : 2));
			}
		}
	}

}

void blendPixel(const size_t x, const size_t y, const uint16_t stateID, const float fsub, PNGWriter* pngWriter)
{
	// This one is used for cave overlay
	// Sets pixels around x,y where A is the anchor
	// T = given color, D = darker, L = lighter
	// A T T T
	// D D L L
	// D D L L
	//	  D L
	//uint8_t L[CHANSPERPIXEL], D[CHANSPERPIXEL], c[CHANSPERPIXEL];
	Color_t L, D;
	Color_t currentColor = colorMap[stateID];
	// Now make a local copy of the color that we can modify just for this one block
	//c[PALPHA] = clamp(int(float(c[PALPHA]) * fsub)); // The brighter the color, the stronger the impact
	currentColor.a = clamp(static_cast<int>(static_cast<float>(currentColor.a) * fsub));
	// They are for the sides of blocks
	L = modColor(currentColor, -17);
	D = modColor(currentColor, -27);
	// In case the user wants noise, calc the strength now, depending on the desired intensity and the block's brightness
	int noise = 0;
	if (Global::settings.noise && currentColor.noise) {
		//noise = int(float(Global::settings.noise * colors[color][NOISE]) * (float(GETBRIGHTNESS(c) + 10) / 2650.0f));
		noise = static_cast<int>(static_cast<float>(currentColor.noise * Global::settings.noise) * (static_cast<float>(currentColor.brightness + 10) / 2650.0f));
	}
	// Top row
	uint8_t *pos = pngWriter->getPixel(x, y);
	for (size_t i = 0; i < 4; ++i, pos += CHANSPERPIXEL) {
		blend(pos, currentColor);
		if (noise) {
			modColor(pos, rand() % (noise * 2) - noise);
		}
	}
	// Second row
	pos = pngWriter->getPixel(x, y+1);
	for (size_t i = 0; i < 4; ++i, pos += CHANSPERPIXEL) {
		blend(pos, (i < 2 ? D : L));
		if (noise) {
			modColor(pos, rand() % (noise * 2) - noise * (i == 0 || i == 3 ? 1 : 2));
		}
	}
}

void blend(uint8_t* const destination, const uint8_t* const source)
{
#define PALPHA 3
	if (destination[PALPHA] == 0 || source[PALPHA] == 255) { //compare alpha
		std::memcpy(destination, source, PNGWriter::BYTESPERPIXEL);
		return;
	}
#define BLEND(ca,aa,cb) uint8_t(((size_t(ca) * size_t(aa)) + (size_t(255 - aa) * size_t(cb))) / 255)
	destination[0] = BLEND(source[0], source[PALPHA], destination[0]);
	destination[1] = BLEND(source[1], source[PALPHA], destination[1]);
	destination[2] = BLEND(source[2], source[PALPHA], destination[2]);
	destination[PALPHA] += (size_t(source[PALPHA]) * size_t(255 - destination[PALPHA])) / 255;
}

namespace
{

	inline void assignBiome(uint8_t* const color, const uint8_t biome, const uint16_t block)
	{
		//do there what you want, this code response for changing single pixel depending on its biome
		//note this works only for anvli format. old one still requires donkey kong biome extractor

		//if (colorMap[block].blockType / BLOCKBIOME)
			//addColor(color, biomes[biome]);
		//dropped biom support
	}

	inline void blend(uint8_t* const destination, const Color_t& source)
	{
		if (destination[PALPHA] == 0 || source.a == 255) { //compare alpha
			destination[0] = source.r;
			destination[1] = source.g;
			destination[2] = source.b;
			destination[3] = source.a;
			return;
		}
#define BLEND(ca,aa,cb) uint8_t(((size_t(ca) * size_t(aa)) + (size_t(255 - aa) * size_t(cb))) / 255)
		destination[0] = BLEND(source.r, source.a, destination[0]);
		destination[1] = BLEND(source.g, source.a, destination[1]);
		destination[2] = BLEND(source.b, source.a, destination[2]);
		destination[PALPHA] += (size_t(source.a) * size_t(255 - destination[PALPHA])) / 255;
	}

	Color_t modColor(const Color_t& color, const int mod)
	{
		Color_t retCol = color;
		retCol.r = clamp(color.r + mod);
		retCol.g = clamp(color.g + mod);
		retCol.b = clamp(color.b + mod);
		return retCol;
	}

	void modColor(uint8_t* const pos, const int mod)
	{
		pos[0] = clamp(pos[0] + mod);
		pos[1] = clamp(pos[1] + mod);
		pos[2] = clamp(pos[2] + mod);
	}

	void setColor(uint8_t* const pos, const Color_t & color)
	{
		pos[0] = color.r;
		pos[1] = color.g;
		pos[2] = color.b;
		pos[3] = color.a;
	}

	//setzt color an pos + 3 weitere
	void setSnow(const size_t x, const size_t y, const Color_t& color, PNGWriter* pngWriter)
	{
		// Top row (second row)
		uint8_t* pos = pngWriter->getPixel(x,y+1);
		for (size_t i = 0; i < 4; ++i, pos += CHANSPERPIXEL) {
			pos[0] = color.r;
			pos[1] = color.g;
			pos[2] = color.b;
			pos[3] = color.a;
		}
	}

	void setTorch(const size_t x, const size_t y, const Color_t& color, PNGWriter* pngWriter)
	{
		// Maybe the orientation should be considered when drawing, but it probably isn't worth the efford
		uint8_t *pos = pngWriter->getPixel(x+2,y+1);
		pos[0] = color.r;
		pos[1] = color.g;
		pos[2] = color.b;
		pos[3] = color.a;

		pos = pngWriter->getPixel(x + 2, y + 2);
		pos[0] = color.r;
		pos[1] = color.g;
		pos[2] = color.b;
		pos[3] = color.a;
	}

	void setFlower(const size_t x, const size_t y, const Color_t& color, PNGWriter* pngWriter)
	{
		uint8_t *pos = pngWriter->getPixel(x, y + 1);
		setColor(pos + (CHANSPERPIXEL), color);
		setColor(pos + (CHANSPERPIXEL*3), color);

		pos = pngWriter->getPixel(x + 2, y + 2);
		setColor(pos, color);

		pos = pngWriter->getPixel(x + 1, y + 3);
		setColor(pos, color);
	}

	void setFire(const size_t x, const size_t y, const Color_t& color, const Color_t& light, const Color_t& dark, PNGWriter* pngWriter)
	{
		// This basically just leaves out a few pixels
		// Top row
		uint8_t *pos = pngWriter->getPixel(x, y);
		blend(pos, light);
		blend(pos + CHANSPERPIXEL*2, dark);
		// Second and third row
		for (size_t i = 1; i < 3; ++i) {
			pos = pngWriter->getPixel(x, y + i);
			blend(pos, dark);
			blend(pos+(CHANSPERPIXEL*i), color);
			blend(pos+(CHANSPERPIXEL*3), light);
		}
		// Last row
		pos = pngWriter->getPixel(x, y + 3);
		blend(pos+(CHANSPERPIXEL*2), light);
	}

	void setGrass(const size_t x, const size_t y, const Color_t& color, const Color_t& light, const Color_t& dark, const int sub, PNGWriter* pngWriter)
	{
		// this will make grass look like dirt from the side
		Color_t L = colorMap[blockTree["minecraft:dirt"].get()];
		Color_t D = L;
		L = modColor(L, sub - 15);
		D = modColor(D, sub - 25);
		// consider noise
		int noise = 0;
		if (Global::settings.noise && color.noise) {
			//noise = int(float(Global::settings.noise * colors[GRASS][NOISE]) * (float(GETBRIGHTNESS(color) + 10) / 2650.0f));
			noise = static_cast<int>(static_cast<float>(Global::settings.noise * color.noise) * (static_cast<float>(color.brightness + 10) / 2650.0f));
		}
		// Top row
		uint8_t *pos = pngWriter->getPixel(x, y);
		for (size_t i = 0; i < 4; ++i, pos += CHANSPERPIXEL) {
			setColor(pos, color);
			if (noise) {
				modColor(pos, rand() % (noise * 2) - noise);
			}
		}
		// Second row
		pos = pngWriter->getPixel(x, y+1);
		setColor(pos, dark);
		setColor(pos+CHANSPERPIXEL, dark);
		setColor(pos+CHANSPERPIXEL*2, light);
		setColor(pos+CHANSPERPIXEL*3, light);

		// Third row
		pos = pngWriter->getPixel(x, y+2);
		setColor(pos, D);
		setColor(pos+CHANSPERPIXEL, D);
		setColor(pos+CHANSPERPIXEL*2, L);
		setColor(pos+CHANSPERPIXEL*3, L);

		// Last row
		pos = pngWriter->getPixel(x, y+3);
		setColor(pos, D);
		setColor(pos + CHANSPERPIXEL, D);
		setColor(pos + CHANSPERPIXEL * 2, L);
		setColor(pos + CHANSPERPIXEL * 3, L);
	}

	void setFence(const size_t x, const size_t y, const Color_t& color, PNGWriter* pngWriter)
	{
		// First row
		uint8_t *pos = pngWriter->getPixel(x, y);
		blend(pos+CHANSPERPIXEL, color);
		blend(pos+CHANSPERPIXEL*2, color);
		// Second row
		pos = pngWriter->getPixel(x+1, y+1);
		blend(pos, color);
		// Third row
		pos = pngWriter->getPixel(x, y+2);
		blend(pos+CHANSPERPIXEL, color);
		blend(pos+CHANSPERPIXEL*2, color);
	}

	void setStep(const size_t x, const size_t y, const Color_t& color, const Color_t& light, const Color_t& dark, PNGWriter* pngWriter)
	{
		uint8_t *pos = pngWriter->getPixel(x, y+1);
		for (size_t i = 0; i < 4; ++i, pos += CHANSPERPIXEL) {
			setColor(pos, color);
		}
		pos = pngWriter->getPixel(x, y+2);
		for (size_t i = 0; i < 4; ++i, pos += CHANSPERPIXEL) {
			setColor(pos, color);
		}
	}

	void setUpStep(const size_t x, const size_t y, const Color_t& color, const Color_t& light, const Color_t& dark, PNGWriter* pngWriter)
	{
		uint8_t *pos = pngWriter->getPixel(x, y);
		for (size_t i = 0; i < 4; ++i, pos += CHANSPERPIXEL) {
			setColor(pos, color);
		}
		pos = pngWriter->getPixel(x, y+1);
		for (size_t i = 0; i < 4; ++i, pos += CHANSPERPIXEL) {
			setColor(pos, color);
		}
	}

	void setRedwire(const size_t x, const size_t y, const Color_t& color, PNGWriter* pngWriter)
	{
		uint8_t *pos = pngWriter->getPixel(x+1, y+2);
		blend(pos, color);
		blend(pos+CHANSPERPIXEL, color);
	}

	// The g_BlendAll versions of the block set functions
	//
	void setSnowBA(const size_t x, const size_t y, const Color_t& color, PNGWriter* pngWriter)
	{
		// Top row (second row)
		uint8_t *pos = pngWriter->getPixel(x, y+1);
		for (size_t i = 0; i < 4; ++i, pos += CHANSPERPIXEL) {
			blend(pos, color);
		}
	}

	void setTorchBA(const size_t x, const size_t y, const Color_t& color, PNGWriter* pngWriter)
	{
		// Maybe the orientation should be considered when drawing, but it probably isn't worth the effort
		uint8_t *pos = pngWriter->getPixel(x+2, y+1);
		blend(pos, color);
		pos = pngWriter->getPixel(x+2, y+2);
		blend(pos, color);
	}

	void setFlowerBA(const size_t x, const size_t y, const Color_t& color, PNGWriter* pngWriter)
	{
		uint8_t *pos = pngWriter->getPixel(x, y+1);
		blend(pos+CHANSPERPIXEL, color);
		blend(pos+CHANSPERPIXEL*3, color);
		pos = pngWriter->getPixel(x+2, y+2);
		blend(pos, color);
		pos = pngWriter->getPixel(x+1, y+3);
		blend(pos, color);
	}

	void setGrassBA(const size_t x, const size_t y, const Color_t& color, const Color_t& light, const Color_t& dark, const int sub, PNGWriter* pngWriter)
	{
		// this will make grass look like dirt from the side
		Color_t L = colorMap[blockTree["minecraft:dirt"].get()];
		Color_t D = L;
		L = modColor(L, sub - 15);
		D = modColor(D, sub - 25);
		// consider noise
		int noise = 0;
		if (Global::settings.noise && color.noise) {
			//noise = int(float(Global::settings.noise * colors[GRASS][NOISE]) * (float(GETBRIGHTNESS(color) + 10) / 2650.0f));
			noise = static_cast<int>(static_cast<float>(Global::settings.noise * color.noise) * (static_cast<float>(color.brightness + 10) / 2650.0f));
		}
		// Top row
		uint8_t *pos = pngWriter->getPixel(x, y);
		for (size_t i = 0; i < 4; ++i, pos += CHANSPERPIXEL) {
			blend(pos, color);
			if (noise) {
				modColor(pos, rand() % (noise * 2) - noise);
			}
		}
		// Second row
		pos = pngWriter->getPixel(x, y+1);
		blend(pos, dark);
		blend(pos+CHANSPERPIXEL, dark);
		blend(pos+CHANSPERPIXEL*2, light);
		blend(pos+CHANSPERPIXEL*3, light);
		// Third row
		pos = pngWriter->getPixel(x, y+2);
		blend(pos, D);
		blend(pos+CHANSPERPIXEL, D);
		blend(pos+CHANSPERPIXEL*2, L);
		blend(pos+CHANSPERPIXEL*3, L);
		// Last row
		pos = pngWriter->getPixel(x, y+3);
		blend(pos, D);
		blend(pos+CHANSPERPIXEL, D);
		blend(pos+CHANSPERPIXEL*2, L);
		blend(pos+CHANSPERPIXEL*3, L);
	}

	void setStepBA(const size_t x, const size_t y, const Color_t& color, const Color_t& light, const Color_t& dark, PNGWriter* pngWriter)
	{
		uint8_t *pos = pngWriter->getPixel(x, y+1);
		for (size_t i = 0; i < 3; ++i, pos += CHANSPERPIXEL) {
			blend(pos, color);
		}
		pos = pngWriter->getPixel(x, y+2);
		for (size_t i = 0; i < 10; ++i, pos += CHANSPERPIXEL) {
			blend(pos, color);
		}
	}

	void setUpStepBA(const size_t x, const size_t y, const Color_t& color, const Color_t& light, const Color_t& dark, PNGWriter* pngWriter)
	{
		uint8_t *pos = pngWriter->getPixel(x, y);
		for (size_t i = 0; i < 3; ++i, pos += CHANSPERPIXEL) {
			blend(pos, color);
		}
		pos = pngWriter->getPixel(x, y+1);
		for (size_t i = 0; i < 10; ++i, pos += CHANSPERPIXEL) {
			blend(pos, color);
		}
	}
}
