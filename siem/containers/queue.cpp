#include "queue.h"

template<typename T>
Queue<T>::Queue()
    : data(nullptr), capacity(0), head(0), tail(0), count(0) {}

template<typename T>
Queue<T>::~Queue() {
    delete[] data;
}

template<typename T>
void Queue<T>::resize() {
    unsigned int new_capacity = (capacity == 0) ? 1 : capacity * 2;
    T* new_data = new T[new_capacity];

    if(capacity != 0){
        for (unsigned int i = 0; i < count; i++) {
            new_data[i] = data[(head + i) % capacity];
        }
    }

    delete[] data;
    data = new_data;
    capacity = new_capacity;

    head = 0;
    tail = count;
}

template<typename T>
void Queue<T>::push(const T& value) {
    if (count == capacity) {
        resize();
    }
    data[tail] = value;
    tail = (tail + 1) % capacity;
    count++;
}

template<typename T>
bool Queue<T>::pop(T& out) {
    if (count == 0) return false;
    out = data[head];
    head = (head + 1) % capacity;
    count--;
    return true;
}

template<typename T>
bool Queue<T>::empty() const {
    return count == 0;
}

template<typename T>
unsigned int Queue<T>::size() const {
    return count;
}

template<typename T>
void Queue<T>::clear() {
    delete[] data;
    data = nullptr;
    capacity = 0;
    head = tail = count = 0;
}

template class Queue<int>;
