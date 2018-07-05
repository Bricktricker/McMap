#ifndef _RANGEMAP_H_
#define _RANGEMAP_H_

#include <memory>
#include <stdexcept>
#include <vector>

template<typename keyT,
	typename valT,
	typename compT = std::less<keyT>>
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

		MapNode(const keyT& lowBound, const keyT& upBound, const valT& val)
			: lowerBound(lowBound), upperBound(upBound), value(val)
		{}
	};

	compT compare;
	PtrType root;
	size_t treeSize;

	public:
		RangeMap(compT tmpCmp = compT{})
			: compare(tmpCmp), root{ nullptr }, treeSize(0)
		{};

		RangeMap(const RangeMap<keyT, valT, compT>& other)
			: compare(other.compare),
			root(PtrType{ new MapNode(*other.root) }), treeSize(other.treeSize)
		{};

		RangeMap& operator=(const RangeMap<keyT, valT, compT>& other)
		{
			comparator(other.comparator);
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

		valT& get(const keyT& key) {
			if (!root) {
				throw std::out_of_range("Map is empty");
			}
			MapNode* current = root.get();
			while (current) {
				if (compare(key, current->lowerBound)){
					// key < current->lowerBound, weiter nach links
					current = current->leftTree.get();
				}else if (compare(current->upperBound, key)) {
					//current->upperBound < key, weiter nach links
					current = current->rightTree.get();
				}else {
					return current->value;
				}
			}
			throw std::out_of_range("key not in map");
		}
		valT& operator[](const keyT& key) {
			try{
				return get(key);
			}
			catch (std::out_of_range e) {
				__debugbreak();
				return valT{};
			}
		}

		void balance() {
			std::vector<MapNode*> nodes;
			nodes.reserve(treeSize);
			storeBSTNodes(root.release(), nodes);
			const size_t n = nodes.size();
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
