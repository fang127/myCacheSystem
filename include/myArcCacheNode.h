#ifndef MYARCCACHENODE_H
#define MYARCCACHENODE_H

#include <memory>

namespace myCacheSystem
{
    // 前向声明
    template <typename KEY, typename VALUE>
    class myArcLruCachePart;

    template <typename KEY, typename VALUE>
    class myArcLfuCachePart;

    /*
        节点
    */
    template <typename KEY, typename VALUE>
    class myArcCacheNode
    {
        friend myArcLruCachePart<KEY, VALUE>;
        friend myArcLfuCachePart<KEY, VALUE>;

    public:
        /*
            构造函数
        */
        // 默认构造
        myArcCacheNode() : accessCount_(1), next_(nullptr) {}

        // 有参构造
        myArcCacheNode(KEY key, VALUE value)
            : key_(key), value_(value), accessCount_(1), next_(nullptr)
        {
        }

        /*
            成员函数
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

        size_t getAccessCount() const
        {
            return accessCount_;
        }

        void addAccessCount()
        {
            ++accessCount_;
        }

    private:
        KEY key_;
        VALUE value_;
        size_t accessCount_; // 访问次数
        std::shared_ptr<myArcCacheNode> next_;
        std::weak_ptr<myArcCacheNode> prev_;
    };
}

#endif // MYARCCACHENODE_H