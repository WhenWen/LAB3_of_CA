#pragma once

#include <math.h>
#include <map>
#include <vector>

#include <QObject>

#include "Signals/Signal.h"
#include "../external/VSRTL/core/vsrtl_register.h"
#include "processors/RISC-V/rv_memory.h"
#include "cache_organize_component.h"
#include "cache_policy_object.h"

using RWMemory = vsrtl::core::RVMemory<32, 32>;
using ROMMemory = vsrtl::core::ROM<32, 32>;

namespace Ripes {

class CacheSim : public QObject {
    Q_OBJECT
public:
    static constexpr unsigned s_invalidIndex = static_cast<unsigned>(-1);

    enum class WriteAllocPolicy { WriteAllocate, NoWriteAllocate };
    enum class SkewedAssocPolicy {Skewed, NonSkewed};
    enum class WritePolicy { WriteThrough, WriteBack };
    enum class ReplPolicy { Random, LRU, LRU_LIP, NoCache, PLRU, DIP };
    enum class AccessType { Read, Write };
    enum class CacheType { DataCache, InstrCache };

    struct CacheSize {
        unsigned bits = 0;
        std::vector<QString> components;
    };

    struct CachePreset {
        int blocks;
        int sets;
        int ways;

        WritePolicy wrPolicy;
        WriteAllocPolicy wrAllocPolicy;
        ReplPolicy replPolicy;
        SkewedAssocPolicy skewPolicy;
    };

    struct CacheIndex {
        unsigned set = s_invalidIndex;
        unsigned way = s_invalidIndex;
        unsigned block = s_invalidIndex;
        void assertValid() const {
            Q_ASSERT(set != s_invalidIndex && "Cache set index is invalid");
            Q_ASSERT(way != s_invalidIndex && "Cache way index is invalid");
            Q_ASSERT(block != s_invalidIndex && "Cache block index is invalid");
        }
    };

    struct CacheTransaction {
        uint32_t address;
        CacheIndex index;

        bool isHit = false;
        bool isWriteback = false;  // True if the transaction resulted in an eviction of a dirty cache set
        AccessType type;
        bool transToValid = false;  // True if the cache set just transitioned from invalid to valid
        bool tagChanged = false;    // True if transToValid or the previous entry was evicted
    };

    struct CacheAccessTrace {
        int hits = 0;
        int misses = 0;
        int reads = 0;
        int writes = 0;
        int writebacks = 0;
        CacheAccessTrace() {}
        CacheAccessTrace(const CacheTransaction& transaction) : CacheAccessTrace(CacheAccessTrace(), transaction) {}
        CacheAccessTrace(const CacheAccessTrace& pre, const CacheTransaction& transaction) {
            reads = pre.reads + (transaction.type == AccessType::Read ? 1 : 0);
            writes = pre.writes + (transaction.type == AccessType::Write ? 1 : 0);
            writebacks = pre.writebacks + (transaction.isWriteback ? 1 : 0);
            hits = pre.hits + (transaction.isHit ? 1 : 0);
            misses = pre.misses + (transaction.isHit ? 0 : 1);
        }
    };

    CacheSim(QObject* parent);
    void setType(CacheType type);
    void setWritePolicy(WritePolicy policy);
    void setWriteAllocatePolicy(WriteAllocPolicy policy);
    void setReplacementPolicy(ReplPolicy policy);
    void setSkewedAssocPolicy(SkewedAssocPolicy policy) {
        m_skewPolicy = policy;
    }

    void recvSigAccess(uint32_t address, bool isWrite) {
        if (isWrite) access(address, AccessType::Write);
        else access(address, AccessType::Read);
    }
    void access(uint32_t address, AccessType type);
    void undo();
    void processorReset();

    WriteAllocPolicy getWriteAllocPolicy() const { return m_wrAllocPolicy; }
    ReplPolicy getReplacementPolicy() const { return m_replPolicy; }
    WritePolicy getWritePolicy() const { return m_wrPolicy; }
    SkewedAssocPolicy getSkewedPolicy() const {
        return m_skewPolicy;
    }

    const std::map<unsigned, CacheAccessTrace>& getAccessTrace() const { return m_accessTrace; }

    double getHitRate() const;
    unsigned getHits() const;
    unsigned getMisses() const;
    unsigned getWritebacks() const;
    CacheSize getCacheSize() const;
    CacheType getCacheType() const { return this->m_type; }

    uint32_t buildAddress(unsigned tag, unsigned lineIdx, unsigned blockIdx) const;

    int getBlockBits() const { return m_blocks; }
    int getWaysBits() const { return m_ways; }
    int getSetBits() const { return m_sets; }
    int getTagBits() const { return 32 - 2 /*byte offset*/ - getBlockBits() - getSetBits(); }

    int getBlocks() const { return static_cast<int>(std::pow(2, m_blocks)); }
    int getWays() const { return static_cast<int>(std::pow(2, m_ways)); }
    int getSets() const { return static_cast<int>(std::pow(2, m_sets)); }
    unsigned getBlockMask() const { return m_blockMask; }
    unsigned getTagMask() const { return m_tagMask; }
    unsigned getSetMask() const { return m_setMask; }

    unsigned getSetIdx(const uint32_t address) const;
    unsigned skewhash(uint32_t address, unsigned way);
    uint32_t skewhashhelper(const uint32_t part);

    unsigned getBlockIdx(const uint32_t address) const;
    unsigned getTag(const uint32_t address) const;

    const CacheSet* getSet(unsigned idx) const;

    Gallant::Signal1<bool> sigCacheIsHit;

public slots:
    void setBlocks(unsigned blocks);
    void setSets(unsigned sets);
    void setWays(unsigned ways);
    void setPreset(const CachePreset& preset);

    /**
     * @brief processorWasClocked/processorWasReversed
     * Slot functions for clocked/Reversed signals emitted by the currently attached processor.
     */
    void processorWasClocked();
    void processorWasReversed();

signals:
    void configurationChanged();
    void dataChanged(const CacheTransaction* transaction);
    void hitrateChanged();

    // Signals that the entire cache line @p
    /**
     * @brief wayInvalidated
     * Signals that all ways in the set @param setIdx which contains way @param wayIdx should be invalidated in
     * the graphical view.
     */
    void wayInvalidated(unsigned setIdx, unsigned wayIdx);

    /**
     * @brief cacheInvalidated
     * Signals that all cache sets in the cache should be invalidated in the graphical view
     */
    void cacheInvalidated();

private:
    struct CacheTrace {
        CacheTransaction transaction;
        CacheWay oldWay;
    };

    std::pair<unsigned, CacheWay*> locateEvictionWay(const CacheTransaction& transaction);
    CacheWay evictAndUpdate(CacheTransaction& transaction);
    void analyzeCacheAccess(CacheTransaction& transaction);
    void analyzeCacheAccessSkewedCache(CacheTransaction& transaction);
    void updateConfiguration();
    void pushAccessTrace(const CacheTransaction& transaction);
    void popAccessTrace();
    void setReplacementPolicyObject();

    /**
     * @brief isAsynchronouslyAccessed
     * If the processor is in its 'running' state, it is currently being executed in a separate thread. In this case,
     * cache accessing is also performed asynchronously, and we do not want to perform any signalling to the GUI (the
     * entirety of the graphical representation of the cache is invalidated and redrawn upon asynchronous running
     * finishing).
     */
    bool isAsynchronouslyAccessed() const;

    /**
     * @brief reassociateMemory
     * Binds to a memory component exposed by the processor handler, based on the current cache type.
     */
    void reassociateMemory();

    ReplPolicy m_replPolicy = ReplPolicy::LRU;
    CachePolicyBase* m_replPolicyObject = new LruPolicy(4, 8, 1);


    WritePolicy m_wrPolicy = WritePolicy::WriteBack;
    WriteAllocPolicy m_wrAllocPolicy = WriteAllocPolicy::WriteAllocate;
    SkewedAssocPolicy m_skewPolicy = SkewedAssocPolicy::NonSkewed;

    unsigned m_blockMask = -1;
    unsigned m_setMask = -1;
    unsigned m_tagMask = -1;

    int m_blocks = 0;  // Some power of 2
    int m_sets = 3;   // Some power of 2
    int m_ways = 2;    // Some power of 2

    /**
     * @brief m_memory
     * The cache simulator may be attached to either a ROM or a Read/Write memory element. Accessing the underlying
     * VSRTL component signals are dependent on the given type of the memory.
     */
    CacheType m_type;
    union {
        RWMemory const* rw = nullptr;
        ROMMemory const* rom;

    } m_memory;

    /**
     * @brief m_cacheSets
     * The datastructure for storing our cache hierachy, as per the current cache configuration.
     */
    std::map<unsigned, CacheSet> m_cacheSets;

    void updateCacheSetReplFields(CacheSet& cacheSet, unsigned int setIdx, unsigned wayIdx, bool isHit);
    /**
     * @brief revertCacheSetReplFields
     * Called whenever undoing a transaction to the cache. Reverts a cache set's replacement fields according to the
     * configured replacement policy.
     */
    void revertCacheSetReplFields(CacheSet& cacheSet, const CacheWay& oldWay, unsigned wayIdx);

    /**
     * @brief m_accessTrace
     * The access trace stack contains cache access statistics for each simulation cycle. Contrary to the TraceStack
     * (m_traceStack).
     */
    std::map<unsigned, CacheAccessTrace> m_accessTrace;

    /**
     * @brief m_traceStack
     * The following information is used to track all most-recent modifications made to the stack. The stack is of a
     * fixed sized which is equal to the undo stack of VSRTL memory elements. Storing all modifications allows us to
     * rollback any changes performed to the cache, when clock cycles are undone.
     */
    std::deque<CacheTrace> m_traceStack;

    /**
     * @brief m_isResetting
     * The cacheSim can be reset by either internally modyfing cache configuration parameters or externally through a
     * processor reset. Given that modifying the cache parameters itself will prompt a reset of the processor, we need a
     * way to distinquish whether a processor reset request originated from an internal cache configuration change. If
     * so, we do not emit a processor request signal, avoiding a signalling loop.
     */
    bool m_isResetting = false;

    CacheTrace popTrace();
    void pushTrace(const CacheTrace& trace);
};

const static std::map<CacheSim::ReplPolicy, QString> s_cacheReplPolicyStrings{{CacheSim::ReplPolicy::Random, "Random"},
                                                                              {CacheSim::ReplPolicy::LRU, "LRU"},
                                                                              {CacheSim::ReplPolicy::LRU_LIP, "LRU_LIP"},
                                                                              {CacheSim::ReplPolicy::NoCache, "NoCache"},
                                                                              {CacheSim::ReplPolicy::DIP, "DIP"},
                                                                              {CacheSim::ReplPolicy::PLRU, "PLRU"}};
const static std::map<CacheSim::WriteAllocPolicy, QString> s_cacheWriteAllocateStrings{
    {CacheSim::WriteAllocPolicy::WriteAllocate, "Write allocate"},
    {CacheSim::WriteAllocPolicy::NoWriteAllocate, "No write allocate"}};

const static std::map<CacheSim::WritePolicy, QString> s_cacheWritePolicyStrings{
    {CacheSim::WritePolicy::WriteThrough, "Write-through"},
    {CacheSim::WritePolicy::WriteBack, "Write-back"}};

const static std::map<CacheSim::SkewedAssocPolicy, QString> s_cacheSkewedAssocStrings {
    {CacheSim::SkewedAssocPolicy::Skewed,"Skewed-associative"},
    {CacheSim::SkewedAssocPolicy::NonSkewed, "Non-skewed-associative"}};

}  // namespace Ripes
