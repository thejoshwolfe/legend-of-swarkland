#ifndef REFERENCE_COUNTER_HPP
#define REFERENCE_COUNTER_HPP

#include <stddef.h>

template <typename T>
class Reference;

// public extend this class to enable reference counting.
class ReferenceCounted {
private:
    template <typename T>
    friend class Reference;
    int _reference_count = 0;
};

template <typename T>
class Reference {
public:
    // new references
    Reference() :
            _target(NULL) {
    }
    Reference(T * brand_new_value) :
            _target(brand_new_value) {
        increment_reference();
    }
    // copy references
    Reference(Reference<T> const & other) {
        _target = other._target;
        increment_reference();
    }
    Reference & operator=(Reference const & other) {
        if (this == &other)
            return *this;
        // update in place
        decrement_reference();
        _target = other._target;
        increment_reference();
        return *this;
    }
    // release references
    ~Reference() {
        decrement_reference();
    }
    // access the target
    T * operator->() {
        return _target;
    }
    const T * operator->() const {
        return _target;
    }
    T & operator*() {
        return *_target;
    }
    const T & operator*() const {
        return *_target;
    }
    bool operator==(Reference<T> const & other) const {
        return _target == other._target;
    }
    bool operator!=(Reference<T> const & other) const {
        return !(*this == other);
    }
private:
    T * _target;
    void increment_reference() {
        if (_target == NULL)
            return;
        _target->_reference_count++;
    }
    void decrement_reference() {
        if (_target == NULL)
            return;
        _target->_reference_count--;
        if (_target->_reference_count == 0)
            destroy(_target, 1);
    }
};

#endif
