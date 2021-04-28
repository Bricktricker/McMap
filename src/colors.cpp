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

bool loadBlockTree(const json& blocks)
{
	for (auto block = blocks.begin(); block != blocks.end(); ++block) {
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

	return true;
}

bool loadColors(const std::string& path)
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

	if (!loadBlockTree(jData["blocks"])) {
		return false;
	}

	const json& models = jData["models"];
	Global::colorMap.reserve(models.size() + 1);
	Global::colorMap.emplace_back(0, false, ColorArray{}); // add air to list

	for (const auto& model : models) {
		const uint64_t drawMode = model["drawMode"];
		const bool isSolidBlock = model["solidBlock"];
		ColorArray colors;

		const json& jColors = model["colors"];
		for (const auto col : jColors) {
			const Channel r = col["r"];
			const Channel g = col["g"];
			const Channel b = col["b"];
			const Channel a = col["a"];
			const uint8_t n = col["n"];
			const uint8_t brightness = (uint8_t)sqrt(double(r) *  double(r) * .236 + double(g) *  double(g) * .601 + double(b) * double(b) * .163);
			colors.addColor(Color_t{ r, g, b, a, n, brightness });
		}

		Global::colorMap.emplace_back(drawMode, isSolidBlock, colors);
	}

	//load special block ids
	const json tags = jData["tags"];
	for (auto tag = tags.begin(); tag != tags.end(); ++tag) {
		SpecialBlocks blockEnum;
		const std::string tagName = tag.key();
		if (tagName == "leaves") {
			blockEnum = SpecialBlocks::LEAVES;
		}
		else if (tagName == "water") {
			blockEnum = SpecialBlocks::WATER;
		}
		else if (tagName == "lava") {
			blockEnum = SpecialBlocks::LAVA;
		}
		else if (tagName == "torches") {
			blockEnum = SpecialBlocks::TORCH;
		}
		else if (tagName == "snow") {
			blockEnum = SpecialBlocks::SNOW;
		}
		else if (tagName == "grass_block") {
			blockEnum = SpecialBlocks::GRASS_BLOCK;
		}
		else {
			std::cerr << "Warning: unknown tag " << tagName << " in colors file\n";
			continue;
		}

		const json items = tag.value();
		Global::specialBlockMap[blockEnum] = std::vector<StateID_t>(items.begin(), items.end());
	}

	return true;
}
