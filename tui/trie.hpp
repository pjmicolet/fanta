#include <string>
#include <unordered_map>
#include <vector>
#include <memory>

class TrieNode {
public:
    std::unordered_map<char, std::shared_ptr<TrieNode>> children;
    bool is_end = false;
    std::string full_word;
};

class Trie {
public:
    Trie() : root(std::make_shared<TrieNode>()) {}

    void insert(const std::string& word) {
        auto curr = root;
        for (char c : word) {
            if (curr->children.find(c) == curr->children.end()) {
                curr->children[c] = std::make_shared<TrieNode>();
            }
            curr = curr->children[c];
        }
        curr->is_end = true;
        curr->full_word = word;
    }

    std::string get_suggestion(const std::string& prefix) {
        if (prefix.empty()) return "";
        auto curr = root;
        for (char c : prefix) {
            char upper_c = std::toupper(c);
            if (curr->children.find(upper_c) == curr->children.end()) {
                return "";
            }
            curr = curr->children[upper_c];
        }
        
        // Find the first complete word from this prefix
        return find_first_word(curr);
    }

private:
    std::shared_ptr<TrieNode> root;

    std::string find_first_word(std::shared_ptr<TrieNode> node) {
        if (node->is_end) return node->full_word;
        for (auto const& [c, child] : node->children) {
            std::string res = find_first_word(child);
            if (!res.empty()) return res;
        }
        return "";
    }
};
