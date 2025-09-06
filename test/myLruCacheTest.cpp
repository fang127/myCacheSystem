#include <iostream>
#define DEBUG
#include <string>
#include "myCachePolicy.h"
#include "myLru.hpp"

int main()
{
    // 测试 myLruCache
    myCacheSystem::myLruCache<int, std::string> lruCache(3);
    lruCache.put(1, "one");
    lruCache.put(2, "two");
    lruCache.put(3, "three");
    std::string value;
    if (lruCache.get(2, value))
    {
        std::cout << "Key 2: " << value << std::endl; // 应输出 "two"
    }
    else
    {
        std::cout << "Key 2 not found" << std::endl;
    }
    lruCache.put(4, "four"); // 这将淘汰 key 1
    if (lruCache.get(1, value))
    {
        std::cout << "Key 1: " << value << std::endl;
    }
    else
    {
        std::cout << "Key 1 not found" << std::endl; // 应输出 "Key 1 not found"
    }
    // 打印此时的主缓存内容
#ifdef DEBUG
    lruCache.printCache();
#endif
    std::cout << "------------------------" << std::endl;
    // 测试 myKLruCache
    myCacheSystem::myKLruCache<int, std::string> kLruCache(3, 3, 2); // 容量3，访问2次后进入缓存
    kLruCache.put(1, "one");
    kLruCache.put(2, "two");
    kLruCache.put(1, "one_updated"); // 访问1次
    kLruCache.put(3, "three");
    kLruCache.put(2, "two_updated"); // 访问2次，2进入缓存
    kLruCache.put(4, "four");
    kLruCache.put(1, "one_updated_again"); // 访问2次，1进入缓存
    kLruCache.put(5, "five");              // 5访问1次
    kLruCache.put(6, "six");               // 6访问1次

    std::cout << "Get key 1: " << kLruCache.get(1) << std::endl; // 应该返回 "one_updated_again"
    std::cout << "Get key 2: " << kLruCache.get(2) << std::endl; // 应该返回 "two_updated"
    std::cout << "Get key 3: " << kLruCache.get(3) << std::endl; // 应该返回 ""
    std::cout << "Get key 4: " << kLruCache.get(4) << std::endl; // 应该返回 ""

    // 打印此时的历史缓存内容和主缓存内容
#ifdef DEBUG
    kLruCache.printCache();
#endif

    return 0;
}