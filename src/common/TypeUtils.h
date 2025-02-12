/*
 * Copyright (c) 2021, Antti Hyvarinen <antti.hyvarinen@gmail.com>
 * Copyright (c) 2021, Martin Blicha <martin.blicha@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef OPENSMT_TYPEUTILS_H
#define OPENSMT_TYPEUTILS_H

namespace opensmt {

// std::pair is not trivially copyable.  This version seems to allow on current compilers faster implementations.
template<class T, class U>
struct pair {
    T first;
    U second;
};

template<typename T>
class span {
    T *_beg;
    uint32_t _size;
public:
    span(T *beg, uint32_t size) : _beg{beg}, _size{size} {}

    uint32_t size() const { return _size; }

    T const & operator[](uint32_t index) const { return *(_beg + index); }

    T const *begin() const { return _beg; }

    T const *end() const { return _beg + _size; }
};
}

#endif //OPENSMT_TYPEUTILS_H
