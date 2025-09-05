#ifndef MYCACHEPOLICY_H
#define MYCACHEPOLICY_H

namespace myCacheSystem
{
    /*
        抽象基类，缓存池
    */
    template <typename KEY, typename VALUE>
    class myCachePolicy
    {
    public:
        /*
            构造函数
        */
        virtual ~myCachePolicy() {};

        /*
            纯虚函数接口
        */
        // 添加缓存
        virtual void put(KEY key, VALUE value) = 0;

        // 获取value
        virtual bool get(KEY key, VALUE &value) = 0;

        // 访问缓存数据函数
        virtual VALUE get(KEY key) = 0;
    };
} // namespace KamaCache

#endif // MYCACHEPOLICY_H