/*
 * PngReader.h
 *
 *  Created on: 04.11.2010
 *      Author: Zahl
 */

#ifndef _PNGREADER_H_
#define _PNGREADER_H_

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
	uint32_t _width;
	uint32_t _height;
	PngColorType _chans;
	int _bitDepth;
	int _bytesPerPixel;
	uint8_t _status; //Bits left to right: bit 0: File error, bit 1: not a PNG, bit 2: libPNG error

public:
	explicit PngReader(const std::string& filename);
	PngReader();
	void open(const std::string& filename);
	virtual ~PngReader() = default;
	uint32_t getWidth() const noexcept { return _width; }
	uint32_t getHeight() const noexcept { return _height; }
	PngColorType getColorType() const noexcept { return _chans; }
	int getBitsPerChannel() const noexcept { return _bitDepth; }
	bool isValidImage() const noexcept { return _status == 0; }
	const std::vector<uint8_t>& getImageData() const noexcept { return _imageData; }
	int getBytesPerPixel() const noexcept { return _bytesPerPixel; }
};

#endif
