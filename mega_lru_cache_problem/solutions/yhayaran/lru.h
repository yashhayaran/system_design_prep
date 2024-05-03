
#include <string>
#include <functional>
#include <chrono>
#include <map>
#include <list>
#include <memory>
#include <mutex>
#include <future>
#include <vector>
#include <deque>
#include <set>
#include <sstream>
#include <random>
#include <chrono>

#include <assert.h>


class LRUCleanable
{
public:
    virtual ~LRUCleanable(){}

    /**
     * @brief cleanup
     * @return the size cleaned
     */
    void virtual cleanup() = 0; //remove any memory asociated resource

};

template <typename T, typename PK/*primary_key*/>
class LRUCacheElement
{
private:
    int64_t mLastAccessTime = 0;
    int64_t mSize = 0;
    std::weak_ptr<T> mWeakPointerElement;
    PK mPrimaryKey;
    typename std::list<std::shared_ptr<LRUCacheElement<T,PK>>>::iterator mElementInListItr;

public:
    LRUCacheElement(std::shared_ptr<T> element, PK primaryKey)
        : mWeakPointerElement(element), mPrimaryKey(primaryKey)
    {}
    void updateAccessTime()
    {
        mLastAccessTime = std::time(nullptr);
    }
    void setElementInListItr(const typename std::list<std::shared_ptr<LRUCacheElement<T,PK> > >::iterator &elementInListItr)
    {
        mElementInListItr = elementInListItr;
    }
    typename std::list<std::shared_ptr<LRUCacheElement<T,PK> > >::iterator elementInListItr() const
    {
        return mElementInListItr;
    }

    int64_t size() const
    {
        return mSize;
    }

    void setSize(const int64_t &size)
    {
        mSize = size;
    }

    std::weak_ptr<T> weakPointerElement() const
    {
        return mWeakPointerElement;
    }

    PK primaryKey() const
    {
        return mPrimaryKey;
    }};


/**
 * @brief The LRUCache class
 * This cache uses weak ptr of elements (of LRUCleanable).
 * That is important: it assumes the ownership is elsewhere.
 * It will initiate a cleaning thread that will
 * call cleanup methods of the elements once reached some soft limit
 * , so that they can clean their resources.
 * Also, when adding a new element, if certain hard limit is reached
 * cleanup will be also carried out.
 * Weak pointers failed to be locked will simply be removed from the cache upon cleanup
 */
template <typename T, typename PK/*primary_key*/>
class LRUCache {
    static_assert(std::is_base_of<LRUCleanable, T>::value, "T must derive from LRUCleanable");
private:
    std::list<std::shared_ptr<LRUCacheElement<T,PK>>> mListOfElements; //to keep order
    std::map<PK,std::shared_ptr<LRUCacheElement<T,PK>>> mMapOfElements; //To ease the search
    int64_t mTotalSize = 0;
    int64_t mMaxSizeSoft = 0; //scheduled cleaner will act on this
    int64_t mMaxSizeHard = 0; //cache won't be allowed to exceed this
    std::mutex elementsMutex;

    //cleaning thread stuff
    std::unique_ptr<std::thread> mCleanerThread;
    bool mFinished = false;
    int64_t mCleanScheduleMs;
    std::condition_variable mCleancv;
    std::mutex mCleanMutex;

    void end()
    {
        {
            std::lock_guard<std::mutex> g(mCleanMutex);
            mFinished = true;
        }
        mCleancv.notify_all();
    }

    void loopCleaner()
    {
        while(true)
        {
            std::unique_lock<std::mutex> lk(mCleanMutex);
            if (mCleancv.wait_for(lk,std::chrono::milliseconds(mCleanScheduleMs)) == std::cv_status::timeout)
            {
                cleanup();
            }
            if (mFinished) break;
        }
    }


public:
    ~LRUCache()
    {
        if (mCleanerThread)
        {
            end();
            mCleanerThread->join();
        }
    }

    /**
     * @brief LRUCache Creates a cache of T, with PK being the primary key
     * @param maxSizeSoft Soft limit (bytes): cleaning will limit the size of the cache to this
     * @param maxSizeHard Hard limit (bytes): surpassing this will force a cleaning
     * @param cleanScheduleMs  if received, a thread will be created to do the cleaning every cleanScheduleMs milliseconds
     */
    LRUCache(int64_t maxSizeSoft, int64_t maxSizeHard, int64_t cleanScheduleMs = 0)
        : mMaxSizeSoft(maxSizeSoft), mMaxSizeHard(maxSizeHard), mCleanScheduleMs(cleanScheduleMs)
    {
        if (cleanScheduleMs)
        {
            mCleanerThread.reset(new std::thread([this]()
            {
                this->loopCleaner();
            }
            ));
        }
    }

    void updateElement(std::shared_ptr<T> element, const PK &key, int64_t size)
    {
        {
            std::lock_guard<std::mutex> g(elementsMutex);

            std::shared_ptr<LRUCacheElement<T,PK>> cacheElement;

            auto itrMap = mMapOfElements.find(key);
            if (itrMap == mMapOfElements.end())
            {
                cacheElement = std::make_shared<LRUCacheElement<T,PK>>(element, key);
                mMapOfElements.insert(std::pair<PK,std::shared_ptr<LRUCacheElement<T,PK>>>(key, cacheElement));
            }
            else //remove from list to reorder when inserting
            {
                cacheElement = itrMap->second;
                mListOfElements.erase(cacheElement->elementInListItr());
                mTotalSize -= cacheElement->size();
            }

            cacheElement->setSize(size);
            mTotalSize += size;

            cacheElement->updateAccessTime();

            mListOfElements.push_back(cacheElement);// insert at the back, and save the itr in the element
            cacheElement->setElementInListItr(std::prev(mListOfElements.end()));
        }
        if (mTotalSize > mMaxSizeHard)
        {
            cleanup(&key);
        }
    }


    void removeElement(const PK &key)
    {
        std::lock_guard<std::mutex> g(elementsMutex);

        std::shared_ptr<LRUCacheElement<T, PK>> cacheElement;

        auto itrMap = mMapOfElements.find(key);
        if (itrMap != mMapOfElements.end()) //remove from set to reorder when inserting
        {
            cacheElement = itrMap->second;
            mListOfElements.erase(cacheElement->elementInListItr());
            mTotalSize -= cacheElement->size();

            mMapOfElements.erase(itrMap);
        }
    }

    void cleanup(const PK *keyToSaveFromPurge = nullptr)
    {
        std::vector<std::shared_ptr<LRUCleanable>> toClean;
        {
            std::lock_guard<std::mutex> g(elementsMutex);
            while (mListOfElements.size() &&  mTotalSize > mMaxSizeSoft)
            {
                auto el = mListOfElements.front();
                mListOfElements.pop_front();
                mMapOfElements.erase(el->primaryKey());
                if (!keyToSaveFromPurge || *keyToSaveFromPurge != el->primaryKey())
                {
                    auto weakPointerEl = el->weakPointerElement();
                    auto shrPointerEl = weakPointerEl.lock();
                    if (shrPointerEl)
                    {
                        toClean.push_back(shrPointerEl);
                    }

                    mTotalSize -= el->size();
                }
            }
        }

        for (auto &elementToClean : toClean)
        {
            elementToClean->cleanup();
        }
    }

};

