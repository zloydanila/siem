#pragma once
#include <string>
#include <utility>
#include "../include/json.hpp"

using namespace std;
using json = nlohmann::json;

template<typename K, typename V>
struct KeyValue {
    K key;
    V value;
    KeyValue(const K& k, const V& v) : key(k), value(v) {}
};

template<typename K, typename V>
class HashMap {
private:
    struct Node {
        K key;
        V value;
        Node* next;
        Node(const K& k, const V& v);
    };

    Node** buckets;
    unsigned int bucket_count;
    unsigned int item_count;

    unsigned int hash(const K& key) const;
    void rehash();

public:
    class Iterator {
    private:
        HashMap* map;
        unsigned int bucket_index;
        Node* current;
        void find_next();
    public:
        Iterator(HashMap* m, unsigned int index, Node* node);
        KeyValue<K, V> operator*();
        Iterator& operator++();
        bool operator!=(const Iterator& other);
    };

    class ConstIterator {
    private:
        const HashMap* map;
        unsigned int bucket_index;
        Node* current;
        void find_next();
    public:
        ConstIterator(const HashMap* m, unsigned int index, Node* node);
        KeyValue<K, V> operator*() const;
        ConstIterator& operator++();
        bool operator!=(const ConstIterator& other);
    };

    HashMap();
    HashMap(const HashMap& other);
    HashMap& operator=(const HashMap& other);
    ~HashMap();
    
    void insert(const K& key, const V& value);
    bool contains(const K& key) const;
    V& operator[](const K& key);
    unsigned int count(const K& key) const;
    void clear();
    unsigned int size() const { return item_count; }
    
    Iterator begin();
    Iterator end();
    ConstIterator begin() const;
    ConstIterator end() const;
};