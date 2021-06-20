/**
 * This file contains functions to draw the world in to an image buffer
 */
#include <cstring> //memcpy (for g++)

#include "draw_png.h"
#include "colors.h"
#include "helper.h"

#define CHANSPERPIXEL image::PNGWriter::CHANSPERPIXEL
namespace draw
{

	inline void blend(Channel* const destination, const Color_t& source); //Blend color to pixel
	//inline void blend(Channel* const destination, const Channel* const source); //Blend to pixel
	Color_t modColor(const Color_t& color, const int mod);
	inline void modColor(Channel* const pos, const int mod);
	inline void setColor(Channel* const pos, const Color_t& color);

	uint64_t calcImageSize(const size_t mapChunksX, const size_t mapChunksZ, const size_t mapHeight, size_t &pixelsX, size_t &pixelsY, const bool tight)
	{
		pixelsX = (mapChunksX * CHUNKSIZE_X + mapChunksZ * CHUNKSIZE_Z) * 2 + (tight ? 3 : 10);
		pixelsY = (mapChunksX * CHUNKSIZE_X + mapChunksZ * CHUNKSIZE_Z + mapHeight * static_cast<size_t>(Global::OffsetY)) + (tight ? 3 : 10);
		return pixelsX * image::PNGWriter::BYTESPERPIXEL * pixelsY;
	}

	/*
	fsub: brightnessAdjustment
	*/
	void setPixel(const int x, const int y, const StateID_t stateID, const float fsub, image::PNGWriter* pngWriter)
	{
		if (x < 0 || static_cast<size_t>(x) >= pngWriter->getWidth()) {
			return;
		}
		if (y < 0 || static_cast<size_t>(y) >= pngWriter->getHeight()) {
			return;
		}

		const Model_t& model = Global::colorMap[stateID];
		std::array<ColorArray, 3> colors; //array of colors, 0 are brightness modified colors, 1 are light colors, 2 are dark colors

		for (size_t i = 0; i < model.colors.length; i++) {
			const Color_t& orig = model.colors[i];
			const int sub = static_cast<int>(fsub * (static_cast<float>(orig.brightness) / 323.0f + 0.21f));  // The brighter the color, the stronger the impact
			const Color_t currentColor = modColor(orig, sub); //set brightness
			colors[0].addColor(currentColor);
			colors[1].addColor(modColor(currentColor, -17));
			colors[2].addColor(modColor(currentColor, -27));
		}

		/*
			Drawmode:
			00X - empty
			01X - orig color, X index
			10X - light color, X index
			11X - dark color, X index
		*/
		uint64_t drawMode = model.drawMode;
		for (size_t yPos = 0; yPos < 4; yPos++) {
			for (size_t xPos = 0; xPos < 4; xPos++) {
				const uint64_t pixelDrawMode = drawMode & 0b111;
				drawMode >>= 3;
				if ((pixelDrawMode & 0b110) == 0) {
					continue;
				}
				Channel* pos = pngWriter->getPixel(static_cast<size_t>(x) + xPos, static_cast<size_t>(y) + yPos);

				const size_t colorIdx = (((pixelDrawMode & 0b110) >> 1) - 1);
				const Color_t& pixelColor = colors[colorIdx][pixelDrawMode & 1];

				// In case the user wants noise, calc the strength now, depending on the desired intensity and the block's brightness
				int noise = 0;
				if (Global::settings.noise && pixelColor.noise) {
					noise = static_cast<int>(static_cast<float>(Global::settings.noise * pixelColor.noise) * (static_cast<float>(pixelColor.brightness + 10) / 2650.0f));
				}

				if (pixelColor.a == 255 && !Global::settings.blendAll) {
					setColor(pos, pixelColor);
				} else {
					blend(pos, pixelColor);
				}

				if (noise) {
					modColor(pos, rand() % (noise * 2) - noise);
				}
			}
		}

	}

	void blendPixel(const int x, const int y, const StateID_t stateID, const float fsub, image::PNGWriter* pngWriter)
	{
		if (x < 0 || static_cast<size_t>(x) >= pngWriter->getWidth()) {
			return;
		}
		if (y < 0 || static_cast<size_t>(y) >= pngWriter->getHeight()) {
			return;
		}

		const Model_t& model = Global::colorMap[stateID];
		std::array<ColorArray, 3> colors; //array of colors, 0 are brightness modified colors, 1 are light colors, 2 are dark colors

		for (size_t i = 0; i < model.colors.length; i++) {
			Color_t currentColor = model.colors[i]; //create copy so we can modify alpha
			currentColor.a = helper::clamp(static_cast<int>(static_cast<float>(currentColor.a) * fsub));
			colors[0].addColor(currentColor);
			colors[1].addColor(modColor(currentColor, -17));
			colors[2].addColor(modColor(currentColor, -27));
		}

		/*
			Drawmode:
			00X - empty
			01X - orig color, X index
			10X - light color, X index
			11X - dark color, X index
		*/
		uint64_t drawMode = model.drawMode;
		for (size_t yPos = 0; yPos < 4; yPos++) {
			for (size_t xPos = 0; xPos < 4; xPos++) {
				const uint64_t pixelDrawMode = drawMode & 0b111;
				drawMode >>= 3;
				if ((pixelDrawMode & 0b110) == 0) {
					continue;
				}
				Channel* pos = pngWriter->getPixel(static_cast<size_t>(x) + xPos, static_cast<size_t>(y) + yPos);

				const size_t colorIdx = ((pixelDrawMode & 0b110) >> 1) - 1;
				const Color_t& pixelColor = colors[colorIdx][pixelDrawMode & 1];

				// In case the user wants noise, calc the strength now, depending on the desired intensity and the block's brightness
				int noise = 0;
				if (Global::settings.noise && pixelColor.noise) {
					noise = static_cast<int>(static_cast<float>(Global::settings.noise * pixelColor.noise) * (static_cast<float>(pixelColor.brightness + 10) / 2650.0f));
				}

				blend(pos, pixelColor);

				if (noise) {
					modColor(pos, rand() % (noise * 2) - noise);
				}
			}
		}
	}

	void blend(Channel* const destination, const Channel* const source)
	{
#define PALPHA 3
		if (destination[PALPHA] == 0 || source[PALPHA] == 255) { //compare alpha
			std::memcpy(destination, source, image::PNGWriter::BYTESPERPIXEL);
			return;
		}
#define BLEND(ca,aa,cb) Channel(((size_t(ca) * size_t(aa)) + (size_t(255 - aa) * size_t(cb))) / 255)
		destination[0] = BLEND(source[0], source[PALPHA], destination[0]);
		destination[1] = BLEND(source[1], source[PALPHA], destination[1]);
		destination[2] = BLEND(source[2], source[PALPHA], destination[2]);
		destination[PALPHA] += static_cast<Channel>((size_t(source[PALPHA]) * size_t(255 - destination[PALPHA])) / 255);
	}

	inline void blend(Channel* const destination, const Color_t& source)
	{
		if (destination[PALPHA] == 0 || source.a == 255) { //compare alpha
			destination[0] = source.r;
			destination[1] = source.g;
			destination[2] = source.b;
			destination[3] = source.a;
			return;
		}
#define BLEND(ca,aa,cb) Channel(((size_t(ca) * size_t(aa)) + (size_t(255 - aa) * size_t(cb))) / 255)
		destination[0] = BLEND(source.r, source.a, destination[0]);
		destination[1] = BLEND(source.g, source.a, destination[1]);
		destination[2] = BLEND(source.b, source.a, destination[2]);
		destination[PALPHA] += static_cast<Channel>((size_t(source.a) * size_t(255 - destination[PALPHA])) / 255);
	}

	Color_t modColor(const Color_t& color, const int mod)
	{
		Color_t retCol = color;
		retCol.r = helper::clamp(color.r + mod);
		retCol.g = helper::clamp(color.g + mod);
		retCol.b = helper::clamp(color.b + mod);
		return retCol;
	}

	void modColor(Channel* const pos, const int mod)
	{
		pos[0] = helper::clamp(pos[0] + mod);
		pos[1] = helper::clamp(pos[1] + mod);
		pos[2] = helper::clamp(pos[2] + mod);
	}

	void setColor(Channel* const pos, const Color_t & color)
	{
		pos[0] = color.r;
		pos[1] = color.g;
		pos[2] = color.b;
		pos[3] = color.a;
	}
}