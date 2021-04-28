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
 * 2) Pretty much the same thing goes for getCompound(). The pointer
 * to the NBT_Tag instance returned by it is owned by the root NBT
 * object. NEVER delete it on your own, and do NOT use it anymore
 * after you deleted the root NBT instance.
 *
 * Rule of thumb: Only use the pointer returned by getByteArray
 * or getCompound in a temporary context, never store it unless
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
T readBuffer(const std::vector<uint8_t>& data, size_t& pos) {
	T val = helper::swap_endian<T>(*reinterpret_cast<const T*>(&data[pos]));
	pos += sizeof(T);
	return val;
}


///-------------------------------------------

NBT::NBT(const std::vector<uint8_t>& _data)
	: data(_data), _good(true)
{
	_type = tagUnknown;
	size_t pos = 0;
	_good = parseData(_data, pos);
	if (pos > _data.size()) {
		std::cerr << "Error reading NBT data\n";
		exit(1);
	}
}

NBT_Tag::NBT_Tag()
	: _type(tagUnknown)
{
}

NBT_Tag::NBT_Tag(const std::vector<uint8_t>& data, size_t& pos, TagType type)
	: _type(type)
{
	if (!parseData(data, pos, false)) {
		std::cerr << "Error reading NBT\n";
		exit(1);
	}
}

NBT_Tag::NBT_Tag(const std::vector<uint8_t>& data, size_t& pos) {
	if (!parseData(data, pos)) {
		std::cerr << "Error reading NBT List\n";
		exit(1);
	}
}

bool NBT_Tag::parseData(const std::vector<uint8_t>& data, size_t& pos, bool parseHeader)
{

	if (parseHeader) {
		_type = (TagType)data[pos];
		pos++;
		uint16_t strLen = readBuffer<uint16_t>(data, pos);
		if (strLen > 0) {
			_name = std::string((char*)&data[pos], strLen);
			pos += strLen;
		}
	}

	switch (_type)
	{
	case tagByte:
		dataHolder._primDataType._byte = readBuffer<int8_t>(data, pos);
		break;
	case tagShort:
		dataHolder._primDataType._short = readBuffer<int16_t>(data, pos);
		break;
	case tagInt:
		dataHolder._primDataType._int = readBuffer<int32_t>(data, pos);
		break;
	case tagLong:
		dataHolder._primDataType._long = readBuffer<int64_t>(data, pos);
		break;
	case tagFloat:
		dataHolder._primDataType._float = readBuffer<float>(data, pos);
		break;
	case tagDouble:
		dataHolder._primDataType._double = readBuffer<double>(data, pos);
		break;
	case tagByteArray: {
		uint32_t len = readBuffer<uint32_t>(data, pos);
		PrimArray<uint8_t> arr{&data[pos], len};
		pos += len;
		dataHolder._byteArray = arr;
		break;
	}
	case tagString: {
		uint16_t len = readBuffer<uint16_t>(data, pos);
		PrimArray<char> arr{ (const char*)&data[pos], len };
		pos += len;
		dataHolder._string = arr;
		break;
	}
	case tagList: {
		TagType t = (TagType)readBuffer<uint8_t>(data, pos);
		uint32_t len = readBuffer<uint32_t>(data, pos);
		new(&dataHolder._list)(NBTlist);
		if (len > 0) {
			for (uint32_t i = 0; i < len; i++) {
				dataHolder._list.emplace_back(new NBT_Tag(data, pos, t));
			}
		}
		break;
	}
	case tagCompound: {
		new(&dataHolder._compound)(tagmap);
		while (pos < data.size() && data[pos] != 0){
			auto tag = std::unique_ptr<NBT_Tag>(new NBT_Tag(data, pos));
			dataHolder._compound.insert(std::pair<std::string, std::unique_ptr<NBT_Tag>>(tag->getName(), std::move(tag)));
		}
		if (data[pos] == 0) pos++;
		break;
	}
	case tagIntArray: {
		uint32_t len = readBuffer<uint32_t>(data, pos);
		PrimArray<int32_t> arr{ (const int32_t*)&data[pos], len };
		pos += len * sizeof(int32_t);
		dataHolder._intArray = arr;
		break;
	}
	case tagLongArray:{
		uint32_t len = readBuffer<uint32_t>(data, pos);
		PrimArray<int64_t> arr{ (const int64_t*)&data[pos], len };
		pos += len * sizeof(int64_t);
		dataHolder._longArray = arr;
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

NBT_Tag::~NBT_Tag()
{
	if (_type == tagCompound) {
		dataHolder._compound.~tagmap();
	}
	else if (_type == tagList) {
		dataHolder._list.~NBTlist();
	}
}

void NBT_Tag::printTags() const
{
	if (_type != tagCompound) {
		std::cerr << "Not a tagCompound\n";
		return;
	}
	for (auto& val : dataHolder._compound) {
		std::cout << "Have compound '" << val.first << "' of type " << val.second->getType() << '\n';
	}
	std::cout << "End list.\n";
}


// ----- Data getters -------
// The return value is always a boolean, telling you if the tag was found and the operation
// was successful.
// Protip: Always perform a check if the array really has at least the size you expect it
// to have, to save you some headache.

bool NBT_Tag::getCompound(const string& name, NBT_Tag* &compound)
{
	if (_type == tagCompound) {
		auto posVal = dataHolder._compound.find(name);
		if (posVal != dataHolder._compound.end()) {
			if (posVal->second->getType() == tagCompound) {
				compound = posVal->second.get();
				return true;
			}
		}
	}

	compound = nullptr;
	return false;
}

bool NBT_Tag::getList(const string& name, std::list<NBT_Tag*> &lst)
{
	if (_type == tagCompound) {
		auto posVal = dataHolder._compound.find(name);
		if (posVal != dataHolder._compound.end()) {
			if (posVal->second->getType() == tagList) {
				for (auto& v : posVal->second->dataHolder._list) {
					lst.push_back(v.get());
				}
				return true;
			}
		}
	}

	lst.clear();
	return false;
}

bool NBT_Tag::getByte(const string& name, int8_t &value)
{
	if (_type == tagCompound) {
		auto posVal = dataHolder._compound.find(name);
		if (posVal != dataHolder._compound.end()) {
			if (posVal->second->getType() == tagByte) {
				value = posVal->second->dataHolder._primDataType._byte;
				return true;
			}
		}
	}

	value = 0;
	return false;
}

bool NBT_Tag::getShort(const string& name, int16_t &value)
{
	if (_type == tagCompound) {
		auto posVal = dataHolder._compound.find(name);
		if (posVal != dataHolder._compound.end()) {
			if (posVal->second->getType() == tagShort) {
				value = posVal->second->dataHolder._primDataType._short;
				return true;
			}
		}
	}

	value = 0;
	return false;
}

bool NBT_Tag::getInt(const string& name, int32_t &value)
{
	if (_type == tagCompound) {
		auto posVal = dataHolder._compound.find(name);
		if (posVal != dataHolder._compound.end()) {
			if (posVal->second->getType() == tagInt) {
				value = posVal->second->dataHolder._primDataType._int;
				return true;
			}
		}
	}
	value = 0;
	return false;
}

bool NBT_Tag::getLong(const string& name, int64_t &value)
{
	if (_type == tagCompound) {
		auto posVal = dataHolder._compound.find(name);
		if (posVal != dataHolder._compound.end()) {
			if (posVal->second->getType() == tagLong) {
				value = posVal->second->dataHolder._primDataType._long;
				return true;
			}
		}
	}

	value = 0;
	return false;
}

bool NBT_Tag::getByteArray(const string& name, PrimArray<uint8_t>& data)
{
	if (_type == tagCompound) {
		auto posVal = dataHolder._compound.find(name);
		if (posVal != dataHolder._compound.end()) {
			if (posVal->second->getType() == tagByteArray) {
				const auto& d = dataHolder._compound.at(name);
				data._data = d->dataHolder._byteArray._data;
				data._len = d->dataHolder._byteArray._len;
				return true;
			}
		}
	}

	data._data = nullptr;
	data._len = 0;
	return false;
}

bool NBT_Tag::getIntArray(const string& name, PrimArray<int32_t>& data)
{
	if (_type == tagCompound) {
		auto posVal = dataHolder._compound.find(name);
		if (posVal != dataHolder._compound.end()) {
			if (posVal->second->getType() == tagIntArray) {
				const auto& d = dataHolder._compound.at(name);
				data._data = d->dataHolder._intArray._data;
				data._len = d->dataHolder._intArray._len;
				return true;
			}
		}
	}

	data._data = nullptr;
	data._len = 0;
	return false;
}

bool NBT_Tag::getLongArray(const string& name, PrimArray<int64_t>& data)
{
	if (_type == tagCompound) {
		auto posVal = dataHolder._compound.find(name);
		if (posVal != dataHolder._compound.end()) {
			if (posVal->second->getType() == tagLongArray) {
				const auto& d = dataHolder._compound.at(name);
				data._data = d->dataHolder._longArray._data;
				data._len = d->dataHolder._longArray._len;
				return true;
			}
		}
	}

	data._data = nullptr;
	data._len = 0;
	return false;
}

bool NBT_Tag::getString(const string& name, std::string& data)
{
	if (_type == tagCompound) {
		auto posVal = dataHolder._compound.find(name);
		if (posVal != dataHolder._compound.end()) {
			if (posVal->second->getType() == tagString) {
				const auto tmp = dataHolder._compound.at(name)->dataHolder._string;
				data = std::string(tmp._data, tmp._len);
				return true;
			}
		}
	}
	data = "";
	return false;
}
