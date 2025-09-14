#include <iostream>
#include "myLru.h"
#include "myLfu.h"
#include "myArcCache.h"
#include <string>
#include <vector>
#include <random>
#include <chrono>
#include <iomanip>
#include <algorithm>

// 打印结果
void printResult(const std::string &message, int capacity, const std::vector<int> &hits, const std::vector<int> &get_operations)
{
    std::cout << "=== " << message << "结果汇总 ===" << std::endl;
    std::cout << "缓存大小: " << capacity << std::endl;

    std::vector<std::string> names;
    if (hits.size() == 3)
    {
        names = {"LRU", "LFU", "ARC"};
    }
    else if (hits.size() == 4)
    {
        names = {"LRU", "LFU", "ARC", "LRU-K"};
    }
    else if (hits.size() == 5)
    {
        names = {"LRU", "LFU", "ARC", "LRU-K", "LFU-Aging"};
    }

    for (size_t i = 0; i < hits.size(); ++i)
    {
        double hitRate = 100.0 * hits[i] / get_operations[i];
        std::cout << ((i < names.size()) ? names[i] : "Algorithm " + std::to_string(i + 1))
                  << "- 命中率：" << std::fixed << std::setprecision(2)
                  << hitRate << "%";
        // 添加具体命中次数和总操作数
        std::cout << "(" << hits[i] << "/" << get_operations[i] << ")" << std::endl;
    }

    std::cout << std::endl;
}

// 测试热点数据
void testHotData()
{
    std::cout << "=== 测试场景1：热点数据访问测试 ===" << std::endl;

    // 1. 定义缓存容量、热点数据量、非热点数据量
    size_t CAPACITY = 20;
    size_t HOT_KEY = 20;
    size_t COLD_KEY = 5000;
    size_t OPERATIONS = 500000;

    // 2. 创建LRU,LFU,ARC
    myCacheSystem::myLruCache<int, std::string> lru(CAPACITY);
    myCacheSystem::myLfuCache<int, std::string> lfu(CAPACITY);
    myCacheSystem::myArcCache<int, std::string> arc(CAPACITY);
    myCacheSystem::myKLruCache<int, std::string> klru(CAPACITY, HOT_KEY + COLD_KEY, 2);
    myCacheSystem::myLfuCache<int, std::string> lfuAging(CAPACITY, 20000);

    // 3. 定义保存结果的数据结构
    std::array<myCacheSystem::myCachePolicy<int, std::string> *, 5> cache{&lru, &lfu, &arc, &klru, &lfuAging}; // 缓冲池
    std::vector<int> hits(5, 0);                                                                               // 保存缓存命中数
    std::vector<int> get_operations(5, 0);                                                                     // 三种策略测试分别get访问缓存总次数
    std::vector<std::string> names = {"LRU", "LFU", "ARC", "LRU-K", "LFU-Aging"};
    std::random_device rd; // 生成随机数
    std::mt19937 gen(rd());

    // 4. 预热数据
    for (int i = 0; i < cache.size(); ++i)
    {
        // 存入一些数据
        for (int key = 0; key < CAPACITY; ++key)
        {
            std::string value = "value" + std::to_string(key);
            cache[i]->put(key, value);
        }

        // get与put交替，随机数判断，70操作get，30操作put
        for (int j = 0; j < OPERATIONS; ++j)
        {
            bool isPut = gen() % 100 < 30;

            int key = 0;
            // key取随机值，70%是热点，30是冷点
            if (gen() % 100 < 70)
            {
                key = gen() % HOT_KEY;
            }
            else
            {
                key = gen() % COLD_KEY + HOT_KEY;
            }

            if (isPut)
            {
                std::string value = "value" + std::to_string(key) + "_v" + std::to_string(j % 100);
                cache[i]->put(key, value);
            }
            else
            {
                std::string result;
                get_operations[i]++;
                if (cache[i]->get(key, result))
                {
                    hits[i]++;
                }
            }
        }
    }
    printResult("热点数据访问测试", CAPACITY, hits, get_operations);
}

int main()
{
    testHotData();
}