#ifndef _RANGEMAP_H_
#define _RANGEMAP_H_

#include <memory>
#include <stdexcept>
#include <vector>
#include <array>
#include <cassert>

template<typename keyT,
	typename valT,
	typename compT = std::less<keyT>,
	size_t cacheSize = 5>
	class RangeMap {
	struct MapNode;
	typedef std::unique_ptr<MapNode> PtrType;

	struct MapNode {
		keyT lowerBound, upperBound;
		valT value;
		PtrType leftTree, rightTree;

		MapNode(const MapNode& other)
			: lowerBound(other.lowerBound),
			upperBound(other.upperBound),
			value(other.value)
		{
			if (other.leftTree)
				leftTree.reset(new MapNode(*other.leftTree));
			if (other.rightTree)
				rightTree.reset(new MapNode(*other.rightTree));
		}

		MapNode& operator=(const MapNode& other) {
			lowerBound = other.lowerBound;
			upperBound = other.upperBound;
			value = other.value;

			if (other.leftTree)
				leftTree.reset(new MapNode(*other.leftTree));
			if (other.rightTree)
				rightTree.reset(new MapNode(*other.rightTree));

			return *this;
		}

		MapNode(const keyT& lowBound, const keyT& upBound, const valT& val)
			: lowerBound(lowBound), upperBound(upBound), value(val)
		{}
	};

	struct CacheNode {
		keyT m_key;
		valT* m_value; //Not owned by object
	};

	compT compare;
	PtrType root;
	size_t treeSize;
	std::array<CacheNode, cacheSize> m_cache;

	public:

		explicit RangeMap(compT tmpCmp = compT{})
			: compare(tmpCmp), root{ nullptr }, treeSize(0)
		{};

		RangeMap(const RangeMap<keyT, valT, compT>& other)
			: compare(other.compare),
			root(PtrType{ new MapNode(*other.root) }), treeSize(other.treeSize)
		{};

		RangeMap& operator=(const RangeMap<keyT, valT, compT>& other)
		{
			compare = other.compare;
			treeSize = other.treeSize;
			if (other.root)
				root.reset(PtrType{ new MapNode(*other.root) });
			return *this;
		};

		void insert(const keyT& lowBound, const keyT& upBound, const valT& value) {
			if (root) { //Baum ist nicht leer
				MapNode* current = root.get();
				while (current) {
					if (compare(upBound, current->lowerBound)) {
						//upBound < current->lowerBound, weiter nach links
						if (!current->leftTree) {
							current->leftTree = std::make_unique<MapNode>(lowBound, upBound, value);
							break;
						}
						current = current->leftTree.get();
					}
					else if (compare(current->upperBound, lowBound)) {
						//current->upperBound < lowBound, weiter nach rechts
						if (!current->rightTree) {
							current->rightTree = std::make_unique<MapNode>(lowBound, upBound, value);
							break;
						}
						current = current->rightTree.get();
					}
					else {
						//Bound bereits vorhanden
						throw std::out_of_range("Range already in map");
					}
				}
			}
			else {
				root = std::make_unique<MapNode>(lowBound, upBound, value);
			}
			//Objekt eingefügt
			treeSize++;
		}

		void addToCache(const size_t index, const keyT& key) {
			assert(index < cacheSize);
			m_cache[index] = CacheNode{key, &get(key)};
		}

		valT& get(const keyT& key) {
			if (!root) {
				throw std::out_of_range("Map is empty");
			}
			MapNode* current = root.get();
			while (current) {
				if (compare(key, current->lowerBound)){
					// key < current->lowerBound, go to left
					current = current->leftTree.get();
				}else if (compare(current->upperBound, key)) {
					//current->upperBound < key, go to right
					current = current->rightTree.get();
				}else {
					return current->value;
				}
			}
			throw std::out_of_range("key not in map");
		}
		valT operator[](const keyT& key) {
			assert(root);

			//Cache lookup
			size_t idx = cacheSize / 2;
			for (size_t i = 0; i <= cacheSize / 2; ++i) {
				const auto cacheKey = m_cache[idx].m_key;
				if (cacheKey == key) {
					assert(m_cache[idx].m_value != nullptr);
					return *m_cache[idx].m_value;
				}
				else if (cacheKey > key) {
					--idx;
				}
				else {
					++idx;
				}
			}

			MapNode* current = root.get();
			while (current) {
				if (compare(key, current->lowerBound)) {
					// key < current->lowerBound, go to left
					current = current->leftTree.get();
				}
				else if (compare(current->upperBound, key)) {
					//current->upperBound < key, go to right
					current = current->rightTree.get();
				}
				else {
					return current->value;
				}
			}
			return valT{};
		}

		void balance() {
			std::vector<MapNode*> nodes;
			nodes.reserve(treeSize);
			storeBSTNodes(root.release(), nodes);
			const int n = static_cast<const int>(nodes.size());
			root.reset(reBuildTree(nodes, 0, n-1));
		}

		size_t size() const noexcept { return treeSize; };

		void clear() {
			treeSize = 0U;
			root.reset(nullptr);
		}

private: //functions

	void storeBSTNodes(MapNode* node, std::vector<MapNode*>& container) {
		if (node == nullptr)
			return;

		storeBSTNodes(node->leftTree.release(), container);
		container.push_back(node);
		storeBSTNodes(node->rightTree.release(), container);
	}
	MapNode* reBuildTree(const std::vector<MapNode*>& container, const int start, const int end) {
		if (start > end)
			return nullptr;

		const int mid = (start + end) / 2;
		MapNode* node = container[mid];

		node->leftTree.reset(reBuildTree(container, start, mid-1));
		node->rightTree.reset(reBuildTree(container, mid+1, end));
		return node;
	}
};	
#endif
