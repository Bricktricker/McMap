#ifndef _TREE_H_
#define _TREE_H_

#include <vector>
#include <map>
template<typename KeyT,typename ValT>
class Tree{

    struct TreeNode {
        std::map<KeyT, TreeNode> nodes;
        ValT value;
		TreeNode() = default;
		explicit TreeNode(const ValT& val)
            : value(val)
        {}
    };

    TreeNode root;
    std::vector<KeyT> order;

public:
    Tree() = default;
	explicit Tree(const ValT& val)
        : root(val)
    {}

    void add(const std::vector<KeyT>& states, const ValT& value){
        TreeNode* current = &root;
        for(const KeyT& state : states){
            current = &(current->nodes[state]);
        }
        current->value = value;
		root.value = value;
    }

	void add(const std::vector<KeyT>& states, const KeyT& endKey, const ValT& value) {
		TreeNode* current = &root;
		for (const KeyT& state : states) {
			current = &(current->nodes[state]);
		}
		current = &(current->nodes[endKey]);
		current->value = value;
		root.value = value;
	}

	void add(const ValT& value) {
		root.value = value;
	}

	const ValT& get(const std::vector<KeyT>& states) const {
		TreeNode const *current = &root;
		for (const KeyT& state : states) {
			current = &(current->nodes.at(state));
		}
		return current->value;
	}

	const ValT& get() const noexcept {
		return root.value;
	}

    void setOrder(const std::vector<KeyT>& newOrder){
        order = newOrder;
    }

    const std::vector<KeyT>& getOrder() const noexcept {
        return order;
    }

	void clear() {
		root.nodes.clear();
		order.clear();
		root.value = ValT{};
	}
};
#endif
