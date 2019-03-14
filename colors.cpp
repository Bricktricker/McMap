//C++ Header
#include <fstream>
#include <iostream>
#include <map>
//My-Header
#include "defines.h"
#include "globals.h"
#include "colors.h"
#include "json.hpp"

using nlohmann::json;

void buildTree(std::vector<std::string>& strVec, const json jState, Tree<std::string, StateID_t>& tree) {
	for (auto itr = jState.begin(); itr != jState.end(); ++itr) {
		if (itr->is_primitive()) {
			const StateID_t val = itr.value();
			tree.add(strVec, itr.key(), val);
		}
		else {
			strVec.push_back(itr.key());
			buildTree(strVec, *itr, tree);
			strVec.pop_back();
		}
	}
}

bool loadBlockTree(const std::string& path)
{
	json jData;
	try {
		std::ifstream i(path);
		if (i.fail()) {
			std::cerr << "Could not open " << path << '\n';
			return false;
		}

		i >> jData;
		i.close();
	}
	catch (const nlohmann::json::parse_error& e) {
		std::cerr << e.what() << std::endl;
		return false;
	}

	const json allBlocks = jData["allBlocks"];
	for (auto block = allBlocks.begin(); block != allBlocks.end(); ++block) {
		const std::string name = block.key();
		Tree<std::string, StateID_t> tree;
		
		const auto jOrderItr = block->find("order");
		if (jOrderItr != block->end()) {
			tree.setOrder(*jOrderItr);

			json jStates = (*block)["states"];
			std::vector<std::string> vec;
			buildTree(vec, jStates, tree);
		}
		else {
			tree.add((*block)["states"][""]);
		}

		Global::blockTree[name] = tree;
	}

	//load special block ids
	const json specialBlocks = jData["specialBlocks"];
	for (auto block = specialBlocks.begin(); block != specialBlocks.end(); ++block) {
		SpecialBlocks blockEnum;
		const std::string blockStr = block.key();
		if (blockStr == "grass_block") {
			blockEnum = SpecialBlocks::GRASS_BLOCK;
		}
		else if (blockStr == "water") {
			blockEnum = SpecialBlocks::WATER;
		}
		else if (blockStr == "lava") {
			blockEnum = SpecialBlocks::LAVA;
		}
		else if (blockStr == "leaves") {
			blockEnum = SpecialBlocks::LEAVES;
		}
		else if (blockStr == "torch") {
			blockEnum = SpecialBlocks::TORCH;
		}
		else if (blockStr == "snow") {
			blockEnum = SpecialBlocks::SNOW;
		}
		else {
			std::cerr << "Error loading BlockID.json file";
			return false;
		}

		std::vector<std::pair<StateID_t, StateID_t>> ranges;
		for (auto range : block.value()) {
			const StateID_t minID = range["min"];
			const StateID_t maxID = range["max"];
			ranges.emplace_back(minID, maxID);
		}
		Global::specialBlockMap[blockEnum] = ranges;
	}

	return true;
}

bool loadColorMap(const std::string& path)
{
	json jData;
	try {
		std::ifstream i(path);
		if (i.fail()) {
			std::cerr << "Could not open " << path << '\n';
			return false;
		}

		i >> jData;
		i.close();
	}
	catch (const nlohmann::json::parse_error& e) {
		std::cerr << e.what() << std::endl;
		return false;
	}

	for (const auto& col : jData) {
		StateID_t from = col["from"];
		StateID_t to = col["to"];
		const json color = col["color"];
		Channel r, g, b, a;
		uint8_t noise, blockType;
		r = color["r"];
		g = color["g"];
		b = color["b"];
		a = color["a"];
		noise = color["n"];
		blockType = col["blockType"];
		uint8_t brightness = (uint8_t)sqrt(double(r) *  double(r) * .236 + double(g) *  double(g) * .601 + double(b) * double(b) * .163);

		Color_t finalCol{ r, g, b, a, noise, brightness, blockType };
		Global::colorMap.insert(from, to, finalCol);
	}
	Color_t finalCol{ 0, 0, 0, 0, 0, 0, 0 };
	Global::colorMap.insert(0, 0, finalCol);

	Global::colorMap.balance();

	//cachecontent: Air, Stone, Grass, Dirt, Water 
	Global::colorMap.addToCache(0, 0);
	Global::colorMap.addToCache(1, 1);
	Global::colorMap.addToCache(2, 9);
	Global::colorMap.addToCache(3, 10);
	Global::colorMap.addToCache(4, 34);

	return true;
}
