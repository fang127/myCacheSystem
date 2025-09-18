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
void printResult(const std::string &message, const std::vector<std::string> &names, int capacity, const std::vector<int> &hits, const std::vector<int> &get_operations)
{
    std::cout << "=== " << message << "结果汇总 ===" << std::endl;
    std::cout << "缓存大小: " << capacity << std::endl;

    for (size_t i = 0; i < hits.size(); ++i)
    {
        double hitRate = 100.0 * hits[i] / get_operations[i];
        std::cout << names[i]
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
    printResult("热点数据访问测试", names, CAPACITY, hits, get_operations);
}

// 测试循环扫描场景
void testLoopPattern()
{
    std::cout << "\n=== 测试场景2：循环扫描测试 ===" << std::endl;

    // 初始化参数
    const int CAPACITY = 50;       // 缓存容量
    const int LOOP_SIZE = 500;     // 循环次数
    const int OPERATIONS = 200000; // 总操作次数

    // 初始化缓存
    myCacheSystem::myLruCache<int, std::string> lru(CAPACITY);
    myCacheSystem::myLfuCache<int, std::string> lfu(CAPACITY);
    myCacheSystem::myArcCache<int, std::string> arc(CAPACITY);
    myCacheSystem::myKLruCache<int, std::string> klru(CAPACITY, LOOP_SIZE * 2, 2);
    myCacheSystem::myLfuCache<int, std::string> lfuAging(CAPACITY, 3000);
    std::array<myCacheSystem::myCachePolicy<int, std::string> *, 5> caches = {&lru, &lfu, &arc, &klru, &lfuAging};

    // 设置结果数据
    std::vector<int> hits(5, 0); // 保存命中数
    std::vector<std::string> names = {"LRU", "LFU", "ARC", "LRU-K", "LFU-Aging"};
    std::vector<int> get_operations(5, 0); // 保存访问缓存操作数

    // 随机数分布器
    std::random_device rd;
    std::mt19937 gen(rd());

    // 开始测试
    for (int i = 0; i < caches.size(); ++i)
    {
        // 预热缓存
        for (int key = 0; key < LOOP_SIZE / 5; ++key)
        {
            std::string value = "loop" + std::to_string(key);
            caches[i]->put(key, value);
        }

        int current_pos = 0; // 设置循环扫描位置

        // 交替读写进行操作，循环LOOP_SIZE次
        for (int op = 0; op < OPERATIONS; ++op)
        {
            // 判断是get还是put操作
            bool isPut = (gen() % 100 < 20); // put操作占比20%，get占比80%
            int key;

            // 60%的概率循环扫描，30%随机跳跃，10%访问范围外的数据
            if (op % 100 < 60)
            {
                key = current_pos;
                current_pos = (current_pos + 1) % LOOP_SIZE; // 加一
            }
            else if (op % 100 < 90)
            {
                key = gen() % LOOP_SIZE;
            }
            else
            {
                key = LOOP_SIZE + (gen() % LOOP_SIZE);
            }

            if (isPut)
            {
                std::string value = "loop" + std::to_string(key) + "_v" + std::to_string(op % 100);
                caches[i]->put(key, value);
            }
            else
            {
                std::string result;
                get_operations[i]++;
                if (caches[i]->get(key, result))
                {
                    hits[i]++;
                }
            }
        }
    }

    printResult("循环扫描测试", names, CAPACITY, hits, get_operations);
}

// 测试不同的工作负载
void testWorkLoadShift()
{
    std::cout << "\n=== 测试场景3：工作负载剧烈变化测试 ===" << std::endl;

    const int CAPACITY = 30;
    const int OPERATIONS = 80000;
    const int PHASE_OPERATIONS = OPERATIONS / 5;

    // 初始化缓存
    myCacheSystem::myLruCache<int, std::string> lru(CAPACITY);
    myCacheSystem::myLfuCache<int, std::string> lfu(CAPACITY);
    myCacheSystem::myArcCache<int, std::string> arc(CAPACITY);
    myCacheSystem::myKLruCache<int, std::string> klru(CAPACITY, 500, 2);
    myCacheSystem::myLfuCache<int, std::string> lfuAging(CAPACITY, 10000);
    std::array<myCacheSystem::myCachePolicy<int, std::string> *, 5> caches = {&lru, &lfu, &arc, &klru, &lfuAging};

    // 设置结果数据
    std::vector<int> hits(5, 0); // 保存命中数
    std::vector<std::string> names = {"LRU", "LFU", "ARC", "LRU-K", "LFU-Aging"};
    std::vector<int> get_operations(5, 0); // 保存访问缓存操作数

    // 随机数分布器
    std::random_device rd;
    std::mt19937 gen(rd());

    for (int i = 0; i < caches.size(); ++i)
    {
        // 预热数据
        for (int key = 0; key < 30; ++key)
        {
            std::string value = "init" + std::to_string(key);
            caches[i]->put(key, value);
        }

        // 多阶段测试
        // 阶段1 热点访问，15%写入。
        // 阶段2 大范围随机，写比例30 %。
        // 阶段3 顺序扫描，10 % 写入。
        // 阶段4 局部性随机，微调为25 %。
        // 阶段5 混合访问，调整为20 %。
        for (int op = 0; op < OPERATIONS; ++op)
        {
            int phase = op / PHASE_OPERATIONS; // 确认是第几阶段

            // 确认put的概率
            int isPutNum;
            switch (phase)
            {
            case 0:
                isPutNum = 15;
                break;
            case 1:
                isPutNum = 30;
                break;
            case 2:
                isPutNum = 10;
                break;
            case 3:
                isPutNum = 25;
                break;
            default:
                isPutNum = 20;
            }
            bool isPut = (gen() % 100 < isPutNum);

            // 设置key
            int key;
            if (op < PHASE_OPERATIONS)
            {
                key = gen() % 5; // 热点数据，5个
            }
            else if (op < PHASE_OPERATIONS * 2)
            {
                key = gen() % 400; // 大范围随机,400个
            }
            else if (op < PHASE_OPERATIONS * 3)
            {
                key = (op - PHASE_OPERATIONS * 2) % 100; // 顺序访问，100个
            }
            else if (op < PHASE_OPERATIONS * 4)
            {
                int locality = (op / 800) % 5;      // 调整5个局部区域
                key = locality * 15 + (gen() % 15); // 每区域15个
            }
            else
            {
                int r = gen() % 100;
                // 40%访问热点
                if (r < 40)
                {
                    key = gen() % 5;
                }
                // 30%访问中等
                else if (r < 70)
                {
                    key = 5 + (gen() % 45);
                }
                // 30%访问大范围
                else
                {
                    key = 50 + (gen() % 350);
                }
            }

            if (isPut)
            {
                std::string value = "value" + std::to_string(key) + "_p" + std::to_string(phase);
                caches[i]->put(key, value);
            }
            else
            {
                std::string result;
                get_operations[i]++;
                if (caches[i]->get(key, result))
                {
                    hits[i]++;
                }
            }
        }
    }
    printResult("工作负载剧烈变化测试", names, CAPACITY, hits, get_operations);
}

int main()
{
    testHotData();
    testLoopPattern();
    testWorkLoadShift();

    return 0;
}