#ifndef LRU_H
#define LRU_H

#include <memory>
#include <unordered_map>
#include <mutex>

namespace myCacheSystem
{
    // LRU的缓存节点
    template <typename KEY, typename VALUE>
    class myLruNode
    {
    public:
        /*
            构造函数
        */
        // 有参构造
        lruNode(KEY key, VALUE value) : key_(key), value_(value), accessCount_(1)
        {
            this->prev_ = nullptr;
            this->next_ = nullptr;
        }

        /*
            成员函数接口
        */
        // 获取key
        KEY getKey() const
        {
            return this->key_;
        }

        // 获取value
        VALUE getValue() const
        {
            return this->value_;
        }

        // 获取访问次数
        size_t getAccessCount() const
        {
            return this->accessCount_;
        }

        // 更改value
        void setValue(const VALUE &value)
        {
            this->value_ = value;
        }

        // 增加访问次数
        void addAccessCount()
        {
            ++this->accessCount_;
        }

    private:
        KEY key_;                                      // 键
        VALUE value_;                                  // 值
        size_t accessCount_;                           // 访问次数
        std::weak_ptr<myLruNode<KEY, VALUE>> *prev_;   // 前向节点    weak_ptr打破循环引用
        std::shared_ptr<myLruNode<KEY, VALUE>> *next_; // 后向节点    shared_ptr自动释放
    };

    // LRU缓存池
    template <typename KEY, typename VALUE>
    class myLruCache
    {
    public:
        using LruNodeType = myLruNode<Key, Value>;
        using NodePtr = std::shared_ptr<LruNodeType>;
        using NodeMap = std::unordered_map<Key, NodePtr>;

        /*
            构造函数
        */
        // 有参构造
        LruCache(int capacity) : capacity_(capacity)
        {
            this->lruNodeListInit();
        }

        // 析构函数
        ~LruCache() = default;

        /*
            成员函数接口
            对该缓存池的操作有：
            1. 当有新加入数据操作时，先判断该 key 值是否已经在缓存空间中，如果在的话更新 key 对应的 value 值，并把该数据加入到缓存空间的最右边；
            2. 如果新加入数据的 key 值不在缓存空间中，则判断缓存空间是否已满，若缓存空间未满，则构造新的节点加入到缓存空间的最右边，否则把该数据加入到缓存空间的右边并淘汰掉队列最左边的数据（缓存中最久未被使用的数据）；
            3. 访问一个数据且该数据存在于缓存空间中，返回该数据对应值并将该数据放入缓存空间的最右边；
            4. 访问一个数据但该数据不存在于缓存空间中，返回 - 1 表示缓存中无该数据。
        */
        // 添加缓存
        void put(const KEY &key, const VALUE &value)
        {
            // 1. 判断内存大小是否足够
            if (this->capacity_ <= 0)
            {
                return;
            }

            // 2. 缓存区为资源，要加互斥锁，避免竞争
            std::lock_guard<std::mutex> lock(this->mutex_);
            // 3. 查找key是否已经存在，存在则更新value，不存在则添加
            auto it = this->nodeMap_.find(key);
            if (it != nodeMap_.end())
            {
                updataLruNode(it->second, value);
                return;
            }
            addLruNode(key, value);
        }

        // 获取value
        bool get(const KEY &key, VALUE &value)
        {
            // 添加锁，避免竞争
            std::lock_guard<std::mutex> lock(this->mutex_);
            auto it = this->nodeMap_.find(key);
            if (it != nodeMap_.end())
            {
                this->removeToRecent(it->second);
                value = it->second->getValue();
                return true;
            }
            return false;
        }

        // 访问缓存数据函数
        VALUE &get(const KEY &key)
        {
            VALUE value{};
            this->get(key);
            return value;
        }

        // 删除指定结点
        void remove(KEY key)
        {
            std::lock_guard<std::mutex> lock(this->mutex_);
            auto it = this->nodeMap_.find(key);
            if (it != this->nodeMap_.end())
            {
                this->removeNode(it->second);
                this->nodeMap_.erase(it);
            }
        }

    private:
        /*
            私有成员函数方法
        */
        // 链表节点初始化
        void lruNodeListInit()
        {
            head_ = make_shared<LruNodeType>(KEY(), VALUE());
            tail_ = make_shared<LruNodeType>(KEY(), VALUE());
            head_->next_ = tail_;
            tail_->prev_ = head_;
        }

        // 更新节点的value
        void updataLruNode(NodePtr node, const VALUE &value)
        {
            // 1. 更新值
            node->setValue(value);
            // 2. 将该节点移动至末尾
            removeToRecent(NodePtr node);
        }

        // 移动到链表末尾
        void removeToRecent(NodePtr node)
        {
            removeNode(node);
            insertNode(node);
        }

        // 移除节点
        void removeNode(NodePtr node)
        {
            if (!node->prev.expired() && node->next)
            {
                auto prev = node->prev_.lock(); // 获取shared_ptr
                prev->next_ = node->next_;      // 更新前一个节点的next指针
                node->next_->prev_ = prev;      // 更新后一个节点的prev指针
                node->next_ = nullptr;          // 清空next_指针，彻底断开节点与链表的连接,prev_ 是 weak_ptr，不需要手动重置
            }
        }

        // 再末尾插入节点
        void insertNode(NodePtr node)
        {
            auto prev = this->tail_->prev_.lock(); // 获取尾节点上一结点，即最后一个有效节点
            prev->next_ = node;
            node->next_ = this->tail_;
            this->tail_->prev_ = node;
            node->prev_ = prev;
        }

        // 增加结点
        void addLruNode(const KEY &key, const VALUE &value)
        {
            // 判断容量，如果大于等于缓存区，则移除最近最久未使用的结点
            if (this->nodeMap_.size() >= this->capacity_)
            {
                removeLruNode();
            }
            NodePtr newNode = std::make_shared<LruNodeType>(key, value); // 构建新结点
            insertNode(newNode);                                         // 插入末尾
            this->nodeMap_[key] = newNode;                               // 更新哈希表
        }

        // 删除最近最少使用结点
        void removeLruNode()
        {
            NodePtr node = this->head_->next;
            this->removeNode(node);
            this->nodeMap_.erase(node->getKey());
        }

        int capacity_;     // 缓存容量
        NodeMap nodeMap_;  // 哈希表，便于快速查找节点
        std::mutex mutex_; // 互斥锁
        NodePtr head_;     // 虚拟头结点
        NodePtr tail_;     // 虚拟尾结点
    };
}

#endif // LRU_H