#pragma once
#include <string>
using namespace std;

template<typename T>
class Vector {
private:
    T* data;
    unsigned int capacity;
    unsigned int size;
    void resize();

public:
    Vector();
    ~Vector();
    void push_back(const T& value);
    T& operator[](unsigned int index);
    const T& operator[](unsigned int index) const;
    unsigned int get_size() const;
    bool empty() const;
    void clear();
};