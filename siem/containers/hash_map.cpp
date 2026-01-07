#include "hash_map.h"
#include "../include/json.hpp"
#include "../database/database.h"
#include "../collection/collection.h"
#include <stdexcept>

using namespace std;
using json = nlohmann::json;

template<typename K, typename V>
HashMap<K, V>::Node::Node(const K& k, const V& v) : key(k), value(v), next(nullptr) {}

template<typename K, typename V>
HashMap<K, V>::HashMap() : bucket_count(16), item_count(0) {
    buckets = new Node*[bucket_count]();
    for (unsigned int i = 0; i < bucket_count; i++) {
        buckets[i] = nullptr;
    }
}

template<typename K, typename V>
HashMap<K, V>::HashMap(const HashMap& other) : bucket_count(other.bucket_count), item_count(0) {
    buckets = new Node*[bucket_count]();
    for (unsigned int i = 0; i < bucket_count; i++) {
        buckets[i] = nullptr;
    }
    
    auto it = other.begin();
    auto end_it = other.end();
    while (it != end_it) {
        auto kv = *it;
        insert(kv.key, kv.value);
        ++it;
    }
}

template<typename K, typename V>
HashMap<K, V>& HashMap<K, V>::operator=(const HashMap& other) {
    if (this != &other) {
        clear();
        delete[] buckets;
        
        bucket_count = other.bucket_count;
        item_count = 0;
        buckets = new Node*[bucket_count]();
        for (unsigned int i = 0; i < bucket_count; i++) {
            buckets[i] = nullptr;
        }
        
        auto it = other.begin();
        auto end_it = other.end();
        while (it != end_it) {
            auto kv = *it;
            insert(kv.key, kv.value);
            ++it;
        }
    }
    return *this;
}

template<typename K, typename V>
HashMap<K, V>::~HashMap() {
    clear();
    delete[] buckets;
}

template<typename K, typename V>
unsigned int HashMap<K, V>::hash(const K& key) const {
    unsigned int hash_value = 0;
    const char* str = key.c_str();
    while (*str) {
        hash_value = (hash_value * 31) + *str;
        str++;
    }
    return hash_value % bucket_count;
}

template<>
unsigned int HashMap<int, bool>::hash(const int& key) const {
    return key % bucket_count;
}

template<>
unsigned int HashMap<double, bool>::hash(const double& key) const {
    return static_cast<unsigned int>(key) % bucket_count;
}

template<typename K, typename V>
void HashMap<K, V>::rehash() {
    unsigned int old_count = bucket_count;
    bucket_count *= 2;
    Node** new_buckets = new Node*[bucket_count]();
    for (unsigned int i = 0; i < bucket_count; i++) {
        new_buckets[i] = nullptr;
    }
    
    for (unsigned int i = 0; i < old_count; i++) {
        Node* current = buckets[i];
        while (current) {
            Node* next = current->next;
            unsigned int new_index = hash(current->key);
            current->next = new_buckets[new_index];
            new_buckets[new_index] = current;
            current = next;
        }
    }
    
    delete[] buckets;
    buckets = new_buckets;
}

template<typename K, typename V>
void HashMap<K, V>::insert(const K& key, const V& value) {
    if (item_count >= bucket_count * 0.75) {
        rehash();
    }
    
    unsigned int index = hash(key);
    Node* current = buckets[index];
    
    while (current) {
        if (current->key == key) {
            current->value = value;
            return;
        }
        current = current->next;
    }
    
    Node* newNode = new Node(key, value);
    newNode->next = buckets[index];
    buckets[index] = newNode;
    item_count++;
}

template<typename K, typename V>
bool HashMap<K, V>::contains(const K& key) const {
    unsigned int index = hash(key);
    Node* current = buckets[index];
    
    while (current) {
        if (current->key == key) {
            return true;
        }
        current = current->next;
    }
    return false;
}

template<typename K, typename V>
V& HashMap<K, V>::operator[](const K& key) {
    unsigned int index = hash(key);
    Node* current = buckets[index];
    
    while (current) {
        if (current->key == key) {
            return current->value;
        }
        current = current->next;
    }
    
    if (item_count >= bucket_count * 0.75) {
        rehash();
        index = hash(key);
    }
    
    Node* newNode = new Node(key, V());
    newNode->next = buckets[index];
    buckets[index] = newNode;
    item_count++;
    
    return newNode->value;
}

template<typename K, typename V>
unsigned int HashMap<K, V>::count(const K& key) const {
    return contains(key) ? 1 : 0;
}

template<typename K, typename V>
void HashMap<K, V>::clear() {
    for (unsigned int i = 0; i < bucket_count; i++) {
        Node* current = buckets[i];
        while (current) {
            Node* temp = current;
            current = current->next;
            delete temp;
        }
        buckets[i] = nullptr;
    }
    item_count = 0;
}

template<typename K, typename V>
void HashMap<K, V>::Iterator::find_next() {
    if (!map) return;
    
    if (!current) {
        while (bucket_index < map->bucket_count) {
            if (map->buckets[bucket_index]) {
                current = map->buckets[bucket_index];
                return;
            }
            bucket_index++;
        }
    } else {
        current = current->next;
        if (!current) {
            bucket_index++;
            find_next();
        }
    }
}

template<typename K, typename V>
HashMap<K, V>::Iterator::Iterator(HashMap* m, unsigned int index, Node* node) : map(m), bucket_index(index), current(node) {}

template<typename K, typename V>
KeyValue<K, V> HashMap<K, V>::Iterator::operator*() {
    if (!current) throw runtime_error("Dereferencing end iterator");
    return KeyValue<K, V>(current->key, current->value);
}

template<typename K, typename V>
typename HashMap<K, V>::Iterator& HashMap<K, V>::Iterator::operator++() {
    find_next();
    return *this;
}

template<typename K, typename V>
bool HashMap<K, V>::Iterator::operator!=(const Iterator& other) {
    return current != other.current || bucket_index != other.bucket_index;
}

template<typename K, typename V>
void HashMap<K, V>::ConstIterator::find_next() {
    if (!map) return;
    
    if (!current) {
        while (bucket_index < map->bucket_count) {
            if (map->buckets[bucket_index]) {
                current = map->buckets[bucket_index];
                return;
            }
            bucket_index++;
        }
    } else {
        current = current->next;
        if (!current) {
            bucket_index++;
            find_next();
        }
    }
}

template<typename K, typename V>
HashMap<K, V>::ConstIterator::ConstIterator(const HashMap* m, unsigned int index, Node* node) : map(m), bucket_index(index), current(node) {}

template<typename K, typename V>
KeyValue<K, V> HashMap<K, V>::ConstIterator::operator*() const {
    if (!current) throw runtime_error("Dereferencing end iterator");
    return KeyValue<K, V>(current->key, current->value);
}

template<typename K, typename V>
typename HashMap<K, V>::ConstIterator& HashMap<K, V>::ConstIterator::operator++() {
    find_next();
    return *this;
}

template<typename K, typename V>
bool HashMap<K, V>::ConstIterator::operator!=(const ConstIterator& other) {
    return current != other.current || bucket_index != other.bucket_index;
}

template<typename K, typename V>
typename HashMap<K, V>::Iterator HashMap<K, V>::begin() {
    for (unsigned int i = 0; i < bucket_count; i++) {
        if (buckets[i]) {
            return Iterator(this, i, buckets[i]);
        }
    }
    return end();
}

template<typename K, typename V>
typename HashMap<K, V>::Iterator HashMap<K, V>::end() {
    return Iterator(this, bucket_count, nullptr);
}

template<typename K, typename V>
typename HashMap<K, V>::ConstIterator HashMap<K, V>::begin() const {
    for (unsigned int i = 0; i < bucket_count; i++) {
        if (buckets[i]) {
            return ConstIterator(this, i, buckets[i]);
        }
    }
    return end();
}

template<typename K, typename V>
typename HashMap<K, V>::ConstIterator HashMap<K, V>::end() const {
    return ConstIterator(this, bucket_count, nullptr);
}

template class HashMap<string, int>;
template class HashMap<string, string>;
template class HashMap<string, double>;
template class HashMap<string, bool>;
template class HashMap<string, nlohmann::json>;
template class HashMap<string, Collection>;
template class HashMap<int, bool>;
template class HashMap<double, bool>;
template class HashMap<string, Database*>;
