#include <algorithm>
#include <limits>
#include "WorldStorage.h"
#include "globals.h"

constexpr uint32_t HEIGHTMAP_DEFAULT = 0xffff0000;
constexpr auto DEFAULT_LIGHT_GETTER = []() {
	// Preset: all bright / dark depending on night or day
	if (Global::settings.nightmode) {
		return static_cast<uint8_t>(0x11);
	} else if (Global::settings.underground) {
		return static_cast<uint8_t>(0x00);
	} else {
		return static_cast<uint8_t>(0xFF);
	}
};

WorldStorage::WorldStorage()
	: m_minX(std::numeric_limits<int>::max()),
	m_maxX(std::numeric_limits<int>::min()),
	m_minY(std::numeric_limits<int>::max()),
	m_maxY(std::numeric_limits<int>::min()),
	m_minZ(std::numeric_limits<int>::max()),
	m_maxZ(std::numeric_limits<int>::min())
{}

void WorldStorage::clear()
{
	m_chunkStorage.clear();
	m_minX = std::numeric_limits<int>::max();
	m_maxX = std::numeric_limits<int>::min();
	m_minY = std::numeric_limits<int>::max();
	m_maxY = std::numeric_limits<int>::min();
	m_minZ = std::numeric_limits<int>::max();
	m_maxZ = std::numeric_limits<int>::min();
}

void WorldStorage::clearLightMap()
{
	assert(Global::useLightmap());
	for (auto& chunk : m_chunkStorage) {
		for (auto& section : chunk.second.sections) {
			std::fill(section.light.begin(), section.light.end(), static_cast<uint8_t>(0x00));
		}
	}
}

void WorldStorage::setBlock(const int x, const int y, const int z, const StateID_t blockType)
{
	if (blockType == AIR) {
		return;
	}
	const uint64_t packedChunkPos = packBlockPos(x, z);
	auto& chunk = m_chunkStorage[packedChunkPos];

	m_minX = std::min(m_minX, x);
	m_maxX = std::max(m_maxX, x);
	m_minY = std::min(m_minY, y);
	m_maxY = std::max(m_maxY, y);
	m_minZ = std::min(m_minZ, z);
	m_maxZ = std::max(m_maxZ, z);

	auto& section = chunk.getSection(y);

	const auto xIndex = static_cast<size_t>(x & 0xf);
	const auto yIndex = static_cast<size_t>(y & 0xf);
	const auto zIndex = static_cast<size_t>(z & 0xf);

	const auto index = yIndex + (zIndex + (xIndex * CHUNKSIZE_X) * CHUNKSIZE_Z);
	section.terrain[index] = blockType;
}

StateID_t WorldStorage::getBlock(const int x, const int y, const int z) const
{
	const uint64_t packedChunkPos = packBlockPos(x, z);
	const auto chunkItr = m_chunkStorage.find(packedChunkPos);
	if (chunkItr == m_chunkStorage.end()) {
		return AIR;
	}

	//from getSection
	const int8_t sectionY = static_cast<int8_t>(y >> SECTION_Y_SHIFT);
	const auto sectionItr = std::lower_bound(chunkItr->second.sections.begin(), chunkItr->second.sections.end(), sectionY, [](const WorldStorage::SectionStorage& sec, const int8_t targetSec) {
		return sec.sectionY < targetSec;
	});
	if (sectionItr == chunkItr->second.sections.end()) {
		return AIR;
	}

	//from setBlock
	const auto xIndex = static_cast<size_t>(x & 0xf);
	const auto yIndex = static_cast<size_t>(y & 0xf);
	const auto zIndex = static_cast<size_t>(z & 0xf);

	const auto index = yIndex + (zIndex + (xIndex * CHUNKSIZE_X) * CHUNKSIZE_Z);
	return sectionItr->terrain[index];
}

void WorldStorage::setLight(const int x, const int y, const int z, const uint8_t value)
{
	const uint64_t packedChunkPos = packBlockPos(x, z);
	auto& chunk = m_chunkStorage[packedChunkPos];
	auto& section = chunk.getSection(y);

#ifdef _DEBUG
	if (section.light.empty()) {
		abort();
	}
#endif //_DEBUG

	const auto xIndex = static_cast<size_t>(x & 0xf);
	const auto yIndex = static_cast<size_t>(y & 0xf);
	const auto zIndex = static_cast<size_t>(z & 0xf);
	const auto index = yIndex + (zIndex + (xIndex * CHUNKSIZE_X) * CHUNKSIZE_Z);

	auto arrValue = section.light[index / 2];
	arrValue = section.light[index / 2];
	if ((index & 1) == 0) {
		arrValue = (arrValue & 0xf0) | (value);
	} else {
		arrValue = static_cast<uint8_t>((arrValue & 0xf) | (value << 4));
	}
	section.light[index / 2] = value;
}

uint8_t WorldStorage::getLight(const int x, const int y, const int z) const
{
	const uint64_t packedChunkPos = packBlockPos(x, z);
	const auto chunkItr = m_chunkStorage.find(packedChunkPos);
	if (chunkItr == m_chunkStorage.end()) {
		return DEFAULT_LIGHT_GETTER();
	}

	//from getSection
	const int8_t sectionY = static_cast<int8_t>(y >> SECTION_Y_SHIFT);
	const auto sectionItr = std::lower_bound(chunkItr->second.sections.begin(), chunkItr->second.sections.end(), sectionY, [](const WorldStorage::SectionStorage& sec, const int8_t targetSec) {
		return sec.sectionY < targetSec;
	});
	if (sectionItr == chunkItr->second.sections.end()) {
		return DEFAULT_LIGHT_GETTER();
	}

#ifdef _DEBUG
	if (sectionItr->light.empty()) {
		abort();
	}
#endif //_DEBUG

	const auto xIndex = static_cast<size_t>(x & 0xf);
	const auto yIndex = static_cast<size_t>(y & 0xf);
	const auto zIndex = static_cast<size_t>(z & 0xf);
	const auto index = yIndex + (zIndex + (xIndex * CHUNKSIZE_X) * CHUNKSIZE_Z);

	const auto arrValue = sectionItr->light[index / 2];
	if ((arrValue & 1) == 0) {
		return arrValue & 0xf;
	} else {
		return (arrValue & 0xf0) >> 4;
	}
}

void WorldStorage::setHeight(const int x, const int z, const int16_t low, const int16_t hight)
{
	const uint64_t packedChunkPos = packBlockPos(x, z);
	auto& chunk = m_chunkStorage[packedChunkPos];

	const auto xIndex = static_cast<size_t>(x & 0xf);
	const auto zIndex = static_cast<size_t>(z & 0xf);

	const auto index = zIndex + (xIndex * CHUNKSIZE_X);
	chunk.heightMap[index] = (static_cast<uint32_t>(hight) << 16) | static_cast<uint32_t>(low); //TODO: check if casting word with negative values
}

uint32_t WorldStorage::getHeightPacked(const int x, const int z) const
{
	const uint64_t packedChunkPos = packBlockPos(x, z);
	const auto chunkItr = m_chunkStorage.find(packedChunkPos);
	if (chunkItr == m_chunkStorage.end()) {
		return HEIGHTMAP_DEFAULT;
	}

	const auto xIndex = static_cast<size_t>(x & 0xf);
	const auto zIndex = static_cast<size_t>(z & 0xf);

	const auto index = zIndex + (xIndex * CHUNKSIZE_X);
	return chunkItr->second.heightMap[index];
}

uint64_t WorldStorage::packBlockPos(const int x, const int z)
{
	const auto xChunk = x >> 4;
	const auto zChunk = z >> 4;
	return (static_cast<uint64_t>(xChunk) << 32) | static_cast<uint64_t>(zChunk);
}

WorldStorage::ChunkStorage::ChunkStorage()
{
	heightMap.resize(CHUNKSIZE_X * CHUNKSIZE_Z, static_cast<uint32_t>(HEIGHTMAP_DEFAULT));
}

WorldStorage::SectionStorage& WorldStorage::ChunkStorage::getSection(const int y)
{
	const int8_t sectionY = static_cast<int8_t>(y >> SECTION_Y_SHIFT);
	const auto itr = std::lower_bound(sections.begin(), sections.end(), sectionY, [](const WorldStorage::SectionStorage& sec, const int8_t targetSec) {
		return sec.sectionY < targetSec;
	});
	if (itr == sections.end()) {
		return sections.emplace_back(sectionY);
	} else if (itr->sectionY == sectionY) {
		return *itr;
	} else {
		return *sections.emplace(itr, sectionY);
	}
}

WorldStorage::SectionStorage::SectionStorage(const int8_t _sectionY)
	: sectionY(_sectionY)
{
	terrain.resize(CHUNKSIZE_X * CHUNKSIZE_Z * SECTION_Y, static_cast<StateID_t>(AIR));

	if (Global::useLightmap()) {
		const uint8_t defaultLightValue = DEFAULT_LIGHT_GETTER();
		light.resize((CHUNKSIZE_X * CHUNKSIZE_Z * SECTION_Y) / 2, defaultLightValue);
	}
}
