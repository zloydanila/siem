#pragma once
#include "hash_map.h"

template<typename T>
class UnorderedSet {
private:
    HashMap<T, bool> map;

public:
    void insert(const T& value);
    bool contains(const T& value) const;
    unsigned int count(const T& value) const;
    void clear();
    bool empty() const;
    unsigned int size() const;
};