#include "cachegraphic.h"

#include <QGraphicsLineItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QPen>

#include "processorhandler.h"
#include "radix.h"

namespace {

// Return a set of all keys in a map
template <typename TK, typename TV>
std::set<TK> keys(std::map<TK, TV> const& input_map) {
    std::set<TK> keyset;
    for (auto const& element : input_map) {
        keyset.insert(element.first);
    }
    return keyset;
}
}  // namespace

namespace Ripes {

CacheGraphic::CacheGraphic(CacheSim& cache) : QGraphicsObject(nullptr), m_cache(cache), m_fm(m_font) {
    connect(&cache, &CacheSim::configurationChanged, this, &CacheGraphic::cacheParametersChanged);
    connect(&cache, &CacheSim::dataChanged, this, &CacheGraphic::dataChanged);
    connect(&cache, &CacheSim::wayInvalidated, this, &CacheGraphic::wayInvalidated);
    connect(&cache, &CacheSim::cacheInvalidated, this, &CacheGraphic::cacheInvalidated);

    cacheParametersChanged();
}

void CacheGraphic::updateSetReplFields(unsigned setIdx) {
    auto* cacheSet = m_cache.getSet(setIdx);

    if (cacheSet == nullptr) {
        // Nothing to do
        return;
    }

    if (m_cacheTextItems.at(0).at(0).counter == nullptr) {
        // The current cache configuration does not have any replacement field
        return;
    }

    for (const auto& way : m_cacheTextItems[setIdx]) {
        // If counter was just initialized, the actual (software) counter value may be very large. Mask to the
        // number of actual counter bits.
        unsigned counterVal = cacheSet->at(way.first).counter;
        counterVal &= generateBitmask(m_cache.getWaysBits());
        const QString counterText = QString::number(counterVal);
        way.second.counter->setText(counterText);

        // Counter text might have changed; update counter field position to center in column
        const qreal y = setIdx * m_setHeight + way.first * m_wayHeight;
        const qreal x = m_widthBeforeCounter + m_counterWidth / 2 - m_fm.width(counterText) / 2;

        way.second.counter->setPos(x, y);
    }
}

void CacheGraphic::updateWay(unsigned setIdx, unsigned wayIdx) {
    CacheWay& way = m_cacheTextItems.at(setIdx).at(wayIdx);
    const auto& simWay = m_cache.getSet(setIdx)->at(wayIdx);
    // ======================== Update block text fields ======================
    if (simWay.valid) {
        for (int i = 0; i < m_cache.getBlocks(); i++) {
            QGraphicsSimpleTextItem* blockTextItem = nullptr;
            if (way.blocks.count(i) == 0) {
                // Block text item has not yet been created
                const qreal x =
                    m_widthBeforeBlocks + i * m_blockWidth + (m_blockWidth / 2 - m_fm.width("0x00000000") / 2);
                const qreal y = setIdx * m_setHeight + wayIdx * m_wayHeight;

                way.blocks[i] = createGraphicsTextItemSP(x, y);
                blockTextItem = way.blocks[i].get();
            } else {
                blockTextItem = way.blocks.at(i).get();
            }

            // Update block text
            const uint32_t addressForBlock = m_cache.buildAddress(simWay.tag, setIdx, i);
            const auto data = ProcessorHandler::get()->getMemory().readMemConst(addressForBlock);
            const QString text = encodeRadixValue(data, Radix::Hex);
            blockTextItem->setText(text);
            blockTextItem->setToolTip("Address: " + encodeRadixValue(addressForBlock, Radix::Hex));
            // Store the address within the userrole of the block text. Doing this, we are able to easily retrieve the
            // address for the block if the block is clicked.
            blockTextItem->setData(Qt::UserRole, addressForBlock);
        }
    } else {
        // The way is invalid so no block text should be present
        way.blocks.clear();
    }

    // =========================== Update dirty field =========================
    if (way.dirty) {
        way.dirty->setText(QString::number(simWay.dirty));
    }

    // =========================== Update valid field =========================
    if (way.valid) {
        way.valid->setText(QString::number(simWay.valid));
    }

    // ============================ Update tag field ==========================
    if (simWay.valid) {
        QGraphicsSimpleTextItem* tagTextItem = way.tag.get();
        if (tagTextItem == nullptr) {
            const qreal x = m_widthBeforeTag + (m_tagWidth / 2 - m_fm.width("0x00000000") / 2);
            const qreal y = setIdx * m_setHeight + wayIdx * m_wayHeight;
            way.tag = createGraphicsTextItemSP(x, y);
            tagTextItem = way.tag.get();
        }
        const QString tagText = encodeRadixValue(simWay.tag, Radix::Hex);
        tagTextItem->setText(tagText);
    } else {
        // The way is invalid so no tag text should be present
        if (way.tag) {
            way.tag.reset();
        }
    }

    // ==================== Update dirty blocks highlighting ==================
    const std::set<unsigned> graphicDirtyBlocks = keys(way.dirtyBlocks);
    std::set<unsigned> newDirtyBlocks;
    std::set<unsigned> dirtyBlocksToDelete;
    std::set_difference(graphicDirtyBlocks.begin(), graphicDirtyBlocks.end(), simWay.dirtyBlocks.begin(),
                        simWay.dirtyBlocks.end(), std::inserter(dirtyBlocksToDelete, dirtyBlocksToDelete.begin()));
    std::set_difference(simWay.dirtyBlocks.begin(), simWay.dirtyBlocks.end(), graphicDirtyBlocks.begin(),
                        graphicDirtyBlocks.end(), std::inserter(newDirtyBlocks, newDirtyBlocks.begin()));

    // Delete blocks which are not in sync with the current dirty status of the way
    for (const unsigned& blockToDelete : dirtyBlocksToDelete) {
        way.dirtyBlocks.erase(blockToDelete);
    }
    // Create all required new blocks
    for (const auto& blockIdx : newDirtyBlocks) {
        const auto topLeft =
            QPointF(blockIdx * m_blockWidth + m_widthBeforeBlocks, setIdx * m_setHeight + wayIdx * m_wayHeight);
        const auto bottomRight = QPointF((blockIdx + 1) * m_blockWidth + m_widthBeforeBlocks,
                                         setIdx * m_setHeight + (wayIdx + 1) * m_wayHeight);
        way.dirtyBlocks[blockIdx] = std::make_unique<QGraphicsRectItem>(QRectF(topLeft, bottomRight), this);

        const auto& dirtyRectItem = way.dirtyBlocks[blockIdx];
        dirtyRectItem->setZValue(-1);
        dirtyRectItem->setOpacity(0.4);
        dirtyRectItem->setBrush(Qt::darkCyan);
    }

}

QGraphicsSimpleTextItem* CacheGraphic::tryCreateGraphicsTextItem(QGraphicsSimpleTextItem** item, qreal x, qreal y) {
    if (*item == nullptr) {
        *item = new QGraphicsSimpleTextItem(this);
        (*item)->setFont(m_font);
        (*item)->setPos(x, y);
    }
    return *item;
}

std::unique_ptr<QGraphicsSimpleTextItem> CacheGraphic::createGraphicsTextItemSP(qreal x, qreal y) {
    std::unique_ptr<QGraphicsSimpleTextItem> ptr = std::make_unique<QGraphicsSimpleTextItem>(this);
    ptr->setFont(m_font);
    ptr->setPos(x, y);
    return ptr;
}

void CacheGraphic::cacheInvalidated() {
    for (int setIdx = 0; setIdx < m_cache.getSets(); setIdx++) {
        if (const auto* set = m_cache.getSet(setIdx)) {
            for (const auto& way : *set) {
                updateWay(setIdx, way.first);
            }
            updateSetReplFields(setIdx);
        }
    }
}

void CacheGraphic::wayInvalidated(unsigned setIdx, unsigned wayIdx) {
    updateWay(setIdx, wayIdx);
    updateSetReplFields(setIdx);
}

void CacheGraphic::dataChanged(const CacheSim::CacheTransaction* transaction) {
    if (transaction != nullptr) {
        if (this->m_cache.getCacheType() == CacheSim::CacheType::DataCache) {
            std::cerr << "---------------------------------\n";
            std::cerr << "graphpic transaction address: " << transaction->address << " setIdx: " << transaction->index.set << "\n";
        }
        wayInvalidated(transaction->index.set, transaction->index.way);
        updateHighlighting(true, transaction);
    } else {
        updateHighlighting(false, nullptr);
    }
}

QGraphicsSimpleTextItem* CacheGraphic::drawText(const QString& text, qreal x, qreal y) {
    auto* textItem = new QGraphicsSimpleTextItem(text, this);
    textItem->setFont(m_font);
    textItem->setPos(x, y);
    return textItem;
}

void CacheGraphic::updateHighlighting(bool active, const CacheSim::CacheTransaction* transaction) {
    m_highlightingItems.clear();

    if (active) {
        // Redraw highlighting rectangles indicating the current indexing

        // Draw cache set highlighting rectangle
        QPointF topLeft = QPointF(0, transaction->index.set * m_setHeight);
        QPointF bottomRight = QPointF(m_cacheWidth, (transaction->index.set + 1) * m_setHeight);
        m_highlightingItems.emplace_back(std::make_unique<QGraphicsRectItem>(QRectF(topLeft, bottomRight), this));
        auto* setRectItem = m_highlightingItems.rbegin()->get();
        setRectItem->setZValue(-2);
        setRectItem->setOpacity(0.25);
        setRectItem->setBrush(Qt::yellow);

        // Draw cache block highlighting rectangle
        topLeft = QPointF(transaction->index.block * m_blockWidth + m_widthBeforeBlocks, 0);
        bottomRight = QPointF((transaction->index.block + 1) * m_blockWidth + m_widthBeforeBlocks, m_cacheHeight);
        m_highlightingItems.emplace_back(std::make_unique<QGraphicsRectItem>(QRectF(topLeft, bottomRight), this));
        auto* blockRectItem = m_highlightingItems.rbegin()->get();
        blockRectItem->setZValue(-2);
        blockRectItem->setOpacity(0.25);
        blockRectItem->setBrush(Qt::yellow);

        // Draw highlighting on the currently accessed block
        topLeft = QPointF(transaction->index.block * m_blockWidth + m_widthBeforeBlocks,
                          transaction->index.set * m_setHeight + transaction->index.way * m_wayHeight);
        bottomRight = QPointF((transaction->index.block + 1) * m_blockWidth + m_widthBeforeBlocks,
                              transaction->index.set * m_setHeight + (transaction->index.way + 1) * m_wayHeight);
        m_highlightingItems.emplace_back(std::make_unique<QGraphicsRectItem>(QRectF(topLeft, bottomRight), this));
        auto* hitRectItem = m_highlightingItems.rbegin()->get();
        hitRectItem->setZValue(-1);
        if (transaction->isHit) {
            hitRectItem->setOpacity(0.4);
            hitRectItem->setBrush(Qt::green);
        } else {
            hitRectItem->setOpacity(0.8);
            hitRectItem->setBrush(Qt::red);
        }
    }
}

void CacheGraphic::initializeControlBits() {
    for (int setIdx = 0; setIdx < m_cache.getSets(); setIdx++) {
        auto& set = m_cacheTextItems[setIdx];
        auto* cacheSet = m_cache.getSet(setIdx);
        for (int wayIdx = 0; wayIdx < m_cache.getWays(); wayIdx++) {
            const qreal y = setIdx * m_setHeight + wayIdx * m_wayHeight;
            qreal x;

            // Create valid field
            x = m_bitWidth / 2 - m_fm.width("0") / 2;
            set[wayIdx].valid = drawText("0", x, y);

            if (m_cache.getWritePolicy() == CacheSim::WritePolicy::WriteBack) {
                // Create dirty bit field
                x = m_widthBeforeDirty + m_bitWidth / 2 - m_fm.width("0") / 2;
                set[wayIdx].dirty = drawText("0", x, y);
            }

            if (m_cache.getReplacementPolicy() != CacheSim::ReplPolicy::Random && m_cache.getWays() > 1) {
                // Create LRU field
                const QString counterText = QString::number(m_cache.getWays() - 1);
                x = m_widthBeforeCounter + m_counterWidth / 2 - m_fm.width(counterText) / 2;
                set[wayIdx].counter = drawText(counterText, x, y);
            }
        }
    }
}

QRectF CacheGraphic::boundingRect() const {
    // We do not paint anything in Cachegraphic; only instantiate other QGraphicsItem-derived objects. So just return
    // the bounding rect of child items
    return childrenBoundingRect();
}

void CacheGraphic::cacheParametersChanged() {
    // Remove all items
    m_highlightingItems.clear();
    m_cacheTextItems.clear();
    for (const auto& item : childItems())
        delete item;

    // Determine cell dimensions
    m_wayHeight = m_fm.height();
    m_setHeight = m_wayHeight * m_cache.getWays();
    m_blockWidth = m_fm.width(" 0x00000000 ");
    m_bitWidth = m_fm.width("00");
    m_counterWidth = m_fm.width(QString::number(m_cache.getWays()) + "   ");
    m_cacheHeight = m_setHeight * m_cache.getSets();
    m_tagWidth = m_blockWidth;

    // Draw cache:
    new QGraphicsLineItem(0, 0, 0, m_cacheHeight, this);

    qreal width = 0;
    // Draw valid bit column
    new QGraphicsLineItem(m_bitWidth, 0, m_bitWidth, m_cacheHeight, this);
    const QString validBitText = "V";
    auto* validItem = drawText(validBitText, 0, -m_fm.height());
    validItem->setToolTip("Valid bit");
    width += m_bitWidth;

    if (m_cache.getWritePolicy() == CacheSim::WritePolicy::WriteBack) {
        m_widthBeforeDirty = width;

        // Draw dirty bit column
        new QGraphicsLineItem(width + m_bitWidth, 0, width + m_bitWidth, m_cacheHeight, this);
        const QString dirtyBitText = "D";
        auto* dirtyItem = drawText(dirtyBitText, m_widthBeforeDirty, -m_fm.height());
        dirtyItem->setToolTip("Dirty bit");
        width += m_bitWidth;
    }

    m_widthBeforeCounter = width;

    if (m_cache.getReplacementPolicy() != CacheSim::ReplPolicy::Random && m_cache.getWays() > 1) {
        // Draw counter bit column
        new QGraphicsLineItem(width + m_counterWidth, 0, width + m_counterWidth, m_cacheHeight, this);
        const QString CounterBitText = "Cnt";
        auto* textItem = drawText(CounterBitText, width + m_counterWidth / 2 - m_fm.width(CounterBitText) / 2, -m_fm.height());
        textItem->setToolTip("Least Recently Used bits");
        width += m_counterWidth;
    }

    m_widthBeforeTag = width;

    // Draw tag column
    new QGraphicsLineItem(m_tagWidth + width, 0, m_tagWidth + width, m_cacheHeight, this);
    const QString tagText = "Tag";
    drawText(tagText, width + m_tagWidth / 2 - m_fm.width(tagText) / 2, -m_fm.height());
    width += m_tagWidth;

    m_widthBeforeBlocks = width;

    // Draw horizontal lines between cache blocks
    for (int i = 0; i < m_cache.getBlocks(); i++) {
        const QString blockText = "Block " + QString::number(i);
        drawText(blockText, width + m_tagWidth / 2 - m_fm.width(blockText) / 2, -m_fm.height());
        width += m_blockWidth;
        new QGraphicsLineItem(width, 0, width, m_cacheHeight, this);
    }

    m_cacheWidth = width;

    // Draw cache line rows
    for (int i = 0; i <= m_cache.getSets(); i++) {
        qreal verticalAdvance = i * m_setHeight;
        new QGraphicsLineItem(0, verticalAdvance, m_cacheWidth, verticalAdvance, this);

        if (i < m_cache.getSets()) {
            // Draw cache set rows
            for (int j = 1; j < m_cache.getWays(); j++) {
                verticalAdvance += m_wayHeight;
                auto* setLine = new QGraphicsLineItem(0, verticalAdvance, m_cacheWidth, verticalAdvance, this);
                auto pen = setLine->pen();
                pen.setStyle(Qt::DashLine);
                setLine->setPen(pen);
            }
        }
    }

    // Draw line index numbers
    for (int i = 0; i < m_cache.getSets(); i++) {
        const QString text = QString::number(i);

        const qreal y = i * m_setHeight + m_setHeight / 2 - m_wayHeight / 2;
        const qreal x = -m_fm.width(text) * 1.2;
        drawText(text, x, y);
    }

    // Draw index column text
    const QString indexText = "Index";
    const qreal x = -m_fm.width(indexText) * 1.2;
    drawText(indexText, x, -m_fm.height());

    initializeControlBits();
}

}  // namespace Ripes
