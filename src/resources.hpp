#ifndef RESOURCES_HPP
#define RESOURCES_HPP

// see Makefile invocation of `ld -r -b binary` for where these symbols are defined and where their names come from.

#if defined(__linux__)
 // everthing's fine
#elif defined(__MINGW32__)
 // in windows, the underscore isn't there for some reason.
 #define _binary_resources_start binary_resources_start
 #define _binary_resources_end binary_resources_end
 #define _binary_resources_size binary_resources_size
#else
 #error "compiler"
#endif
extern unsigned char _binary_resources_start;
extern unsigned char _binary_resources_end;
extern unsigned char _binary_resources_size;

// this is the rucksack bundle for the project
static inline unsigned char * get_binary_resources_start() {
    return &_binary_resources_start;
}
static inline long get_binary_resources_size() {
    return &_binary_resources_end - &_binary_resources_start;
}

#endif
