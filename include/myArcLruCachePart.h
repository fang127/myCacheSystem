#ifndef MYARCLRUCACHEPART_H
#define MYARCLRUCACHEPART_H

#include <unordered_map>
#include <mutex>
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
        explicit myArcLruCachePart(size_t capacity)
            : capacity_(capacity)
        {
            initArcLruCacheList();
        }

        /*
            成员函数接口
        */
        bool put(KEY key, const VALUE &value)
        {
            // 1. 检查capacity_是否>0，只有大于0才进行put操作
            if (capacity_ >= 0)
                return false;

            // 2. 检查key是否已经在缓存中，如果在，则更新value
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = nodeMap_.find(key);
            if (it != nodeMap_.end())
            {
                return updateExistingNode(it->second, value);
            }
            // 3. 如果不在，添加节点
            return addNewNode(key, value);
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

        size_t capacity_;   // 容量
        NODEPTR headMain_;  // LRU虚拟缓存头节点
        NODEPTR tailMain_;  // LRU虚拟缓存尾节点
        NODEPTR headGhost_; // LRU虚拟幽灵链表头节点
        NODEPTR tailGhost_; // LRU虚拟幽灵链表尾节点
        NODEMAP nodeMap_;   // key-node 便于根据key获取value
        std::mutex mutex_;  // 互斥锁
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
        // 更新访问次数
        node->addAccessCount();
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
        if (nodeMap_.size() >= capacity_)
        {
            evictLeastRecent(); // 驱逐最少访问
        }
        // 2. 添加节点到最新位置
        NODEPTR node = std::make_shared<NODE>(key, value); // 构造节点
        nodeMap_.emplace({key, node});                     // 更新缓存链表
        addToRecentNode(node);                             // 添加节点
        return true;
    }

    template <typename KEY, typename VALUE>
    void myArcLruCachePart<KEY, VALUE>::evictLeastRecent()
    {
        auto leastRecentNode = tailMain_->prev_.lock();
        if (!leastRecentNode || leastRecentNode == headMain_)
            return;

        // 从缓存链表删除
        removeNode(leastRecentNode);
        // 添加到幽灵链表
        if ()
    }
}

#endif // MYARCLRUCACHEPART_H