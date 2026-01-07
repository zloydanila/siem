#include "vector.h"
#include "../database/database.h"
#include "../include/json.hpp"
#include <thread>

using json = nlohmann::json;

template<typename T>
Vector<T>::Vector() : data(nullptr), capacity(0), size(0) {}

template<typename T>
Vector<T>::~Vector() {
    delete[] data;
}

template<typename T>
void Vector<T>::resize() {
    capacity = capacity == 0 ? 1 : capacity * 2;
    T* new_data = new T[capacity];
    for (unsigned int i = 0; i < size; i++) {
        new_data[i] = data[i];
    }
    delete[] data;
    data = new_data;
}

template<typename T>
void Vector<T>::push_back(const T& value) {
    if (size >= capacity) {
        resize();
    }
    data[size++] = value;
}

template<typename T>
T& Vector<T>::operator[](unsigned int index) {
    return data[index];
}

template<typename T>
const T& Vector<T>::operator[](unsigned int index) const {
    return data[index];
}

template<typename T>
unsigned int Vector<T>::get_size() const {
    return size;
}

template<typename T>
bool Vector<T>::empty() const {
    return size == 0;
}

template<typename T>
void Vector<T>::clear() {
    delete[] data;
    data = nullptr;
    capacity = 0;
    size = 0;
}

template class Vector<int>;
template class Vector<string>;
template class Vector<double>;
template class Vector<bool>;
template class Vector<json>;
template class Vector<Database*>;
template class Vector<thread*>;
