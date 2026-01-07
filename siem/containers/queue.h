#pragma once

template<typename T>
class Queue {
private:
    T* data;
    unsigned int capacity;
    unsigned int head;
    unsigned int tail;
    unsigned int count;

    void resize();

public:
    Queue();
    ~Queue();

    void push(const T& value);
    bool pop(T& out);

    bool empty() const;
    unsigned int size() const;
    void clear();
};
