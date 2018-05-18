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
            _target(nullptr) {
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
        if (_target == nullptr)
            return;
        _target->_reference_count++;
    }
    void decrement_reference() {
        if (_target == nullptr)
            return;
        _target->_reference_count--;
        if (_target->_reference_count == 0)
            destroy(_target, 1);
    }
};

template <typename T>
struct Nullable {
    Nullable() : _non_null(false) {}
    Nullable(nullptr_t) : _non_null(false) {}
    Nullable(const T & value) : _non_null(true), _data(value) {}

    T * operator->() {
        assert(_non_null);
        return &_data;
    }
    const T * operator->() const {
        assert(_non_null);
        return &_data;
    }
    T & operator*() {
        assert(_non_null);
        return _data;
    }
    const T & operator*() const {
        assert(_non_null);
        return _data;
    }

    bool operator==(nullptr_t) const {
        return !_non_null;
    }
    bool operator!=(nullptr_t) const {
        return _non_null;
    }
private:
    bool _non_null;
    T _data;
};

#endif
