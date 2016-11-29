#ifndef RESOURCES_HPP
#define RESOURCES_HPP

// see Makefile invocation of `ld -r -b binary` for where these symbols are defined and where their names come from.

#if defined(__linux__)
#elif defined(__MINGW32__)
 // in windows, the underscore isn't there for some reason.
 #error "TODO: remove _ prefix for binary resources on windows"
#else
 #error "compiler"
#endif

#define BINARY_RESOURCE(name) \
extern unsigned char _binary_##name##_start; \
extern unsigned char _binary_##name##_end; \
static inline unsigned char * get_binary_##name##_start() { \
    return &_binary_##name##_start; \
} \
static inline long get_binary_##name##_size() { \
    return &_binary_##name##_end - &_binary_##name##_start; \
}
BINARY_RESOURCE(version_resource)
BINARY_RESOURCE(font_resource)

#endif
