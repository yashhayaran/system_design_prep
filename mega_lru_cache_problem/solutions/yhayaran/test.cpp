#include "lru.h"
#include <iostream>
#include <unistd.h>



class MyElement : public LRUCleanable
{
private:
    std::string mSomeString;
    int mId;
    int mSize = 10;
    //std::unique_ptr<SomethingBig> mResource;
public:
    MyElement(std::string name, int id) : mSomeString(name), mId(id) {}
    ~MyElement(){}
    
    void print(){ std::cerr << mSomeString << std::endl;}
    int id() { return mId; }

    void virtual cleanup()
    {
        // mResource.reset();
        mSize = 0;
        mSomeString += " cleaned";
    };
    
    int64_t size()
    {
        return mSize;
    }

};

std::shared_ptr<MyElement> createElement(const std::string &s, LRUCache<MyElement, int> &cache )
{
    static int i = 0;
    auto e = std::make_shared<MyElement>(s, ++i);
    cache.updateElement(e, e->id(), e->size());
    return e;
}


int main() {
    std::vector<std::shared_ptr<MyElement> > elements;
    
    {
        LRUCache<MyElement, int> cache(20, 40, 1000);

        elements.push_back(createElement("first element", cache));
        auto second = createElement("second element", cache);
        elements.push_back(second);
        elements.push_back(createElement("third element", cache));
        
        
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
        
        
        elements.push_back(createElement("forth element", cache));
        cache.updateElement(second, second->id() , second->size());
        elements.push_back(createElement("fifth element", cache));
        
    }
    
    for (auto &e : elements)
    {
        e->print();
    }
    
    return 0;
}
