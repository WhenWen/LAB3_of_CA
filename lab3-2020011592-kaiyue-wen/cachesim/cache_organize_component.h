#ifndef CACHE_ORGANIZE_COMPONENT_H
#define CACHE_ORGANIZE_COMPONENT_H

#include <map>
#include <vector>
#include <set>

namespace Ripes {

struct CacheWay {
    uint32_t tag = -1;
    std::set<unsigned> dirtyBlocks;
    bool dirty = false;
    bool valid = false;

    // LRU algorithm relies on invalid cache ways to have an initial high value. -1 ensures maximum value for all
    // way sizes.
    unsigned counter = -1;
    unsigned age = 0;
    bool MRU = false;
};

using CacheSet = std::map<unsigned, CacheWay>;

}




#endif // CACHE_ORGANIZE_COMPONENT_H
