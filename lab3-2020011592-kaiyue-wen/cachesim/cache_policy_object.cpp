#include "cache_policy_object.h"
#include "cache_organize_component.h"
#include <iostream>


namespace Ripes {


void RandomPolicy::locateEvictionWay(std::pair<unsigned, CacheWay*>& ew,
                                     CacheSet& cacheSet, unsigned setIdx) {
    ew.first = std::rand() % ways;
    ew.second = &cacheSet[ew.first];
}

void RandomPolicy::updateCacheSetReplFields(CacheSet &cacheSet, unsigned int setIdx,
                                            unsigned int wayIdx, bool isHit) {
    // No information needs to be updated
    return;
}

void RandomPolicy::revertCacheSetReplFields(CacheSet &cacheSet,
                                            const CacheWay &oldWay,
                                            unsigned int wayIdx) {
    // No information needs to be updated
    return;
}


void LruPolicy::locateEvictionWay(std::pair<unsigned, CacheWay*>& ew,
                                   CacheSet& cacheSet, unsigned setIdx) {
    if (ways == 1) {
        // Nothing to do if we are in LRU and only have 1 set
        ew.first = 0;
        ew.second = &cacheSet[ew.first];
    } else {
        // Lazily all ways in the cache set before starting to iterate
        for (int i = 0; i < ways; i++) {
            cacheSet[i];
        }
        // If there is an invalid cache cacheSet, select that
        for (auto& idx_way : cacheSet) {
            if (!idx_way.second.valid) {
                ew.first = idx_way.first;
                ew.second = &idx_way.second;
                break;
            }
        }
        if (ew.second == nullptr) {
            // Else, Find LRU way
            for (auto& idx_way : cacheSet) {
                if (idx_way.second.counter == ways - 1) {
                    ew.first = idx_way.first;
                    ew.second = &idx_way.second;
                    break;
                }
            }
        }
    }
}

void LruPolicy::updateCacheSetReplFields(CacheSet &cacheSet,unsigned int setIdx,
                                         unsigned int wayIdx, bool isHit) {
    const unsigned preLRU = cacheSet[wayIdx].counter;
    for (auto& idx_way : cacheSet) {
        if (idx_way.second.valid && idx_way.second.counter < preLRU) {
            idx_way.second.counter++;
        }
    }
    cacheSet[wayIdx].counter = 0;
}

void LruPolicy::revertCacheSetReplFields(CacheSet &cacheSet,
                                         const CacheWay &oldWay,
                                         unsigned int wayIdx) {
    for (auto& idx_way : cacheSet) {
        if (idx_way.second.valid && idx_way.second.counter <= oldWay.counter) {
            idx_way.second.counter--;
        }
    }
    cacheSet[wayIdx].counter = oldWay.counter;
}

void LruLipPolicy::locateEvictionWay(std::pair<unsigned, CacheWay*>& ew,
                                        CacheSet& cacheSet, unsigned setIdx) {
    // exactly the same as LRU Policy
    if (ways == 1) {
        // Nothing to do if we are in LRU and only have 1 set
        ew.first = 0;
        ew.second = &cacheSet[ew.first];
    } else {
        // Lazily all ways in the cache set before starting to iterate
        for (int i = 0; i < ways; i++) {
            cacheSet[i];
        }
        // If there is an invalid cache cacheSet, select that
        for (auto& idx_way : cacheSet) {
            if (!idx_way.second.valid) {
                ew.first = idx_way.first;
                ew.second = &idx_way.second;
                break;
            }
        }
        if (ew.second == nullptr) {
            // Else, Find LRU way
            for (auto& idx_way : cacheSet) {
                if (idx_way.second.counter == ways - 1) {
                    ew.first = idx_way.first;
                    ew.second = &idx_way.second;
                    break;
                }
            }
        }
    }
}

void LruLipPolicy::updateCacheSetReplFields(CacheSet &cacheSet, unsigned int setIdx,
                                               unsigned int wayIdx, bool isHit) {
    if(isHit){
        // hitting update is exactly the same as LRU
        const unsigned preLRU = cacheSet[wayIdx].counter;
        for (auto& idx_way : cacheSet) {
            if (idx_way.second.valid && idx_way.second.counter < preLRU) {
                idx_way.second.counter++;
            }
        }
        cacheSet[wayIdx].counter = 0;
    }
    else{
        // if is not hit, meaning an eviction/insertion has happened, we should make cacheSet[wayIdx].counter the largest of all the 
        // valid cacheSet
        const unsigned preLRU = cacheSet[wayIdx].counter;
        const unsigned inf = -1;
        if(preLRU == inf){
            // in this case, a previous invalid cacheway is selected
            unsigned max_counter = 0;
            for (auto& idx_way : cacheSet) {
                if(idx_way.second.valid){
                    // notice that here preLRU is already valid
                    max_counter += 1;
                }
            }
            cacheSet[wayIdx].counter = max_counter - 1;
        }
        // // in the other case, the previous evicted cacheway is already the one with least priority, do nothing
    }
}

void LruLipPolicy::revertCacheSetReplFields(CacheSet &cacheSet,
                                               const CacheWay &oldWay,
                                               unsigned int wayIdx) {
    // ---------------------Part 2. TODO (optional)------------------------------
}

void DipPolicy::locateEvictionWay(std::pair<unsigned, CacheWay*>& ew,
                                        CacheSet& cacheSet, unsigned setIdx) {
    // same as lru
    if (ways == 1) {
        // Nothing to do if we are in LRU and only have 1 set
        ew.first = 0;
        ew.second = &cacheSet[ew.first];
    } else {
        // Lazily all ways in the cache set before starting to iterate
        for (int i = 0; i < ways; i++) {
            cacheSet[i];
        }
        // If there is an invalid cache cacheSet, select that
        for (auto& idx_way : cacheSet) {
            if (!idx_way.second.valid) {
                ew.first = idx_way.first;
                ew.second = &idx_way.second;
                break;
            }
        }
        if (ew.second == nullptr) {
            // Else, Find LRU way
            for (auto& idx_way : cacheSet) {
                if (idx_way.second.counter == ways - 1) {
                    ew.first = idx_way.first;
                    ew.second = &idx_way.second;
                    break;
                }
            }
        }
    }
}

void DipPolicy::updateCacheSetReplFields(CacheSet &cacheSet, unsigned int setIdx,
                                               unsigned int wayIdx, bool isHit) {
    counter += 1;
    if(setIdx == 0){
        lruall += 1;
        if(isHit){
            lruhit += 1;
        }
    }
    if(setIdx == 1){
        lipall += 1;
        if(isHit){
            liphit += 1;
        }
    }
    // printf("%d %d %d %d %d\n",counter, lipall, lruall,liphit,lruhit);
    if(counter == 100000){
        counter = 0;
        double one = 1.0;
        double lrurate = (lruall > 0) ? (lruhit*one)/lruall :0;
        double liprate = (lipall > 0) ? (liphit*one)/lipall :0;
        lru = (lrurate > liprate);
        // printf("%f %f\n",lrurate,liprate);
        lruall = 0;
        lruhit = 0;
        lipall = 0;
        liphit = 0;
    }
    
    if((lru && (setIdx != 1)) || (setIdx == 0) ){
        const unsigned preLRU = cacheSet[wayIdx].counter;
        for (auto& idx_way : cacheSet) {
            if (idx_way.second.valid && idx_way.second.counter < preLRU) {
                idx_way.second.counter++;
            }
        }
        cacheSet[wayIdx].counter = 0;
    }
    else{
        if(isHit){
        // hitting update is exactly the same as LRU
        const unsigned preLRU = cacheSet[wayIdx].counter;
        for (auto& idx_way : cacheSet) {
            if (idx_way.second.valid && idx_way.second.counter < preLRU) {
                idx_way.second.counter++;
            }
        }
        cacheSet[wayIdx].counter = 0;
        }
        else{
            // if is not hit, meaning an eviction/insertion has happened, we should make cacheSet[wayIdx].counter the largest of all the 
            // valid cacheSet
            const unsigned preLRU = cacheSet[wayIdx].counter;
            const unsigned inf = -1;
            if(preLRU == inf){
                // in this case, a previous invalid cacheway is selected
                unsigned max_counter = 0;
                for (auto& idx_way : cacheSet) {
                    if(idx_way.second.valid){
                        // notice that here preLRU is already valid
                        max_counter += 1;
                    }
                }
                cacheSet[wayIdx].counter = max_counter - 1;
            }
            // // in the other case, the previous evicted cacheway is already the one with least priority, do nothing
        }
    }
}

void DipPolicy::revertCacheSetReplFields(CacheSet &cacheSet,
                                               const CacheWay &oldWay,
                                               unsigned int wayIdx) {
    // ---------------------Part 2. TODO (optional)------------------------------
}

void PlruPolicy::locateEvictionWay(std::pair<unsigned, CacheWay*>& ew,
                                        CacheSet& cacheSet, unsigned setIdx) {
    if (ways == 1){
        // one way is trivial
        ew.first = 0;
        ew.second = &cacheSet[ew.first];
    }
    else{
        for(int i = 0; i < ways; i++){
            cacheSet[i];
        }
        for(auto& idx_way : cacheSet){
            if (!idx_way.second.valid) {
                ew.first = idx_way.first;
                ew.second = &idx_way.second;
                break;
            }
        }
    }
    if(ew.second == nullptr){
        for (auto& idx_way : cacheSet) {
            if (! idx_way.second.MRU) {
                ew.first = idx_way.first;
                ew.second = &idx_way.second;
                break;
            }
        }
    }
}

void PlruPolicy::updateCacheSetReplFields(CacheSet &cacheSet, unsigned int setIdx,
                                               unsigned int wayIdx, bool isHit) {
    cacheSet[wayIdx].MRU = true;
    bool alltrue = true; 
    for(auto& idx_way : cacheSet){
        if (! idx_way.second.MRU){
            alltrue = false;
            break;
        }
    }
    if(alltrue){
        for(auto& idx_way : cacheSet){
            idx_way.second.MRU = false;
        }
    }
    cacheSet[wayIdx].MRU = true;
}

void PlruPolicy::revertCacheSetReplFields(CacheSet &cacheSet,
                                               const CacheWay &oldWay,
                                               unsigned int wayIdx) {
    return;
}






}
