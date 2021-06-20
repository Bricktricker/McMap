/***
 * Tiny OO NBT parser for C++
 * v1.0, 09-2010 by Zahl
 * modified by Philipp
 */
#include <iostream>
#include <limits>
#include "nbt.h"
#include "helper.h"

 // For MSVC++, get "zlib compiled DLL" from http://www.zlib.net/ and read USAGE.txt
 // gcc from MinGW works with the static library of zlib; however, trying
 // static linking in MSVC++ gave me weird results and crashes (v2008)
 // on linux, static linking works too, of course, but shouldn't be needed

 /* A few words on this class:
  * It is written in a half-way OO manner. That is, it breaks
  * some best practice rules. For example, for speed and efficiency
  * reasons, it shares memory across classes and even returns pointers
  * to memory locations inside the memory block owned by the NBT class.
  * That's why you should keep two things in mind:
  * 1) The pointer returned by getByteArray() lies inside a memory
  * block that belongs to the NBT class. NEVER try to delete it, and
  * do NOT use it anymore after you deleted the instance of NBT (or
  * after you left the scope of a local instance)
  *
  * Rule of thumb: Only use the pointer returned by getByteArray
  * in a temporary context, never store it unless
  * you know what you're doing.
  * All the other get-methods return a copy of the requested data
  * (if found), so you're safe here...
  *
  * There is no way (yet) to list all available tags, simply because
  * it isn't needed here. It shouldn't be too much work to add that
  * if you need it.
  *
  * --
  * You may use and modify this class in you own projects, just
  * keep a note in your source code that you used/modified my class.
  * And make your tool open source of course.
  * Thanks :-)
  */

template<typename T>
T readBuffer(const std::vector<uint8_t>& data, size_t& pos)
{
	T val = helper::swap_endian<T>(*reinterpret_cast<const T*>(&data[pos]));
	pos += sizeof(T);
	return val;
}

///-------------------------------------------

NBT::NBT(const std::vector<uint8_t>& _data)
	: m_data(_data), m_good(true)
{
	m_type = tagUnknown;
	size_t pos = 0;
	m_good = parseData(_data, pos);
	if (pos > _data.size()) {
		std::cerr << "Error reading NBT data\n";
		exit(1);
	}
}

NBTtag::NBTtag()
	: m_type(tagUnknown)
{}

NBTtag::NBTtag(const std::vector<uint8_t>& data, size_t& pos, TagType type)
	: m_type(type)
{
	if (!parseData(data, pos, false)) {
		std::cerr << "Error reading NBT\n";
		exit(1);
	}
}

NBTtag::NBTtag(const std::vector<uint8_t>& data, size_t& pos)
{
	if (!parseData(data, pos)) {
		std::cerr << "Error reading NBT List\n";
		exit(1);
	}
}

bool NBTtag::parseData(const std::vector<uint8_t>& data, size_t& pos, bool parseHeader)
{
	if (parseHeader) {
		m_type = (TagType) data[pos];
		pos++;
		uint16_t strLen = readBuffer<uint16_t>(data, pos);
		if (strLen > 0) {
			m_name = std::string((char*) &data[pos], strLen);
			pos += strLen;
		}
	}

	switch (m_type) {
	case tagByte:
		m_dataHolder.emplace<int8_t>(readBuffer<int8_t>(data, pos));
		break;
	case tagShort:
		m_dataHolder.emplace<int16_t>(readBuffer<int16_t>(data, pos));
		break;
	case tagInt:
		m_dataHolder.emplace<int32_t>(readBuffer<int32_t>(data, pos));
		break;
	case tagLong:
		m_dataHolder.emplace<int64_t>(readBuffer<int64_t>(data, pos));
		break;
	case tagFloat:
		m_dataHolder.emplace<float>(readBuffer<float>(data, pos));
		break;
	case tagDouble:
		m_dataHolder.emplace<double>(readBuffer<double>(data, pos));
		break;
	case tagByteArray:
	{
		uint32_t len = readBuffer<uint32_t>(data, pos);
		m_dataHolder.emplace<PrimArray<uint8_t>>(&data[pos], len);
		pos += len;
		break;
	}
	case tagString:
	{
		uint16_t len = readBuffer<uint16_t>(data, pos);
		m_dataHolder.emplace<PrimArray<char>>(reinterpret_cast<const char*>(&data[pos]), len);
		pos += len;
		break;
	}
	case tagList:
	{
		TagType t = (TagType) readBuffer<uint8_t>(data, pos);
		uint32_t len = readBuffer<uint32_t>(data, pos);
		auto& list = m_dataHolder.emplace<NBTlist>();
		list.reserve(len);
		for (uint32_t i = 0; i < len; i++) {
			list.emplace_back(data, pos, t);
		}
		break;
	}
	case tagCompound:
	{
		auto& map = m_dataHolder.emplace<TagMap>();
		while (pos < data.size() && data[pos] != 0) {
			const NBTtag tag(data, pos);
			map.insert(std::pair<std::string, NBTtag>(tag.getName(), tag));
		}
		if (data[pos] == 0) pos++;
		break;
	}
	case tagIntArray:
	{
		uint32_t len = readBuffer<uint32_t>(data, pos);
		m_dataHolder.emplace<PrimArray<int32_t>>(reinterpret_cast<const int32_t*>(&data[pos]), len);
		pos += len * sizeof(int32_t);
		break;
	}
	case tagLongArray:
	{
		uint32_t len = readBuffer<uint32_t>(data, pos);
		m_dataHolder.emplace<PrimArray<int64_t>>(reinterpret_cast<const int64_t*>(&data[pos]), len);
		pos += len * sizeof(int64_t);
		break;
	}
	default:
		std::cerr << "NBT file corrupted\n";
		exit(1);
		break;
	}

	if (pos > data.size()) {
		exit(1);
		return false;
	}

	return true;
}

void NBTtag::printTags() const
{
	if (m_type != tagCompound) {
		std::cerr << "Not a tagCompound\n";
		return;
	}
	const TagMap& compound = std::get<TagMap>(m_dataHolder);
	for (auto& val : compound) {
		std::cout << "Have compound '" << val.first << "' of type " << val.second.getType() << '\n';
	}
	std::cout << "End list.\n";
}

std::optional<const NBTtag*> NBTtag::getCompound(const std::string_view name) const
{
	if (m_type == tagCompound) {
		const auto& compound = std::get<TagMap>(m_dataHolder);
		const auto posVal = compound.find(name);
		if (posVal != compound.end()) {
			if (posVal->second.getType() == tagCompound) {
				return &posVal->second;
			}
		}
	}
	return std::nullopt;
}

std::optional<const NBTtag::NBTlist*> NBTtag::getList(const std::string_view name) const
{
	if (m_type == tagCompound) {
		const auto& compound = std::get<TagMap>(m_dataHolder);
		const auto posVal = compound.find(name);
		if (posVal != compound.end()) {
			if (posVal->second.getType() == tagList) {
				return &std::get<NBTtag::NBTlist>(posVal->second.m_dataHolder);
			}
		}
	}
	return std::nullopt;
}

std::optional<std::string> NBTtag::getString(const std::string_view name) const
{
	if (m_type == tagCompound) {
		const auto& compound = std::get<TagMap>(m_dataHolder);
		const auto posVal = compound.find(name);
		if (posVal != compound.end()) {
			if (posVal->second.getType() == tagString) {
				const auto span = std::get<PrimArray<char>>(posVal->second.m_dataHolder);
				return std::string(span.m_data, span.m_len);
			}
		}
	}
	return std::nullopt;
}

