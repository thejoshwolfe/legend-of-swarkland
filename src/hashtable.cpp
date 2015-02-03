#include "hashtable.hpp"

// this can't be inline static, or else any global variables that use it will secretly become static,
// even if you declare them with extern.
int hash_uint256(uint256 a) {
    return hash_oversized<4>(a);
}
