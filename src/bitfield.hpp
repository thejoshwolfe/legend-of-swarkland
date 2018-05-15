#ifndef BITFIELD_HPP
#define BITFIELD_HPP

#include <stdint.h>

template <int bitcount>
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

#endif
