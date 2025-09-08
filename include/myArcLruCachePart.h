#ifndef MYARCLRUCACHEPART_H
#define MYARCLRUCACHEPART_H

#include <unordered_map>
#include <mutex>
#include <memory>
#include "myArcCacheNode.h"

namespace myCacheSystem
{
    template <typename KEY, typename VALUE>
    class myArcLruCachePart
    {
    public:
        typedef myArcCacheNode<KEY, VALUE> NODE;
        typedef std::shared_ptr<NODE> NODEPTR;
        typedef std::unordered_map<KEY, NODEPTR> NODEMAP;

        /*
            构造函数
        */
        explicit myArcLruCachePart(size_t capacity, size_t transformThreshold = 3)
            : capacity_(capacity), ghostCapacity_(capacity), transformThreshold_(transformThreshold)
        {
            initArcLruCacheList();
        }

        /*
            成员函数接口
        */
        // 向缓存添加节点
        bool put(KEY key, const VALUE &value)
        {
            // 1. 检查capacity_是否>0，只有大于0才进行put操作
            if (capacity_ == 0)
                return false;

            // 2. 检查key是否已经在缓存中，如果在，则更新value
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = nodeMainMap_.find(key);
            if (it != nodeMainMap_.end())
            {
                return updateExistingNode(it->second, value);
            }
            // 3. 如果不在，添加节点
            return addNewNode(key, value);
        }

        // 根据key，value找到节点
        bool get(KEY key, VALUE &value, bool &shouldTransform)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            // 1. 在主缓存查找
            auto it = nodeMainMap_.find(key);
            if (it != nodeMainMap_.end())
            {
                value = it->second->getValue();
                shouldTransform = updateNodeAccess(it->second);
                return true;
            }
            return false;
        }

        // 增加容量
        void increaseCapacity()
        {
            ++capacity_;
        }

        // 减少容量
        bool decreaseCapacity()
        {
            if (capacity_ <= 0)
            {
                return false;
            }
            if (nodeMainMap_.size() == capacity_)
            {
                evictLeastRecent();
            }
            --capacity_;
            return true;
        }

        //
        bool checkGhost(kEY key)
        {
            auto it = nodeGhostMap_.find(key);
            if (it != nodeGhostMap_.end())
            {
                removeFromGhost(it->second);
                nodeGhostMap_.erase(it->second->getKey());
                return true;
            }
            return false;
        }

    private:
        /*
            私有成员函数方法
        */
        // 初始化两个链表
        void initArcLruCacheList();

        // 更新缓存链表中的结点
        bool updateExistingNode(NODEPTR node, const VALUE &value);

        // 移除结点
        void removeNode(NODEPTR node);

        // 移动结点到最新位置
        void addToRecentNode(NODEPTR node);

        // 添加节点
        bool addNewNode(const KEY &key, const VALUE &value);

        // 移除最近最少访问节点
        void evictLeastRecent();

        // 采用FIFO的策略移除幽灵链表中先进入的节点
        void removeFifoFromGhost();

        // 从幽灵链表移除
        void removeFromGhost(NODEPTR node);

        // 向幽灵链表添加
        void addToGhost(NODEPTR node);

        // 更新节点accessCount
        bool updateNodeAccess(NODEPTR node);

        size_t capacity_;           // 主容量
        size_t ghostCapacity_;      // 幽灵链表容量
        size_t transformThreshold_; // 转换门槛
        NODEPTR headMain_;          // LRU虚拟缓存头节点
        NODEPTR tailMain_;          // LRU虚拟缓存尾节点
        NODEPTR headGhost_;         // LRU虚拟幽灵链表头节点
        NODEPTR tailGhost_;         // LRU虚拟幽灵链表尾节点
        NODEMAP nodeMainMap_;       // key-node 主链表
        NODEMAP nodeGhostMap_;      // key-node 幽灵链表
        std::mutex mutex_;          // 互斥锁
    };

    template <typename KEY, typename VALUE>
    void myArcLruCachePart<KEY, VALUE>::initArcLruCacheList()
    {
        headMain_ = std::make_shared<NODE>();
        tailMain_ = std::make_shared<NODE>();

        headMain_->next_ = tailMain_;
        tailMain_->prev_ = headMain_;

        headGhost_ = std::make_shared<NODE>();
        tailGhost_ = std::make_shared<NODE>();

        headGhost_->next_ = tailGhost_;
        tailGhost_->prev_ = headGhost_;
    }

    template <typename KEY, typename VALUE>
    bool myArcLruCachePart<KEY, VALUE>::updateExistingNode(NODEPTR node, const VALUE &value)
    {
        // 更新值
        node->setValue(value);
        // 更新位置
        removeNode(node);
        addToRecentNode(node);
        return true;
    }

    template <typename KEY, typename VALUE>
    void myArcLruCachePart<KEY, VALUE>::removeNode(NODEPTR node)
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
    void myArcLruCachePart<KEY, VALUE>::addToRecentNode(NODEPTR node)
    {
        auto prev = tailMain_->prev_.lock();
        prev->next_ = node;
        node->next_ = tailMain_;
        tailMain_->prev_ = node;
        node->prev_ = prev;
    }

    template <typename KEY, typename VALUE>
    bool myArcLruCachePart<KEY, VALUE>::addNewNode(const KEY &key, const VALUE &value)
    {
        // 1. 判断当前capacity是否足够
        if (nodeMainMap_.size() >= capacity_)
        {
            evictLeastRecent(); // 驱逐最少访问
        }
        // 2. 添加节点到最新位置
        NODEPTR node = std::make_shared<NODE>(key, value); // 构造节点
        nodeMainMap_.emplace({key, node});                 // 更新主map
        addToRecentNode(node);                             // 添加节点
        return true;
    }

    template <typename KEY, typename VALUE>
    void myArcLruCachePart<KEY, VALUE>::evictLeastRecent()
    {
        auto leastRecentNode = headMain_->next_;
        if (!leastRecentNode || leastRecentNode == tailMain_)
            return;

        // 从缓存链表删除
        removeNode(leastRecentNode);
        // 添加到幽灵链表
        if (nodeGhostMap_.size() >= capacity_)
        {
            removeFifoFromGhost(); // 采用FIFO的策略移除幽灵链表中的节点
        }

        addToGhost(leastRecentNode);                   // 添加到幽灵链表
        nodeMainMap_.erase(leastRecentNode->getKey()); // 从主缓存map移除
    }

    template <typename KEY, typename VALUE>
    void myArcLruCachePart<KEY, VALUE>::removeFifoFromGhost()
    {
        auto oldestGhostNode = headGhost_->next_;
        if (!oldestGhostNode || oldestGhostNode == tailGhost_)
            return;

        removeFromGhost(oldestGhostNode);
        nodeGhostMap_.erase(oldestGhostNode->getKey());
    }

    template <typename KEY, typename VALUE>
    void myArcLruCachePart<KEY, VALUE>::removeFromGhost(NODEPTR node)
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
    void myArcLruCachePart<KEY, VALUE>::addToGhost(NODEPTR node)
    {
        // 重置节点访问次数
        node->accessCount_ = 1;
        // 添加节点到幽灵链表尾
        auto prev = tailGhost_->prev_.lock();
        prev->next_ = node;
        node->next_ = tailGhost_;
        tailGhost_->prev_ = node;
        node->prev_ = prev;

        // 添加节点到map
        nodeGhostMap_.emplace({node->getKey(), node});
    }

    template <typename KEY, typename VALUE>
    bool myArcLruCachePart<KEY, VALUE>::updateNodeAccess(NODEPTR node)
    {
        node->addAccessCount(); // 增加访问次数
        // 更新位置
        removeNode(node);
        addToRecentNode(node);
        return node->getAccessCount() >= transformThreshold_;
    }
}

#endif // MYARCLRUCACHEPART_H