#include "colors.h"
#include "extractcolors.h"
#include "pngreader.h"
#include "globals.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <array>
#include <iostream>
#include "json.hpp"

using nlohmann::json;

//Not needed?
#define SETCOLOR(col,r,g,b,a) do { \
		colors[col][PBLUE]		= b; \
		colors[col][PGREEN] 		= g; \
		colors[col][PRED] 		= r; \
		colors[col][PALPHA] 		= a; \
		colors[col][BRIGHTNESS]	= (uint8_t)sqrt( \
		                          double(r) *  double(r) * .236 + \
		                          double(g) *  double(g) * .601 + \
		                          double(b) *  double(b) * .163); \
	} while (false)

	
#define SETCOLORNOISE(col,r,g,b,a,n) do { \
		colors[col][PBLUE]		= b; \
		colors[col][PGREEN] 		= g; \
		colors[col][PRED] 		= r; \
		colors[col][PALPHA] 		= a; \
		colors[col][NOISE]		= n; \
		colors[col][BRIGHTNESS]	= (uint8_t)sqrt( \
		                          double(r) *  double(r) * .236 + \
		                          double(g) *  double(g) * .601 + \
		                          double(b) *  double(b) * .163); \
	} while (false)

//struct for Block
struct Block {
	std::map<std::string, uint16_t> states;
};


// See header for description
uint8_t colors[65536][8]; //first Block-ID + extraInformation, second blockdata (PRED PGREEN PBLUE PALPHA NOISE BRIGHTNESS BLOCKTYPE)

std::map<std::string, Block> blocks;

///Setzt bei allen MetaDaten werten die Farbe, addData wird übernommen
void SET_COLORNOISE(uint16_t col, uint8_t r, uint8_t g, uint8_t b, uint8_t a, uint8_t n)
{
	col %= 4096;
	for (int i = 0; i < 16; i++)
	{
		int x = col + (i << 12);
		SETCOLORNOISE(x, r, g, b, a, n);
	}
}
void SET_COLOR(uint16_t col, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
	SET_COLORNOISE(col,r,g,b,a,0);
}
void SET_COLOR1(uint16_t col, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
	SETCOLORNOISE(col,r,g,b,a,0);
}
void SET_COLOR_W(uint16_t col, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
	col -= 239;
	//uint16_t col2 = CARPET + (col<<12);
	col = 35 + (col<<12); //35: whool
	SETCOLORNOISE(col,r,g,b,a,0);
	//SETCOLORNOISE(col2,r,g,b,a,0);
}
void SET_COLOR_C(uint16_t col, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
	col -= 185;
	col = 159 + (col<<12); //159: Clay
	SETCOLORNOISE(col,r,g,b,a,0);
}

void COLOR_COPY(uint16_t from, uint16_t to)
{
	for (int i = 0; i < 16; i++)
	{
		memcpy(colors[(i<<12)+to], colors[(i<<12)+from], 6);
	}
}
//Setzt bei allen MetaDaten werten den Blocktype, addData ist hierbei 0
void SET_BLOCK(uint8_t type, uint8_t block) //type e.g BLOCKFLAT, blog e.g SNOW
{
	for (int i = 0; i < 16; i++)
	{
		colors[(i<<12)+block][BLOCKTYPE] = type; //1<<12 = 4096
	}
}

void loadColors()
{
	std::ifstream i("../Python/113/dataWithColors.json"); //../Python/113/dataWithColors.json
	json jData;
	i >> jData;
	i.close();



}

/**

*/
bool loadColorsFromFile(const std::string& file)
{
	std::ifstream f(file, std::ios::in);
	if (f.fail()) {
		return false;
	}
	/*
	if (Global::settings.lowMemory)
	{
		memset(colorsToMap, 0, sizeof colorsToMap);
		memset(colorsToID, 0, sizeof colorsToID);
	}*/
	int lowmemCounter = 1;
	while (!f.eof()) {
		std::array<char, 500> buffer;
		f.read(buffer.data(), 500);
		if (f.bad()) {
			break;
		}
		//printf("%s %d %d.\n",buffer,*buffer,*(buffer+1));
		size_t pos = 0;
		while (buffer[pos] == ' ' || buffer[pos] == '\t') {
			++pos;
		}
		if (buffer[pos] == '\0' || buffer[pos] == '#' || buffer[pos] == '\12') {
			continue;   // This is a comment or empty line, skip
		}
		int blockid = atoi(&buffer[pos]);
		int blockid3 = 0;
		if (blockid < 1 || blockid > 4095) {
			std::cerr << "Skipping invalid blockid " << blockid <<  " in colors file\n";
			continue;
		}
		
		int suffix = 0;
		while (buffer[pos] != ' ' && buffer[pos] != '\t' && buffer[pos] != '\0') {
			if (buffer[pos] == ':')
			{
				blockid3 = atoi(&buffer[++pos]);
				if (blockid3 < 0 || blockid3 > 15) {
					std::cerr << "Skipping invalid blockid " << blockid << ':' << blockid3 << " in colors file\n";
					suffix = -1;
				}
				suffix = 1;
			}
			++pos;
		}
		if (suffix < 0) continue;

		uint8_t vals[5] = {0,0};
		bool valid = true;
		for (int i = 0; i < 5; ++i) {
			while (buffer[pos] == ' ' || buffer[pos] == '\t') {
				++pos;
			}
			if (buffer[pos] == '\0' || buffer[pos] == '#' || buffer[pos] == '\12') {
				valid = false;
				break;
			}
			vals[i] = clamp(atoi(&buffer[pos]));
			while (buffer[pos] != ' ' && buffer[pos] != '\t' && buffer[pos] != '\0') {
				++pos;
			}
		}
		if (!valid) { //TODO
			if (vals[0] != 0)
			{
				std::cerr << "TODO: colors.cpp:654\n";
				std::cout << blockid << ':' << blockid3 << " copied to " << vals[0] << ':' << (vals[1]&0x0F) << '\n';
			}
			else std::cerr << "Too few arguments for block " << blockid << ", ignoring line.\n";
			continue;
		}
		int blockidSET = (blockid3 << 12) + blockid;
		if (!suffix)
		{
		    for (int blockid3 = 0; blockid3 < 16; blockid3++)
		    {
			memcpy(colors[(blockid3 << 12)+blockid], vals, 5);
			colors[(blockid3 << 12)+blockid][BRIGHTNESS] = GETBRIGHTNESS(colors[(blockid3 << 12)+blockid]);
		    }
		}
		else
		{
		    memcpy(colors[blockidSET], vals, 5);
		    colors[blockidSET][BRIGHTNESS] = GETBRIGHTNESS(colors[blockidSET]);
		}
	}
	return true;
}
/*
* TODO: Funktion umschreiben, das sie fstream nutzt
*/
bool dumpColorsToFile(const std::string& file)
{
	std::ofstream f(file, std::ios::out);
	if (f.bad()) {
		return false;
	}
	f << ("# For Block IDs see http://minecraftwiki.net/wiki/Data_values\n"
				"# Note that noise or alpha (or both) do not work for a few blocks like snow, torches, fences, steps, ...\n"
				"# Actually, if you see any block has an alpha value of 254 you should leave it that way to prevent black artifacts.\n"
				"# If you want to set alpha of grass to <255, use -blendall or you won't get what you expect.\n"
				"# Noise is supposed to look normal using -noise 10\n\n");
	uint16_t head = 0;
	for (size_t i = 1; i < 4096; ++i) {
		uint8_t *c = colors[i];
		if (c[PALPHA] == 0) continue; // if color doesn't exist, don't dump it
		if (++head % 15 == 1) {
			f << "#ID	R	G	B	A	Noise\n";
		}
		f << int(i) << '\t' << int(c[PRED]) << '\t' << int(c[PGREEN]) << '\t' << int(c[PBLUE]) << '\t' << int(c[PALPHA]) << '\t' << int(c[NOISE]) << '\n';
		for (int j = 1; j < 16; j++)
		{
			uint8_t *c2 = colors[i+(j<<12)];
			if (c2[PALPHA] != 0 && (c[PRED] != c2[PRED] || c[PGREEN] != c2[PGREEN] || c[PBLUE] != c2[PBLUE] || c[PALPHA] != c2[PALPHA] || c[NOISE] != c2[NOISE]))
				f << int(i) << ':' << int(j) << '\t' << int(c2[PRED]) << '\t' << int(c2[PGREEN]) << '\t' << int(c2[PBLUE]) << '\t' << int(c2[PALPHA]) << '\t' << int(c2[NOISE]) << '\n';
			//fprintf(f, "%3d:%d\t%3d\t%3d\t%3d\t%3d\t%3d\n", int(i), int(j), int(c2[PRED]), int(c2[PGREEN]), int(c2[PBLUE]), int(c2[PALPHA]), int(c2[NOISE]));
		}
	}
	f << ("\n"
				"# Block types:\n"
				"\n");
	for (size_t i = 1; i < 4096; ++i) {
		uint8_t *c = colors[i];
		if (c[BLOCKTYPE] == 0) continue; // if type is default
		f << "B\t" << int(i) << '\t' << int(c[BLOCKTYPE]) << '\n';
		//fprintf(f, "B\t%3d\t%3d\n", int(i), int(c[BLOCKTYPE]));
		for (int j = 0; j < 16; j++)
		{
			uint8_t *c2 = colors[i+(j<<12)];
			if (c2[BLOCKTYPE] != 0 && (c[BLOCKTYPE] != c2[BLOCKTYPE]))
				f << "B\t" << int(i) << ':' << int(j) << '\t' << int(c2[BLOCKTYPE]) << '\n';
			//fprintf(f, "B\t%3d:%d\t%3d\n", int(i), int(j), int(c2[BLOCKTYPE]));
		}
	}
	return true;
}

/**
 * Extract block colors from given terrain.png file
 */
bool extractColors(const std::string& file)
{
	PngReader png(file);
	if (!png.isValidImage()) return false;
	if (png.getWidth() != png.getHeight() // Quadratic
			|| (png.getWidth() / 16) * 16 != png.getWidth() // Has to be multiple of 16
			|| png.getBitsPerChannel() != 8 // 8 bits per channel, 32bpp in total
			|| png.getColorType() != PngReader::RGBA) return false;
	const auto imgData = png.getImageData();
	// Load em up
	for (int i = 0; i < 256; i++) {
		if (i == TORCH) {
			continue;   // Keep those yellow for now
		}
		int r, g, b, a, n; // i i s g t u o l v n
		if (getTileRGBA(imgData, png.getWidth() / 16, i, r, g, b, a, n)) {
			const bool flag = (colors[i][PALPHA] == 254);
			if (i == FENCE) {
				r = clamp(r + 10);
				g = clamp(g + 10);
				b = clamp(b + 10);
			}
			SETCOLORNOISE(i, r, g, b, a, n);
			if (flag) {
				colors[i][PALPHA] = 254;   // If you don't like this, dump texture pack to txt file and modify that one
			}
		}
	}
	return true;
}

bool loadBlocks(const std::string& path)
{
	std::ifstream i(path); //../Python/113/dataWithColors.json
	json jData;
	i >> jData;
	i.close();


	return false;
}

static PngReader *pngGrass = NULL;
static PngReader *pngLeaf = NULL;
/**
 * This loads grasscolor.png and foliagecolor.png where the
 * biome colors will be read from upon rendering
 * TODO: Funktion umschreiben, das sie fstream nutzt, strings korrigieren
 * Dropped support for biomes
bool loadBiomeColors(const std::string& path)
{
	size_t len = path.size() + 21;
	char *grass = new char[len], *foliage = new char[len];
	snprintf(grass, len + 20, "%s/grasscolor.png", path);
	snprintf(foliage, len + 20, "%s/foliagecolor.png", path);
	pngGrass = new PngReader(grass);
	pngLeaf = new PngReader(foliage);
	if (!pngGrass->isValidImage() || !pngLeaf->isValidImage()) {
		delete[] pngGrass;
		delete[] pngLeaf;
		printf("Could not load %s and %s: no valid PNG files. Biomes disabled.\n", grass, foliage);
		return false;
	}
	if ((pngGrass->getColorType() != PngReader::RGBA && pngGrass->getColorType() != PngReader::RGB) || pngGrass->getBitsPerChannel() != 8
			|| (pngLeaf->getColorType() != PngReader::RGBA && pngLeaf->getColorType() != PngReader::RGB) || pngLeaf->getBitsPerChannel() != 8
			|| pngGrass->getWidth() != 256 || pngGrass->getHeight() != 256 || pngLeaf->getWidth() != 256 || pngLeaf->getHeight() != 256) {
		delete[] pngGrass;
		delete[] pngLeaf;
		printf("Could not load %s and %s; Expecting RGB(A), 8 bits per channel.\n", grass, foliage);
		return false;
	}
	Global::grasscolorDepth = pngGrass->getBytesPerPixel();
	Global::foliageDepth = pngLeaf->getBytesPerPixel();
	Global::grasscolor = pngGrass->getImageData();
	g_TallGrasscolor = new uint8_t[pngGrass->getBytesPerPixel() * 256 * 256];
	g_Leafcolor = pngLeaf->getImageData();
	// Adjust brightness to what colors.txt says
	const int maxG = pngGrass->getWidth() * pngGrass->getHeight() * g_GrasscolorDepth;
	for (int i = 0; i < maxG; ++i) {
		g_TallGrasscolor[i] = ((int)g_Grasscolor[i] * (int)colors[TALL_GRASS][BRIGHTNESS]) / 255;
		g_Grasscolor[i] = ((int)g_Grasscolor[i] * (int)colors[GRASS][BRIGHTNESS]) / 255;
	}
	const int maxT = pngLeaf->getWidth() * pngLeaf->getHeight() * g_FoliageDepth;
	for (int i = 0; i < maxT; ++i) {
		g_Leafcolor[i] = ((int)g_Leafcolor[i] * (int)colors[LEAVES][BRIGHTNESS]) / 255;
	}
	// Now re-calc brightness of those two
	colors[GRASS][BRIGHTNESS] = GETBRIGHTNESS(g_Grasscolor) - 5;
	colors[LEAVES][BRIGHTNESS] = colors[PINELEAVES][BRIGHTNESS] = colors[BIRCHLEAVES][BRIGHTNESS] = GETBRIGHTNESS(g_Leafcolor) - 5;
	printf("Loaded biome color maps from %s\n", path);
	return true;
}
*/