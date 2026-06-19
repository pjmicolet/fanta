#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <cctype>

class TrieNode {
public:
    std::unordered_map<char, std::unique_ptr<TrieNode>> children;
    bool isEnd = false;
    std::string fullWord;
};

class Trie {
public:
    Trie() : root(std::make_unique<TrieNode>()) {}

    void insert(const std::string& word) {
        TrieNode* curr = root.get();
        for (char c : word) {
            if (curr->children.find(c) == curr->children.end()) {
                curr->children[c] = std::make_unique<TrieNode>();
            }
            curr = curr->children[c].get();
        }
        curr->isEnd = true;
        curr->fullWord = word;
    }

    std::string getSuggestion(const std::string& prefix) {
        if (prefix.empty()) return "";
        TrieNode* curr = root.get();
        for (char c : prefix) {
            char upperC = std::toupper(c);
            if (curr->children.find(upperC) == curr->children.end()) {
                return "";
            }
            curr = curr->children[upperC].get();
        }
        
        // Find the first complete word from this prefix
        return findFirstWord(curr);
    }

private:
    std::unique_ptr<TrieNode> root;

    std::string findFirstWord(TrieNode* node) {
        if (node->isEnd) return node->fullWord;
        for (auto const& [c, child] : node->children) {
            std::string res = findFirstWord(child.get());
            if (!res.empty()) return res;
        }
        return "";
    }
};
