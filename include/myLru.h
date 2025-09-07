#ifndef MYLRU_H
#define MYLRU_H

#include <memory>
#include <mutex>
#include <vector>
#include <unordered_map>
#include "myCachePolicy.h"

namespace myCacheSystem
{
    // 前向声明
    template <typename KEY, typename VALUE>
    class myLruCache;

    // LRU的缓存节点
    template <typename KEY, typename VALUE>
    class myLruNode
    {
        friend class myLruCache<KEY, VALUE>;

    public:
        /*
            构造函数
        */
        // 默认构造
        myLruNode() : accessSize_(1), next_(nullptr) {};
        // 有参构造
        myLruNode(KEY key, VALUE value) : key_(key), value_(value), accessCount_(1), next_(nullptr) {}

        /*
            成员函数接口
        */
        // 获取key
        KEY getKey() const { return this->key_; }

        // 获取value
        VALUE getValue() const { return this->value_; }

        // 获取访问次数
        size_t getAccessCount() const { return this->accessCount_; }

        // 更改value
        void setValue(const VALUE &value) { this->value_ = value; }

        // 增加访问次数
        void addAccessCount() { ++this->accessCount_; }

    private:
        KEY key_;                                     // 键
        VALUE value_;                                 // 值
        size_t accessCount_;                          // 访问次数
        std::weak_ptr<myLruNode<KEY, VALUE>> prev_;   // 前向节点 weak_ptr打破循环引用
        std::shared_ptr<myLruNode<KEY, VALUE>> next_; // 后向节点 shared_ptr自动释放
    };

    // LRU缓存池
    template <typename KEY, typename VALUE>
    class myLruCache : public myCachePolicy<KEY, VALUE>
    {
    public:
        using LruNodeType = myLruNode<KEY, VALUE>;
        using NodePtr = std::shared_ptr<LruNodeType>;
        using NodeMap = std::unordered_map<KEY, NodePtr>;

        /*
            构造函数
        */
        // 有参构造
        explicit myLruCache(size_t capacity) : capacity_(capacity) { this->lruNodeListInit(); }

        // 析构函数
        virtual ~myLruCache() override = default;

        /*
            成员函数接口
            对该缓存池的操作有：
            1. 当有新加入数据操作时，先判断该 key
           值是否已经在缓存空间中，如果在的话更新 key 对应的 value
           值，并把该数据加入到缓存空间的最右边；
            2. 如果新加入数据的 key
           值不在缓存空间中，则判断缓存空间是否已满，若缓存空间未满，则构造新的节点加入到缓存空间的最右边，否则把该数据加入到缓存空间的右边并淘汰掉队列最左边的数据（缓存中最久未被使用的数据）；
            3.
           访问一个数据且该数据存在于缓存空间中，返回该数据对应值并将该数据放入缓存空间的最右边；
            4. 访问一个数据但该数据不存在于缓存空间中，返回 - 1 表示缓存中无该数据。
        */
        // 添加缓存
        virtual void put(KEY key, VALUE value) override
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
        virtual bool get(KEY key, VALUE &value) override
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
        virtual VALUE get(KEY key) override
        {
            VALUE value{};
            this->get(key, value);
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

        // 清除缓存
        void clear()
        {
            std::lock_guard<std::mutex> lock(mutex_);
            nodeMap_.clear();
            head_->next_ = tail_;
            tail_->prev_ = head_;
        }

#ifdef DEBUG
        // 测试代码，打印主缓存
        virtual void printCache()
        {
            for (const auto &pair : nodeMap_)
            {
                std::cout << "Key: " << pair.first << ", Value: " << pair.second->getValue() << std::endl;
            }
        }
#endif

    private:
        /*
            私有成员函数方法
        */
        // 链表节点初始化
        void lruNodeListInit()
        {
            head_ = std::make_shared<LruNodeType>(KEY(), VALUE());
            tail_ = std::make_shared<LruNodeType>(KEY(), VALUE());
            head_->next_ = tail_;
            tail_->prev_ = head_;
        }

        // 更新节点的value
        void updataLruNode(NodePtr node, const VALUE &value)
        {
            // 1. 更新值
            node->setValue(value);
            // 2. 将该节点移动至末尾
            removeToRecent(node);
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
            if (!node->prev_.expired() && node->next_)
            {
                auto prev = node->prev_.lock(); // 获取shared_ptr
                prev->next_ = node->next_;      // 更新前一个节点的next指针
                node->next_->prev_ = prev;      // 更新后一个节点的prev指针
                node->next_ = nullptr;          // 清空next_指针，彻底断开节点与链表的连接,prev_
                                                // 是 weak_ptr，不需要手动重置
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
            NodePtr node = this->head_->next_;
            this->removeNode(node);
            this->nodeMap_.erase(node->getKey());
        }

        size_t capacity_;  // 缓存容量
        NodeMap nodeMap_;  // 哈希表，便于快速查找节点
        std::mutex mutex_; // 互斥锁
        NodePtr head_;     // 虚拟头结点
        NodePtr tail_;     // 虚拟尾结点
    };

    /*
        myKLruCache
    */
    template <typename KEY, typename VALUE>
    class myKLruCache : public myLruCache<KEY, VALUE>
    {
    public:
        /*
            构造函数
        */

        // 有参构造函数
        myKLruCache(size_t capacity, size_t historyCapacity, size_t k)
            : myLruCache<KEY, VALUE>(capacity), historyList_(std::make_unique<myLruCache<KEY, size_t>>(historyCapacity)), k_(k) {}

        /*
            成员函数接口
        */
        virtual VALUE get(KEY key) override
        {
            // 1. 先尝试从主缓存找
            VALUE value{};
            bool inMainCache = myLruCache<KEY, VALUE>::get(key, value);

            // 2. 如果数据在主缓存中，直接返回
            if (inMainCache)
            {
                return value;
            }

            // 3. 如果不在主缓存，获取并更新访问历史计数
            size_t historyCount = historyList_->get(key);
            historyCount++;
            historyList_->put(key, historyCount);

            // 4. 如果数据不在主缓存，但访问次数达到了k次
            if (historyCount >= this->k_)
            {
                // 判断是否有历史值
                auto it = historyValueMap_.find(key);
                if (it != historyValueMap_.end())
                {
                    // 获取历史值
                    VALUE storedValue = it->second;
                    // 从历史记录移除
                    historyList_->remove(key);
                    historyValueMap_.erase(it);

                    // 将其添加到主缓存
                    myLruCache<KEY, VALUE>::put(key, storedValue);

                    return storedValue;
                }
                // 如果没有历史值，无法添加到缓存，返回默认值
            }

            return value;
        }

        virtual void put(KEY key, VALUE value) override
        {
            // 1. 如果已经在主缓存则更新value
            VALUE isExistingValue{}; // 临时值
            bool isMainCache = myLruCache<KEY, VALUE>::get(key, isExistingValue);

            if (isMainCache)
            {
                myLruCache<KEY, VALUE>::put(key, value);
                return;
            }

            // 2. 如果不在主缓存，检查k+1后是否达到要求，k没有达到要求不添加到主缓存
            size_t historyCount = historyList_->get(key);
            historyCount++;
            historyList_->put(key, historyCount); // 更新位置至队首

            historyValueMap_[key] = value; // 更新value

            // 3. k+1达到要求，则添加到主缓存
            if (historyCount >= k_)
            {
                historyList_->remove(key);
                historyValueMap_.erase(key);
                myLruCache<KEY, VALUE>::put(key, value); // 添加到主缓存
            }
        }

        void clear()
        {
            historyList_->clear();
            historyValueMap_.clear();
        }

#ifdef DEBUG
        // 测试代码，打印历史缓存内容和缓存次数
        virtual void printCache() override
        {
            std::cout << "History Cache Contents (Key-Value pairs):" << std::endl;
            historyList_->printCache();
            // 打印主缓存内容
            std::cout << "Main Cache Contents:" << std::endl;
            myLruCache<KEY, VALUE>::printCache();
        }
#endif

    private:
        size_t k_;                                             // 进入缓存队列的评判标准
        std::unique_ptr<myLruCache<KEY, size_t>> historyList_; // 访问数据历史记录(value为访问次数)
        std::unordered_map<KEY, VALUE> historyValueMap_;       // 存储未达到k次访问的数据值
    };

    /*
        对LRUCache进行分片处理，避免高并发情况下，同步的时间等待
    */
    template <typename KEY, typename VALUE>
    class myKHashLruCache
    {
        typedef std::unique_ptr<myKLruCache<KEY, VALUE>> myKLruCachePtr;

    public:
        /*
            构造函数
        */
        myKHashLruCache(size_t capacity, size_t sliceNumber)
            : capacity_(capacity), sliceNumber_(sliceNumber > 0 ? sliceNumber : std::thread::hardware_concurrency())
        {
            // 构造sliceCache在堆区
            // 获取每个分区的容量
            size_t sliceSize = std::ceil(static_cast<double>(capacity_) / static_cast<double>(sliceNumber_));
            // 循环构建每个分片
            for (int i = 0; i < sliceSize; ++i)
            {
                lruSliceCache_.emplace_back(std::make_unique<myLruCache<KEY, VALUE>>(sliceSize))
            }
        }

        /*
            成员函数接口
        */
        void put(KEY key, VALUE value)
        {
            size_t hashKey = hashFunction(key) % sliceNumber_;
            lruSliceCache_[hashKey]->put(key, value);
        }

        bool get(KEY key, VALUE &value)
        {
            size_t hashKey = hashFunction(key) % sliceNumber_;
            return lruSliceCache_[hashKey]->get(key, value);
        }

        VALUE get(KEY key)
        {
            VALUE value{};
            get(key, value);
            return value;
        }

        void clear()
        {
            for (auto &lruSliceItem : lruSliceCache_)
            {
                lruSliceItem->clear();
            }
        }

    private:
        size_t hashFunction(KEY key)
        {
            std::hash<KEY> hashFunc;
            return hashFunc(key);
        }

        size_t capacity_;                           // LruCache总容量
        size_t sliceNumber_;                        // 分片数量
        std::vector<myKLruCachePtr> lruSliceCache_; // 存储LruCache的容器
    };

} // namespace myCacheSystem

#endif // MYLRU_H