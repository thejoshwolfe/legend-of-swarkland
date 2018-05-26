#ifndef BITFIELD_HPP
#define BITFIELD_HPP

#include <stdint.h>
#include "uint_oversized.hpp"

template <size_t bitcount>
struct BitField;

template<> struct BitField<1>  { typedef uint8_t Type; };
template<> struct BitField<2>  { typedef uint8_t Type; };
template<> struct BitField<3>  { typedef uint8_t Type; };
template<> struct BitField<4>  { typedef uint8_t Type; };
template<> struct BitField<5>  { typedef uint8_t Type; };
template<> struct BitField<6>  { typedef uint8_t Type; };
template<> struct BitField<7>  { typedef uint8_t Type; };

template<> struct BitField<8>  { typedef uint16_t Type; };
template<> struct BitField<9>  { typedef uint16_t Type; };
template<> struct BitField<10> { typedef uint16_t Type; };
template<> struct BitField<11> { typedef uint16_t Type; };
template<> struct BitField<12> { typedef uint16_t Type; };
template<> struct BitField<13> { typedef uint16_t Type; };
template<> struct BitField<14> { typedef uint16_t Type; };
template<> struct BitField<15> { typedef uint16_t Type; };
template<> struct BitField<16> { typedef uint16_t Type; };

template<> struct BitField<17> { typedef uint32_t Type; };
template<> struct BitField<18> { typedef uint32_t Type; };
template<> struct BitField<19> { typedef uint32_t Type; };
template<> struct BitField<20> { typedef uint32_t Type; };
template<> struct BitField<21> { typedef uint32_t Type; };
template<> struct BitField<22> { typedef uint32_t Type; };
template<> struct BitField<23> { typedef uint32_t Type; };
template<> struct BitField<24> { typedef uint32_t Type; };
template<> struct BitField<25> { typedef uint32_t Type; };
template<> struct BitField<26> { typedef uint32_t Type; };
template<> struct BitField<27> { typedef uint32_t Type; };
template<> struct BitField<28> { typedef uint32_t Type; };
template<> struct BitField<29> { typedef uint32_t Type; };
template<> struct BitField<30> { typedef uint32_t Type; };
template<> struct BitField<31> { typedef uint32_t Type; };
template<> struct BitField<32> { typedef uint32_t Type; };

template<> struct BitField<33> { typedef uint64_t Type; };
template<> struct BitField<34> { typedef uint64_t Type; };
template<> struct BitField<35> { typedef uint64_t Type; };
template<> struct BitField<36> { typedef uint64_t Type; };
template<> struct BitField<37> { typedef uint64_t Type; };
template<> struct BitField<38> { typedef uint64_t Type; };
template<> struct BitField<39> { typedef uint64_t Type; };
template<> struct BitField<40> { typedef uint64_t Type; };
template<> struct BitField<41> { typedef uint64_t Type; };
template<> struct BitField<42> { typedef uint64_t Type; };
template<> struct BitField<43> { typedef uint64_t Type; };
template<> struct BitField<44> { typedef uint64_t Type; };
template<> struct BitField<45> { typedef uint64_t Type; };
template<> struct BitField<46> { typedef uint64_t Type; };
template<> struct BitField<47> { typedef uint64_t Type; };
template<> struct BitField<48> { typedef uint64_t Type; };
template<> struct BitField<49> { typedef uint64_t Type; };
template<> struct BitField<50> { typedef uint64_t Type; };
template<> struct BitField<51> { typedef uint64_t Type; };
template<> struct BitField<52> { typedef uint64_t Type; };
template<> struct BitField<53> { typedef uint64_t Type; };
template<> struct BitField<54> { typedef uint64_t Type; };
template<> struct BitField<55> { typedef uint64_t Type; };
template<> struct BitField<56> { typedef uint64_t Type; };
template<> struct BitField<57> { typedef uint64_t Type; };
template<> struct BitField<58> { typedef uint64_t Type; };
template<> struct BitField<59> { typedef uint64_t Type; };
template<> struct BitField<60> { typedef uint64_t Type; };
template<> struct BitField<61> { typedef uint64_t Type; };
template<> struct BitField<62> { typedef uint64_t Type; };
template<> struct BitField<63> { typedef uint64_t Type; };
template<> struct BitField<64> { typedef uint64_t Type; };

template <size_t bitcount>
struct BitField {
    typedef uint_oversized<(bitcount + 63) / 64> Type;
};

template <typename Enum, typename T>
class BitFieldIterator {
public:
    BitFieldIterator(T bits) : bits(bits) {}

    bool next(Enum * out) {
        if (bits == 0)
            return false;
        T result = ctz(bits);
        bits &= ~(1 << result);
        *out = (Enum)result;
        return true;
    }
private:
    T bits;
};

#endif
