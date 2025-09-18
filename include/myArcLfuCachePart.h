#ifndef MYARCLFUCACHEPART_H
#define MYARCLFUCACHEPART_H

#include <unordered_map>
#include <map>
#include <memory>
#include <mutex>
#include <list>
#include "myArcCacheNode.h"

namespace myCacheSystem
{

    template <typename KEY, typename VALUE>
    class myArcLfuCachePart
    {
    public:
        typedef myArcCacheNode<KEY, VALUE> NODE;
        typedef std::shared_ptr<NODE> NODEPTR;
        typedef std::unordered_map<KEY, NODEPTR> NODEMAP;
        typedef std::map<size_t, std::list<NODEPTR>> FreqMap;
        /*
            构造函数
        */
        // 有参构造
        explicit myArcLfuCachePart(size_t capacity, size_t transformThreshold)
            : capacityMain_(capacity), capacityGhost_(capacity), transformThreshold_(transformThreshold), minFreq_(0)
        {
            initArcLfuCacheList();
        }

        bool put(KEY key, VALUE value)
        {
            if (capacityMain_ == 0)
                return false;

            // 如果当前key已经在主缓存，则更改该节点（值，位置）
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = nodeMainMap_.find(key);
            if (it != nodeMainMap_.end())
            {
                return updateExistingNode(it->second, value);
            }
            // 如果不在，则添加到主缓存
            return addNewNode(key, value);
        }

        bool get(KEY key, VALUE &value)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            // 在主缓存中
            auto it = nodeMainMap_.find(key);
            if (it != nodeMainMap_.end())
            {
                updateNodeToFreq(it->second);
                value = it->second->getValue();
                return true;
            }
            // 不在主缓存中
            return false;
        }

        bool checkGhost(KEY key)
        {
            // 查找幽灵缓存中是否存在该 key
            auto it = nodeGhostMap_.find(key);
            if (it != nodeGhostMap_.end())
            {
                removeFromGhost(it->second);
                nodeGhostMap_.erase(it);
                return true;
            }
            return false;
        }

        bool contain(KEY key)
        {
            return nodeMainMap_.find(key) != nodeMainMap_.end();
        }

        void increaseCapacity()
        {
            ++capacityMain_;
        }

        bool decreaseCapacity()
        {
            if (capacityMain_ <= 0)
                return false;

            if (nodeMainMap_.size() == capacityMain_)
            {
                evictLeastFreq();
            }

            --capacityMain_;
            return true;
        }

    private:
        /*
            私有成员函数方法
        */
        void initArcLfuCacheList();

        // 更新已存在节点（值，位置）
        bool updateExistingNode(NODEPTR node, const VALUE &value);

        // 更新节点位置
        void updateNodeToFreq(NODEPTR node);

        // 添加节点
        bool addNewNode(const KEY &key, const VALUE &value);

        // 关键算法，从主缓存移除最近最少访问元素
        void evictLeastFreq();

        // 从幽灵链表移除最早进入的结点
        void removeFifoFromGhost();

        // 从幽灵链表移除结点
        void removeFromGhost(NODEPTR node);

        // 将节点添加到幽灵结点
        void addToGhost(NODEPTR node);

        size_t capacityMain_;       // 主缓存容量
        size_t capacityGhost_;      // 幽灵缓存容量
        size_t transformThreshold_; // 访问次数阈值
        size_t minFreq_;            // 最小访问次数
        std::mutex mutex_;

        NODEPTR headGhost_; // 幽灵缓存头节点
        NODEPTR tailGhost_; // 幽灵缓存尾结点

        NODEMAP nodeMainMap_;  // 主缓存map key-node
        NODEMAP nodeGhostMap_; // 幽灵缓存map key-node

        FreqMap freqMap_; // 访问频次map 频次-list<node>
    };

    template <typename KEY, typename VALUE>
    void myArcLfuCachePart<KEY, VALUE>::initArcLfuCacheList()
    {
        headGhost_ = std::make_shared<NODE>();
        tailGhost_ = std::make_shared<NODE>();
        headGhost_->next_ = tailGhost_;
        tailGhost_->prev_ = headGhost_;
    }

    template <typename KEY, typename VALUE>
    bool myArcLfuCachePart<KEY, VALUE>::updateExistingNode(NODEPTR node, const VALUE &value)
    {
        // 更新值
        node->setValue(value);
        // 更新位置
        updateNodeToFreq(node);
        return true;
    }

    template <typename KEY, typename VALUE>
    void myArcLfuCachePart<KEY, VALUE>::updateNodeToFreq(NODEPTR node)
    {
        // 频次+1
        size_t oldFreq = node->getAccessCount();
        node->addAccessCount();
        size_t newFreq = node->getAccessCount();
        // 从原位置移除
        auto &oldList = freqMap_[oldFreq];
        oldList.remove(node);
        // 移除后如果list为空，则移除该链表
        if (oldList.empty())
        {
            freqMap_.erase(oldFreq);
            if (oldFreq == minFreq_)
            {
                minFreq_ = newFreq;
            }
        }

        // 添加节点到新位置
        if (freqMap_.find(newFreq) == freqMap_.end())
        {
            freqMap_[newFreq] = std::list<NODEPTR>();
        }
        freqMap_[newFreq].push_back(node);
    }

    template <typename KEY, typename VALUE>
    bool myArcLfuCachePart<KEY, VALUE>::addNewNode(const KEY &key, const VALUE &value)
    {
        // 检查主缓存空间是否足够，如果不够，需要删除最少访问频次节点，将其移动到幽灵链表
        if (nodeMainMap_.size() >= capacityMain_)
        {
            evictLeastFreq();
        }
        NODEPTR newNode = std::make_shared<NODE>(key, value);
        // 正确插入到哈希表
        nodeMainMap_.emplace(key, newNode);
        // 将新结点添加到频次为1的链表
        if (freqMap_.find(1) == freqMap_.end())
        {
            freqMap_[1] = std::list<NODEPTR>();
        }
        freqMap_[1].push_back(newNode);
        minFreq_ = 1;
        return true;
    }

    template <typename KEY, typename VALUE>
    void myArcLfuCachePart<KEY, VALUE>::evictLeastFreq()
    {
        if (freqMap_.empty())
            return;

        auto &minFreqList = freqMap_[minFreq_];
        if (minFreqList.empty())
            return;

        // 获取要被删除的结点，将其从主缓存删除
        NODEPTR leastNode = minFreqList.front();
        minFreqList.pop_front();
        // 删除后如果链表为空,从map中删除该链表，并更新最小频次
        if (minFreqList.empty())
        {
            freqMap_.erase(minFreq_);
            // 更新最小频率
            if (!freqMap_.empty())
            {
                minFreq_ = freqMap_.begin()->first;
            }
        }

        // 将结点添加到幽灵缓存
        if (nodeGhostMap_.size() >= capacityGhost_)
        {
            removeFifoFromGhost();
        }
        addToGhost(leastNode);

        // 从主缓存中移除
        nodeMainMap_.erase(leastNode->getKey());
    }

    template <typename KEY, typename VALUE>
    void myArcLfuCachePart<KEY, VALUE>::removeFifoFromGhost()
    {
        NODEPTR oldGhostNode = headGhost_->next_;
        if (oldGhostNode != tailGhost_)
        {
            removeFromGhost(oldGhostNode);
            nodeGhostMap_.erase(oldGhostNode->getKey());
        }
    }

    template <typename KEY, typename VALUE>
    void myArcLfuCachePart<KEY, VALUE>::removeFromGhost(NODEPTR node)
    {
        if (!node->prev_.expired() && node->next_)
        {
            auto prev = node->prev_.lock();
            prev->next_ = node->next_;
            node->next_->prev_ = prev;
            node->next_ = nullptr;
        }
    }

    template <typename KEY, typename VALUE>
    void myArcLfuCachePart<KEY, VALUE>::addToGhost(NODEPTR node)
    {
        // 将节点插入到幽灵链表尾部（在 tailGhost_ 之前）
        if (!tailGhost_->prev_.expired())
        {
            auto prev = tailGhost_->prev_.lock();
            prev->next_ = node;
            node->next_ = tailGhost_;
            tailGhost_->prev_ = node;
            node->prev_ = prev;
            nodeGhostMap_[node->getKey()] = node;
        }
    }
}

#endif // MYARCLFUCACHEPART_H