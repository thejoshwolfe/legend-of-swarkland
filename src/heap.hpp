#ifndef HEAP_HPP
#define HEAP_HPP

#include "util.hpp"
#include "list.hpp"

// min heap
// HeapComparator: return negative, 0, or positive number
template<typename T, int (*HeapComparator)(T, T)>
class Heap {
public:
    void insert(T element) {
        int index = _data.length();
        _data.append(element);

        while (index > 0) {
            int parent_index = (index - 1) / 2;
            T parent_element = _data[parent_index];
            if (HeapComparator(parent_element, element) < 0)
                break;
            _data[index] = parent_element;
            _data[parent_index] = element;
            index = parent_index;
        }
    }

    T extract_min() {
        if (size() <= 1) {
            // last element
            return _data.pop();
        }
        T result = _data[0];
        _data[0] = _data.pop();

        int parent_index = 0;
        for (;;) {
            int left_index = parent_index * 2 + 1;
            int right_index = parent_index * 2 + 2;
            if (left_index >= size()) {
                // no children
                break;
            }
            int child_index;
            if (right_index >= size()) {
                // single child
                child_index = left_index;
            } else {
                // two children
                child_index = HeapComparator(_data[left_index], _data[right_index]) < 0 ? left_index : right_index;
            }
            T parent_element = _data[parent_index];
            T child_element = _data[child_index];
            if (HeapComparator(parent_element, child_element) < 0)
                break;
            _data[parent_index] = child_element;
            _data[child_index] = parent_element;
            parent_index = child_index;
        }

        return result;
    }

    int size() const {
        return _data.length();
    }

private:
    List<T> _data;
};

#endif
