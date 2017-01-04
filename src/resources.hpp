#ifndef RESOURCES_HPP
#define RESOURCES_HPP

// see Makefile invocation of `ld -r -b binary` for where these symbols are defined and where their names come from.

#if defined(__linux__)
 #define binary_prefix _binary_
#elif defined(__MINGW32__)
 // in windows, the underscore isn't there for some reason.
 #define binary_prefix binary_
#else
 #error "compiler"
#endif

#define BINARY_RESOURCE_PLEASE(prefix, name) \
 extern unsigned char prefix##name##_start; \
 extern unsigned char prefix##name##_end; \
 static constexpr unsigned char * get_binary_##name##_start() { \
     return &prefix##name##_start; \
 } \
 static constexpr long get_binary_##name##_size() { \
     return &prefix##name##_end - &prefix##name##_start; \
 }
// Welcome to todays lesson in: WTF Doesn't The C Preprocessor Work?
// This intermediate macro function is the only way to get a variable (macro)
// to expand before being used in a concatenation (##) with a macro function parameter.
// Isn't that obvious? For GNU's explanation, see the second bullet here:
// https://gcc.gnu.org/onlinedocs/cpp/Argument-Prescan.html#Argument-Prescan
// Specifically we want to concatenate binary_prefix with name, which are both variables.
// however, since name is a parameter and binary_prefix isn't, we need to make them both parameters.
// But that's not good enough yet. We need them each to be used as parameters to another macro function.
// That way, they're fully expanded before being sent into the final macro function.
// Or something like that; fuck it; this works; I'm moving on.
#define BINARY_RESOURCE(prefix, name) BINARY_RESOURCE_PLEASE(prefix, name)

BINARY_RESOURCE(binary_prefix, version_resource)
BINARY_RESOURCE(binary_prefix, font_resource)
BINARY_RESOURCE(binary_prefix, audio_attack_resource)
BINARY_RESOURCE(binary_prefix, spritesheet_resource)
#undef BINARY_RESOURCE
#undef BINARY_RESOURCE_PLEASE

#endif
