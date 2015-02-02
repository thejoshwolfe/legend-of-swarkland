#ifndef HEAP_HPP
#define HEAP_HPP

#include "util.hpp"
#include "list.hpp"

// min heap
template<typename T>
class Heap {
public:
    // return negative, 0, or positive number
    typedef int (*HeapComparator)(T a, T b);

    explicit Heap(HeapComparator comparator) :
        _comparator(comparator)
    {
    }
    ~Heap() {
    }

    Heap(Heap &) = delete;

    void insert(T element) {
        int index = _data.size();
        _data.add(element);

        while (index > 0) {
            int parent_index = (index - 1) / 2;
            T parent_element = _data.at(parent_index);
            if (_comparator(parent_element, element) < 0) break;
            _data.at(index) = parent_element;
            _data.at(parent_index) = element;
            index = parent_index;
        }
    }

    T extract_min() {
        if (size() <= 1) {
          // last element
          return _data.pop();
        }
        T result = _data.at(0);
        _data.at(0) = _data.pop();
        
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
            child_index = _comparator(_data.at(left_index), _data.at(right_index)) < 0 ? left_index : right_index;
          }
          T parent_element = _data.at(parent_index);
          T child_element = _data.at(child_index);
          if (_comparator(parent_element, child_element) < 0) break;
          _data.at(parent_index) = child_element;
          _data.at(child_index) = parent_element;
          parent_index = child_index;
        }
        
        return result;
    }

    int size() const {
        return _data.size();
    }


private:

    List<T> _data;

    HeapComparator _comparator;
};

#endif
