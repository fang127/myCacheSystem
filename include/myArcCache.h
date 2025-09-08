#ifndef MYARCCACHED_H
#define MYARCCACHED_H

#include <memory>
#include "myCachePolicy.h"
#include "myArcLruCachePart.h"
#include "myArcLfuCachePart.h"

namespace myCacheSystem
{
    template <typename KEY, typename VALUE>
    class myArcCache : public myCachePolicy<KEY, VALUE>
    {
    public:
        /*
            构造函数
        */
        explicit myArcCache(size_t capacity, size_t transformThreshold = 3)
            : capacity_(capacity), transformThreshold_(transformThreshold), lruPart_(std::make_unique<myArcLruCachePart<KEY, VALUE>>(capacity_, transformThreshold_)), lfuPart_(std::make_unique<myArcLruCachePart<KEY, VALUE>>(capacity_, transformThreshold_))
        {
        }

        ~myArcCache() override = default;

        /*
            成员函数
        */
        // 添加缓存
        virtual void put(KEY key, VALUE value) override
        {
            checkGhostCaches(key);

            // 检查LFU缓存是否存在key
            bool inLfu = lfuPart_->contain(key);
            // 更新LRU部分缓存
            lruPart_->put(key, value);
        }

        // 获取value
        virtual bool get(KEY key, VALUE &value) override
        {
        }

        // 访问缓存数据函数
        virtual VALUE get(KEY key) override
        {
        }

    private:
        bool checkGhostCaches(KEY key);

        size_t capacity_;                                        // 总容量
        size_t transformThreshold_;                              // 转移阈值
        std::unique_ptr<myArcLruCachePart<KEY, VALUE>> lruPart_; // lru缓存池
        std::unique_ptr<myArcLfuCachePart<KEY, VALUE>> lfuPart_; // lfu缓存池
    };

    template <typename KEY, typename VALUE>
    bool myArcCache<KEY, VALUE>::checkGhostCaches(KEY key)
    {
        bool isInGhost = false;
        if (lruPart_->checkGhost(key))
        {
            if (lruPart_->decreaseCapacity())
            {
                lfuPart_->increaseCapacity();
            }
            isInGhost = true;
        }
        else if (lfuPart_->checkGhost(key))
        {
            if (lfuPart_->decreaseCapacity())
            {
                lruPart_->increaseCapacity();
            }
            isInGhost = true;
        }
        return isInGhost;
    }
}

#endif // MYARCCACHED_H