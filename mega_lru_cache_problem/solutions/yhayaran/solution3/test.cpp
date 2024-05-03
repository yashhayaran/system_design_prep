#include "lru_size_order.h"
#include <iostream>
#include <unistd.h>


class MyElement : public LRUCleanable
{
private:
    std::string mSomeString;
    int mId;
    int64_t mSize = 10;

public:
    MyElement(std::string name, int id) : mSomeString(name), mId(id) {}
    MyElement(std::string name, int id, int64_t size) : mSomeString(name), mId(id), mSize(size)  {}
    ~MyElement(){}
    
    void print(){ std::cout << "Name: " << mSomeString << " ID: " << mId << " Size: " << mSize << std::endl;}
    int id() { return mId; }

    void virtual cleanup()
    {
        // mResource.reset();
        mSize = 0;
        std::cout << "Cleaned: ";
        print();
        mSomeString += " (removed)";
    };
    
    int64_t size()
    {
        return mSize;
    }

    void setSize(int64_t s)
    {
        mSize = s;
    }

};

std::shared_ptr<MyElement> createElement(   const std::string &s,
                                            const int id,
                                            const int64_t size,
                                            LRUCache<MyElement, int> &cache )
{
    auto e = std::make_shared<MyElement>(s, id, size);

    cache.updateElement(e, e->id(), e->size());
    
    return e;
}

std::shared_ptr<MyElement> createElement(   const std::string &s,
                                            const int id,
                                            const int64_t size,
                                            LRUCacheSizeOrder<MyElement, int> &cache )
{
    auto e = std::make_shared<MyElement>(s, id, size);

    cache.updateElement(e, e->id(), e->size());
    
    return e;
}

/**
 * @brief Test to check the old feature.
 * 
 */ 
void test1()
{
    std::vector<std::shared_ptr<MyElement>> elements;
    
    // create LRU cache, soft: 20, hard: 40, 
    LRUCacheSizeOrder<MyElement, int> cache(20, 40, 0, 1);

    elements.push_back(createElement("first element", 1, 10, cache));
    auto second = createElement("second element", 2, 10, cache);
    elements.push_back(second);
    elements.push_back(createElement("third element", 3, 10, cache));
    
    for (auto &e : elements)
    {
        e->print();
    }

    std::cerr << std::endl;
            
    sleep(2);

    for (auto &e : elements)
    {
        e->print();
    }
    std::cerr << std::endl;
    
    
    elements.push_back(createElement("fourth element", 4, 10, cache));
    cache.updateElement(second, second->id() , second->size());
    elements.push_back(createElement("fifth element", 5, 10, cache));
    
    for (auto &e : elements)
    {
        e->print();
    }
    
    sleep(2);
    std::cout << std::endl;

    for (auto &e : elements)
    {
        e->print();
    }
}

/**
 * @brief Test to check new feature.
 * Cache, 
 * hard limit 150 bytes, soft limit 100 bytes
 * threshold 10 secs, cleanup interval 15 secs
 * 
 * Testcase:
 * 
 * Element: Size UpdateTime
 * 
 * A: 30B (1s passed, total 30 Bytes)
 * B: 20B (2s passed, total 50 Bytes)
 * C: 40B (5s passed, total 90 Bytes) (5s -> checkAccessTime(), A)
 * D: 30B (11s passed, total 120 Bytes) (10s ->checkAccessTime(), C, B) 
 * E: 10B (12s passed, total 130 Bytes) (10s -> cleanup(), "Removed: A)
 * F: 50B (13s passed, total 140 Bytes) (10s -> cleanup(), "Removed: C, B, (size wise order) E, F ())
 * D: 70B (14s passed, total 180 Bytes) (updated D) (180 Bytes -> cleanup(), removes in order C, A, B, then E, F)
 * 
 * Pass: If messages comes in same order message with prefix 'Cleaned' comes in order: 
 * Cleaned: Name: A ID: 1 Size: 0 (called becuase of checkAccessTime + cleanup)
 * Cleaned: Name: C ID: 3 Size: 0 (called becuase of checkAccessTime + cleanup)
 * Cleaned: Name: B ID: 2 Size: 0 (called becuase of checkAccessTime + cleanup)
 * Cleaned: Name: E ID: 5 Size: 0 (called becuase of soft limit + cleanup)
 * Cleaned: Name: F ID: 6 Size: 0 (called becuase of soft limit + cleanup)
 */
void test2()
{
    std::vector<std::shared_ptr<MyElement>> elements;
    
    // create LRU cache, soft: 20, hard: 40, 
    LRUCacheSizeOrder<MyElement, int> cache(100, 150, 5, 10);

    sleep(1); // 1s
    elements.push_back(createElement("A", 1, 30, cache));

    sleep(1); // 2s
    elements.push_back(createElement("B", 2, 20, cache));

    sleep(1); // 4s 
    elements.push_back(createElement("C", 3, 40, cache));

    sleep(5); // 11s
    auto elementD = createElement("D", 4, 30, cache);
    elements.push_back(elementD);

    sleep(1); // 12s
    elements.push_back(createElement("E", 5, 10, cache));

    sleep(1); // 13s
    elements.push_back(createElement("F", 6, 50, cache));

    sleep(1); // 14s
    elementD->setSize(70);
    cache.updateElement(elementD, 4, elementD->size());

    for (auto &e : elements)
    {
        e->print();
    }

    std::cout << "Slept for 2 seconds" << std::endl;
    sleep(2);

    for (auto &e : elements)
    {
        e->print();
    }
}

int main()
{
    //test1();
    test2();

    return 0;
}