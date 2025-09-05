#include <iostream>
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
    return 0;
}