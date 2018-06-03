/*
 * PngReader.h
 *
 *  Created on: 04.11.2010
 *      Author: Zahl
 */

#ifndef _PNGREADER_H_
#define _PNGREADER_H_

#include <stdint.h>
#include <cstdlib>
#include <string>
#include <vector>

class PngReader
{
public:
	enum PngColorType {
		RGB,
		RGBA,
		GrayScale,
		GrayScaleAlpha,
		Palette,
		Unknown
	};

private:
	std::vector<uint8_t> _imageData;
	//uint8_t *_imageData;
	uint32_t _width;
	uint32_t _height;
	PngColorType _chans;
	int _bitDepth;
	int _bytesPerPixel;
	uint8_t _status; //Bits left to right: bit 0: File error, bit 1: not a PNG, bit 2: libPNG error

public:
	PngReader(const std::string& filename);
	PngReader();
	void open(const std::string& filename);
	virtual ~PngReader() = default;
	uint32_t getWidth() { return _width; }
	uint32_t getHeight() { return _height; }
	PngColorType getColorType() { return _chans; }
	int getBitsPerChannel() { return _bitDepth; }
	bool isValidImage() { return _status == 0; }
	const std::vector<uint8_t> getImageData() { return _imageData; }
	int getBytesPerPixel() { return _bytesPerPixel; }
};

#endif
