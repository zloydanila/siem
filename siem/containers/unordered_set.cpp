#include "unordered_set.h"

template<typename T>
void UnorderedSet<T>::insert(const T& value) {
    map[value] = true;
}

template<typename T>
bool UnorderedSet<T>::contains(const T& value) const {
    return map.count(value) > 0;
}

template<typename T>
unsigned int UnorderedSet<T>::count(const T& value) const {
    return map.count(value);
}

template<typename T>
void UnorderedSet<T>::clear() {
    map.clear();
}

template<typename T>
bool UnorderedSet<T>::empty() const {
    return map.size() == 0;
}

template<typename T>
unsigned int UnorderedSet<T>::size() const {
    return map.size();
}

template class UnorderedSet<std::string>;
template class UnorderedSet<int>;
template class UnorderedSet<double>;