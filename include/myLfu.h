#ifndef MYLFU_HPP
#define MYLFU_HPP

#include <cmath>
#include <memory>
#include <mutex>
#include <vector>
#include <unordered_map>
#include <thread>

namespace myCacheSystem
{
    // 前向声明
    template <typename KEY, typename VALUE>
    class FreqList;

    template <typename KEY, typename VALUE>
    class myLfuCache;

    template <typename KEY, typename VALUE>
    class myLfuNode
    {
        friend class myLfuCache<KEY, VALUE>;
        friend class FreqList<KEY, VALUE>;

    public:
        /*
            构造函数
        */
        // 默认构造
        myLfuNode() : accessSize_(1), next_(nullptr) {};
        // 有参构造
        myLfuNode(KEY key, VALUE value) : key_(key), value_(value), accessSize_(1), next_(nullptr) {}

        /*
            成员函数接口
        */
        VALUE getValue() const
        {
            return value_;
        }

        KEY getKey() const
        {
            return key_;
        }

        void setValue(const VALUE &value)
        {
            value_ = value;
        }

        void addAccessSize()
        {
            accessSize_++;
        }

        size_t getAccessSize()
        {
            return accessSize_;
        }

        void setAccessSize(size_t accessSize)
        {
            if (accessSize < 1)
            {
                accessSize_ = 1;
                return;
            }
            accessSize_ = accessSize;
        }

    private:
        size_t accessSize_; // 访问次数
        KEY key_;
        VALUE value_;
        std::shared_ptr<myLfuNode<KEY, VALUE>> next_;
        std::weak_ptr<myLfuNode<KEY, VALUE>> prev_;
    };

    /*
        频率链表，访问频次key对应的相同频次的结点
    */
    template <typename KEY, typename VALUE>
    class FreqList
    {
    public:
        typedef std::shared_ptr<myLfuNode<KEY, VALUE>> NodePtr;
        /*
            构造函数
        */
        // 显式构造函数，避免变成转换构造函数
        explicit FreqList(size_t freq) : freq_(freq)
        {
            initNodeList();
        }

        /*
            成员函数方法
        */
        // 增加结点
        void addLfuNode(NodePtr node)
        {
            if (!node)
                return;

            auto prev = tail_->prev_.lock(); // 获取最后一个结点
            prev->next_ = node;
            node->next_ = tail_;
            tail_->prev_ = node;
            node->prev_ = prev;
        }

        // 移除结点
        void removeLfuNode(NodePtr node)
        {
            if (!node || !head_ || !tail_)
                return;

            if (node->prev_.expired() || !node->next_)
                return;

            auto prev = node->prev_.lock();
            prev->next_ = node->next_;
            node->next_->prev_ = prev;
            node->next_ = nullptr;
        }

        // 判断是否为空
        bool isEmpty() const
        {
            return head_->next_ == tail_;
        }

        // 获取首部结点(同频次优先删除最先添加的，即首结点)
        NodePtr getFirstNode() const
        {
            return head_->next_;
        }

    private:
        /*
            私有成员函数方法
        */
        void initNodeList();

        size_t freq_;  // 访问次数
        NodePtr head_; // 虚拟头节点
        NodePtr tail_; // 虚拟尾结点
    };

    template <typename KEY, typename VALUE>
    void FreqList<KEY, VALUE>::initNodeList()
    {
        head_ = std::make_shared<myLfuNode<KEY, VALUE>>();
        tail_ = std::make_shared<myLfuNode<KEY, VALUE>>();
        head_->next_ = tail_;
        tail_->prev_ = head_;
    }

    /*
        基础的LFU实现
    */
    template <typename KEY, typename VALUE>
    class myLfuCache : public myCachePolicy<KEY, VALUE>
    {
    public:
        typedef myLfuNode<KEY, VALUE> LfuNodeType;
        typedef std::shared_ptr<LfuNodeType> NodePrt;
        typedef std::unordered_map<KEY, NodePrt> NodeMap;

        /*
            构造函数
        */
        myLfuCache(size_t capacity, size_t maxAverageNum = 1000000)
            : capacity_(capacity), minFreq_(INT8_MAX), maxAverageNum_(maxAverageNum), curAverageNum_(0), curTotalNum_(0) {}

        ~myLfuCache() override = default;

        /*
            成员函数接口
        */
        // 添加缓存
        virtual void put(KEY key, VALUE value) override
        {
            // 1. 检查capacity是否足够
            if (capacity_ <= 0)
                return;

            // 2. 查看是否已经在缓存中，如果已经在，则更新value已经访问次数
            std::lock_guard<std::mutex> lock(mutex_); // 加互斥锁
            auto it = LfuMap_.find(key);
            if (it != LfuMap_.end())
            {
                it->second->setValue(value); // 重置值
                // 访问次数加一，同时需要移动结点到相应的FreqList中
                getInternal(it->second, value);
                return;
            }

            // 3. 如果不在则添加至缓存池
            putInternal(key, value);
        }

        // 获取value
        virtual bool get(KEY key, VALUE &value) override
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = LfuMap_.find(key); // 获取节点
            if (it != LfuMap_.end())
            {
                getInternal(it->second, value);
                return true;
            }

            return false;
        }

        // 访问缓存数据函数
        virtual VALUE get(KEY key) override
        {
            VALUE value{};
            get(key, value);
            return value;
        }

        // 清空缓存，回收资源
        void clear()
        {
            std::lock_guard<std::mutex> lock(mutex_);
            LfuMap_.clear();
            keyToFreqList_.clear();
        }

    private:
        /*
            私有函数方法
        */
        // 获取缓存
        void getInternal(NodePrt node, VALUE &value);

        // 添加缓存
        void putInternal(KEY key, const VALUE &value);

        // 从对应freq的链表移除
        void removeFromFreqList(NodePrt node);

        // 增加到对应freq的链表
        void addToFreqList(NodePrt node);

        // 增加当前平均访问频次和访问总次数
        void addAccessFreq();

        // 执行算法减少当前所有结点的访问次数
        void handleOverMaxAverageNum();

        // 关键算法，删除最少使用节点
        void removeForLfu();

        // 删除节点后减少当前平均访问频次和访问总次数
        void decreaseFreqNum(size_t freq);

        // 更新最小频率
        void updateMinFreq();

        size_t capacity_;                                                                 // 容量大小
        size_t minFreq_;                                                                  // 最小访问频次(用于找到最小访问频次结点)
        size_t maxAverageNum_;                                                            // 最大平均访问频次(当平均访问次数大于此值，则全部结点的访问频次按照一定的算法同时缩减)
        size_t curAverageNum_;                                                            // 当前平均访问频次
        size_t curTotalNum_;                                                              // 当前访问所有缓存次数总数
        std::mutex mutex_;                                                                // 互斥锁
        NodeMap LfuMap_;                                                                  // key——结点映射
        std::unordered_map<size_t, std::unique_ptr<FreqList<KEY, VALUE>>> keyToFreqList_; //  访问频次-链表
    };

    template <typename KEY, typename VALUE>
    void myLfuCache<KEY, VALUE>::getInternal(NodePrt node, VALUE &value)
    {
        /*
            把该节点从当前频次链表中删除，并且移向频次+1的链表中
        */
        value = node->getValue();
        // 从原有链表删除
        removeFromFreqList(node);
        // 访问频次+1
        node->addAccessSize();
        // 添加到对应访问次数的链表
        addToFreqList(node);
        /*
            结点移动之后，要注意如果当前node的访问频次如果等于minFreq+1，并且其前驱链表为空，
            则说明keyToFreqList_[node->freq - 1]链表因node的迁移已经空了，需要更新最小访问频次,
            同时当前平均访问频次和当前访问所有缓存次数总数都需要更新
        */
        if (node->getAccessSize() - 1 == minFreq_ && keyToFreqList_[node->getAccessSize() - 1]->isEmpty())
        {
            ++minFreq_;
        }
        addAccessFreq();
    }

    template <typename KEY, typename VALUE>
    void myLfuCache<KEY, VALUE>::putInternal(KEY key, const VALUE &value)
    {
        // 如果当前缓存已满则删除最少访问的节点，如果有多个最少访问的节点，则删除最少访问中最近最少使用节点
        if (LfuMap_.size() == capacity_)
        {
            removeForLfu();
        }
        // 添加新节点
        NodePrt node = std::make_shared<LfuNodeType>(key, value);
        // 更新LfuMap
        LfuMap_[key] = node;
        // 更新key-频次链表
        addToFreqList(node);
        // 更新访问次数
        addAccessFreq();
        // 更新最小访问次数
        minFreq_ = std::min(minFreq_, static_cast<size_t>(1));
    }

    template <typename KEY, typename VALUE>
    void myLfuCache<KEY, VALUE>::removeFromFreqList(NodePrt node)
    {
        if (!node)
            return;

        auto freq = node->getAccessSize();   // 获取访问频次
        auto it = keyToFreqList_.find(freq); // 找到链表
        if (it != keyToFreqList_.end())
            it->second->removeLfuNode(node);
    }

    template <typename KEY, typename VALUE>
    void myLfuCache<KEY, VALUE>::addToFreqList(NodePrt node)
    {
        if (!node)
            return;

        auto freq = node->getAccessSize(); // 获取访问频次

        auto it = keyToFreqList_.find(freq); // 找到链表
        // 当前key没有对应的值，所以应该先创建
        if (it == keyToFreqList_.end())
        {
            keyToFreqList_.emplace(freq, std::make_unique<FreqList<KEY, VALUE>>(freq));
        }

        keyToFreqList_[freq]->addLfuNode(node);
    }

    template <typename KEY, typename VALUE>
    void myLfuCache<KEY, VALUE>::addAccessFreq()
    {
        ++curTotalNum_; // 总访问次数加一
        if (LfuMap_.empty())
        {
            curAverageNum_ = 0;
        }
        else
        {
            curAverageNum_ = curTotalNum_ / LfuMap_.size(); // 更新平均访问次数
        }
        // 如果此时平均访问次数大于最大平均访问次数执行算法减少所有结点的访问次数
        if (curAverageNum_ > maxAverageNum_)
        {
            handleOverMaxAverageNum();
        }
    }

    template <typename KEY, typename VALUE>
    void myLfuCache<KEY, VALUE>::handleOverMaxAverageNum()
    {
        if (LfuMap_.empty())
        {
            return;
        }

        // 当前平均访问频次已经超过了最大平均访问频次，所有结点的访问频次- (maxAverageNum_ / 2)
        for (auto it = LfuMap_.begin(); it != LfuMap_.end(); ++it)
        {
            // 检查结点是否为空
            if (!it->second)
                continue;

            NodePrt node = it->second;

            // 从当前列表移除
            removeFromFreqList(node);

            // 减少频率
            auto freq = node->getAccessSize();
            freq -= maxAverageNum_ / 2;
            node->setAccessSize(freq);

            // 添加节点到对应的链表
            addToFreqList(node);
        }

        // 更新最小频率
        updateMinFreq();
    }

    template <typename KEY, typename VALUE>
    void myLfuCache<KEY, VALUE>::removeForLfu()
    {
        // 找到最少频次链表
        auto it = keyToFreqList_.find(minFreq_);
        // 找到该节点
        NodePrt node = it->second->getFirstNode();
        // 更新key-频次链表
        removeFromFreqList(node);
        // 更新key-node
        LfuMap_.erase(node->getKey());
        // 更新频次
        decreaseFreqNum(node->getAccessSize());
    }

    template <typename KEY, typename VALUE>
    void myLfuCache<KEY, VALUE>::decreaseFreqNum(size_t freq)
    {
        curTotalNum_ -= freq;
        if (LfuMap_.empty())
        {
            curAverageNum_ = 0;
        }
        else
        {
            curAverageNum_ = curTotalNum_ / LfuMap_.size();
        }
    }

    template <typename KEY, typename VALUE>
    void myLfuCache<KEY, VALUE>::updateMinFreq()
    {
        minFreq_ = INT8_MAX;
        // 循环查找key-频次链表
        for (const auto &pair : keyToFreqList_)
        {
            if (pair.second && !pair.second->isEmpty())
            {
                minFreq_ = std::min(minFreq_, pair.first);
            }
        }
        if (minFreq_ == INT8_MAX)
            minFreq_ = 1;
    }

    /*
        myhashLfuCache
    */
    template <typename KEY, typename VALUE>
    class myHashLfuCache
    {
    public:
        /*
            构造函数
        */
        myHashLfuCache(size_t capacity, size_t sliceNum, size_t maxAverageNum = 10)
            : capacity_(capacity), sliceNumber_(sliceNum > 0 ? sliceNum : std::thread::hardware_concurrency())
        {
            // 计算每个分片capacity
            size_t sliceSize = std::ceil(static_cast<double>(capacity_) / static_cast<double>(sliceNumber_));
            // 创建多个lfuCache
            for (int i = 0; i < sliceNumber_; ++i)
            {
                lfuSliceCache_.emplace_back(std::make_unique<myLfuCache<KEY, VALUE>>(sliceSize, maxAverageNum));
            }
        }

        /*
            成员函数接口
        */
        void put(KEY key, VALUE value)
        {
            // 计算key对应的hash值
            size_t hashKey = hashFunction(key) % sliceNumber_;
            // 调用相应的缓存池put
            lfuSliceCache_[hashKey]->put(key, value);
        }

        bool get(KEY key, VALUE &value)
        {
            size_t hashKey = hashFunction(key) % sliceNumber_;
            return lfuSliceCache_[hashKey]->get(key, value);
        }

        VALUE get(KEY key)
        {
            VALUE value{};
            get(key, value);
            return value;
        }

        void clear()
        {
            for (auto &lfuSliceItem : lfuSliceCache_)
            {
                lfuSliceItem->clear();
            }
        }

    private:
        size_t hashFunction(KEY key) const
        {
            std::hash<KEY> hashFunc;
            return hashFunc(key);
        }

        size_t capacity_;                                                    // 总容量
        size_t sliceNumber_;                                                 // 分片数量
        std::vector<std::unique_ptr<myLfuCache<KEY, VALUE>>> lfuSliceCache_; // 分片容器
    };
}

#endif // MYLFU_HPP