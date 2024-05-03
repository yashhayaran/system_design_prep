#include "lru.h"
#include <iostream>

/**
 * @brief LRUCacheSizeOrder is upgraded version of LRUCache, with new functionality of 
 * first removing the elements updated access time before certian threshold in order od size, 
 * e.g. during cleanup, threshold is 1hr.  
 * element A (size 20 bytes) updated 01:10hr before the current time, 
 * element B (size 50 bytes) updated 01:07hr before the current time,
 * element C (size 30 bytes) updated 01:05hr before the current time,
 * need to be remove B first, because of size of B is greatest, then C (even if C is more latest than A),
 * then A. 
 * elements with lesser time than threshold limit, gets removed till soft limit is not met.
 * element D (size 20 bytes) updated 00:40hr before the current time, 
 * element D (size 30 bytes) updated 00:30hr before the current time, 
 * 
 * Strategy:
 * Added new thread, which starts checking all elements in the list of 
 * elements (first element is oldest, last is recent) after every interval of threshold. 
 * if the element's access time - current time >= threshold, then marks for DONT_CHECK_AGAIN and 
 * puts in the map (key: size-primarykey, value: itr to list element)
 * will stop when an element timestamp is less then threshold or have marker DONT_CHECK_AGAIN. 
 * 
 * cleanup():
 * first removes the elements from the new map (key: size-primarykey, value: itr to list element),
 * then removes the elements from the list of elements, till the soft limit is reached.  
 * 
 * @tparam T LRUCleanable
 * @tparam PK Int
 */

template <typename T, typename PK /*primary_key*/>
class LRUCacheSizeOrder
{
    typedef std::shared_ptr<LRUCacheElement<T,PK>> SPTR_CACHE_ELEMENT;
    static_assert(std::is_base_of<LRUCleanable, T>::value, "T must derive from LRUCleanable");

protected:
    /**
     * @brief List to keep the track of elements in order of updated access time, 
     * most recently updated element will be at the end of the list.
     * @type: Shared pointer to the element.
     */
    std::list<SPTR_CACHE_ELEMENT> _listOfElements;
    
    /**
     * @brief Ordered map to easily find the element with correct primary key.
     * @key: Primary Key
     * @value: Shared pointer to the element.
     */
    std::map<PK, SPTR_CACHE_ELEMENT> _mapOfPKWithElement;

    /**
     * @brief Track the total size of cache in bytes.
     */
    int64_t _nTotalSizeOfCache = 0;

    /**
     * @brief Soft limit in bytes, Cleaner Theard will try to remove the elements
     * till total size of becomes lesser or equal to soft limit. 
     */
    int64_t _nSoftLimitInBytes = 0;

    /**
     * @brief Hard limit in bytes, while update or new element in cache, will check the total size. 
     * If total size is greater than hard limit, then cleanup will happen.
     */
    int64_t _nHardLimitInBytes = 0;

    /**
     * @attention new variable 
     * @brief Threshold in seconds for cleanup of elements based on 
     * criteria of decending order of size.
     */
    int64_t _nThresholdInSec = 0;

    /**
     * @brief Mutex for exclusive handling of the elements.
     */
    std::mutex _mutexForElementAccess;

    /**
     * @brief To STOP the cleaner thread.
     */
    bool _bFinished = false;             

    /**
     * @brief Timeout value in seconds for cleaner thread to call cleanup.
     */                               
    int64_t _nCleanScheduleInSec;

    /**
     * @brief Cleaner thread object.
     */    
    std::unique_ptr<std::thread> _CleanerThread;

    /**
     * @brief Acts a event handler.
     */    
    std::condition_variable _CleanerThreadSignal;

    /**
     * @brief Mutex for cleaner thread.
     */  
    std::mutex _CleanerThreadMutex;

    /**
     * @brief SizePKPair to hold the Size <> PK as pair
     * With CompareSizePKPair keeps in order like:
     * 
     * std::map<SizePKPair, char, CompareSizePKPair> mapper;
     * mapper.insert(std::make_pair(SizePKPair(100, 2), 'B'));
     * mapper.insert(std::make_pair(SizePKPair(100, 1), 'A'));
     * mapper.insert(std::make_pair(SizePKPair(200, 3), 'C'));
     * mapper.insert(std::make_pair(SizePKPair(50, 4), 'D'));
     * mapper.insert(std::make_pair(SizePKPair(100, 5), 'E'));
     * Output:
     * Size: 200 PK: 3 Element: C
     * Size: 100 PK: 1 Element: A
     * Size: 100 PK: 2 Element: B
     * Size: 100 PK: 5 Element: E
     * Size: 50 PK: 4 Element: D
     */
    struct SizePKPair
    {
        int64_t nSize = 0;
        int64_t nPrimaryKey = 0;
        SizePKPair(const int64_t& size, const int64_t& pk) : nSize(size), nPrimaryKey(pk) {};
    };
    struct CompareSizePKPair{
    bool operator()(const SizePKPair& x, const SizePKPair& y) const 
        {
            if (x.nSize > y.nSize)
            {
                return true;
            }
            else if(x.nSize == y.nSize)
            {
                return x.nPrimaryKey < y.nPrimaryKey;
            }
            else
            {
                return false;
            }
            return false;
        }
    };

    /** 
     * @brief Keep the track of the size(s) of all the elements in decending order with respective PK. Since elements can have same size.
     * @key: SizePKPair, Size + PK
     * @value: Iterator to the element in List of Cached Elements
     * @compare: std::greater to keep the value in Decending order of Size of elements
    */
    std::map<   SizePKPair, 
                typename std::list<SPTR_CACHE_ELEMENT>::iterator,
                CompareSizePKPair> _mapOfElementsOrderSize;

    /**
     * @brief Threshold checking thread object.
     */    
    std::unique_ptr<std::thread> _ThresholdThread;

    /**
     * @brief Acts a event handler.
     */    
    std::condition_variable _ThresholdThreadSignal;

    /**
     * @brief Mutex for cleaner thread.
     */  
    std::mutex _ThresholdThreadMutex;

    /**
     * @brief Stop the thread processing.
     */
    virtual void end()          
    {
        std::lock_guard<std::mutex> g(_CleanerThreadMutex);
        std::lock_guard<std::mutex> t(_ThresholdThreadMutex);
        _bFinished = true;
        _ThresholdThreadSignal.notify_all();
        _CleanerThreadSignal.notify_all();
    }

    /**
     * @brief Function to call cleanup() at fixed interval.
     */
    virtual void loopCleaner()
    {
        while(true)
        {
            std::unique_lock<std::mutex> lk(_CleanerThreadMutex);
            if (_CleanerThreadSignal.wait_for(lk, std::chrono::seconds(_nCleanScheduleInSec)) == std::cv_status::timeout)
            {
                cleanup();
            }
            if (_bFinished) break;
        }
    }

    /**
     * @brief Function to call checkAccessTime at fixed interval.
     */
    virtual void thresholdChecker()
    {
        while(true)
        {
            std::unique_lock<std::mutex> lk(_ThresholdThreadMutex);
            if (_ThresholdThreadSignal.wait_for(lk, std::chrono::seconds(_nThresholdInSec)) == std::cv_status::timeout)
            {
                checkAccessTime();
            }
            if (_bFinished) break;
        }
    }

    /**
     * @brief Function to act as checker for elements in _listOfElements (auto sorted according to access time)
     * Looks for the first element (oldest), 
     * 1)   If  diff time (element's access - current) >= threshold time
     * 2)       True:   Mark it as candidate for size-wise cleanup and adds to the size wise map
     *                  Note:   cleanup() first removes the elements of size wise map, then go to _listOfElements 
     *                          Mimics the behavior that before threshold time elements gets deleted as per deceding size order 
     *                          and elements with diff time less than threshold gets removed as per last recenelty updated
     * 3)           Continue to next element, go to (1)
     * 4)       False:  Breaks the looking here because it doesnt make sense to keep lookinf forward here. 
     * 
     */
    virtual void checkAccessTime()
    {
        if (_listOfElements.size())
        {
            std::cout << std::endl << "*checkAccessTime()*" << std::endl;
            auto currentTime = std::time(nullptr);

            std::lock_guard<std::mutex> A(_mutexForElementAccess);
            
            for(auto& oldestElement: _listOfElements)
            {
                if(!oldestElement->getMarkSizeWiseCleanup())
                {
                    if (currentTime - oldestElement->getAccessTime() + 1 >= _nThresholdInSec)
                    {
                        // mark here for size based cleanup
                        oldestElement->setMarkSizeWiseCleanup(true);

                        //std::cout << "Added to size wise map : "  << oldestElement->primaryKey() << std::endl;

                        // add to multimap for track the elements in decending order or size 
                        _mapOfElementsOrderSize.insert(
                            std::pair<SizePKPair, typename std::list<SPTR_CACHE_ELEMENT>::iterator>
                            (SizePKPair(oldestElement->size(), oldestElement->primaryKey()), oldestElement->elementInListItr())
                        );
                    }
                    else 
                    {
                        break;
                    }
                }
            }
        }
    }

public:
    virtual ~LRUCacheSizeOrder()
    {
        end();

        if(_CleanerThread)
        {
            _CleanerThread->join();
        }

        if(_ThresholdThread)
        {
            _ThresholdThread->join();
        }
    }

    /**
     * @brief LRUCache Creates a cache of T, with PK being the primary key
     * @param maxSizeSoft Soft limit (bytes): cleaning will limit the size of the cache to this
     * @param maxSizeHard Hard limit (bytes): surpassing this will force a cleaning
     * @param cleanScheduleMs if received, a thread will be created to do the cleaning every cleanScheduleMs milliseconds
     * @param thresholdInSec Threshold in Seconds for size-based-criteria cleanup
     */
    LRUCacheSizeOrder(int64_t maxSizeSoft, int64_t maxSizeHard, int64_t thresholdInSec = 0, int64_t cleanScheduleMs = 0)
                    :   _nSoftLimitInBytes(maxSizeSoft), 
                        _nHardLimitInBytes(maxSizeHard), 
                        _nThresholdInSec(thresholdInSec),
                        _nCleanScheduleInSec(cleanScheduleMs)
    {
        // Start the cleaner thread, if the interval is given.
        if (cleanScheduleMs)
        {
            _CleanerThread.reset(new std::thread([this]()
                                                {
                                                    this->loopCleaner();
                                                }
                                                )
            );
        }

        // Start the threshold thread, if the interval is given.
        if(thresholdInSec)
        {
            _ThresholdThread.reset(new std::thread([this]()
                                                {
                                                    this->thresholdChecker();
                                                }
                                                )
            );
        }
    }

    /**
     * @brief updateElement function to add the new element or update the element with old primary key.
     * 
     * @param element 
     * @param key 
     * @param size 
     */
    virtual void updateElement(std::shared_ptr<T> element, const PK& key, size_t size)
    {
        std::cout << "Update/Insert: ";
        element->print();

        // Shouldn't there must be a criteria of size for elements insertion or updation, 
        // element's size must be equal to smaller than 
        // hard limit in bytes? 
        // Note: If there is no such condition so, what can happen during manual cleanup 
        // (occurs when total size > hard limit) total size still can remain greater than hard limit, 
        // references from internal map and list be removed but the element is still holds the memory. 
        // I am bit confused here, may be we can discuss about that. 
        // In original LRUCache implemention there is no such condition.
        if(size <= _nHardLimitInBytes)
        {
            {
                std::lock_guard<std::mutex> B(_mutexForElementAccess);

                SPTR_CACHE_ELEMENT cacheElement;

                auto itrMap = _mapOfPKWithElement.find(key);
                if (itrMap == _mapOfPKWithElement.end())
                {
                    cacheElement = std::make_shared<LRUCacheElement<T,PK>>(element, key);
                    _mapOfPKWithElement.insert(std::pair<PK,SPTR_CACHE_ELEMENT>(key, cacheElement));
                }
                else    //remove from list to reorder when inserting
                {
                    cacheElement = itrMap->second;
                    _listOfElements.erase(cacheElement->elementInListItr());
                    _nTotalSizeOfCache -= cacheElement->size();
                }

                // remove the marker and from the size wise list
                removeSizeWiseMarker(cacheElement);

                // set size
                cacheElement->setSize(size);

                // increase the total size
                _nTotalSizeOfCache += size;

                // update the access time 
                cacheElement->updateAccessTime();

                // add to list of elements to the end
                _listOfElements.push_back(cacheElement);

                // add the iterator to last element of the list of elements
                cacheElement->setElementInListItr(std::prev(_listOfElements.end()));
            }

            // is the still total size is greated then the hard limit, 
            // do cleanup
            if (_nTotalSizeOfCache > _nHardLimitInBytes)
            {
                cleanup(&key);
            }
        }
    }

    virtual void cleanup(const PK* keyToSaveFromPurge = nullptr)
    {
        std::cout << std::endl << "*cleanup()*" << std::endl;
        std::lock_guard<std::mutex> D(_mutexForElementAccess);
            
        //  vector of data to clean
        std::vector<std::shared_ptr<LRUCleanable>> toClean;

        // first try to remove the element from the size wise map
        if(_mapOfElementsOrderSize.size())
        {
            for (auto& el: _mapOfElementsOrderSize)
            {
                typename std::list<SPTR_CACHE_ELEMENT>::iterator itr = el.second;
                auto weakPointerEl = (*itr)->weakPointerElement();
                auto shrPointerEl = weakPointerEl.lock();
                if (shrPointerEl)
                {
                    toClean.push_back(shrPointerEl);
                }
                _nTotalSizeOfCache -= (*itr)->size();
            }

            _mapOfElementsOrderSize.clear();
        }

        // do loop till total number of elements is greater than 0, 
        // and total size is greater than soft limit
        while (_listOfElements.size() && _nTotalSizeOfCache > _nSoftLimitInBytes)
        {
            // take the first element (or say oldest element in the list)
            auto el = _listOfElements.front();

            // if the current element is marked for size wise cleanup,
            // then skip it because we have already considered for cleaning
                // just pop it out, removes from the list
            _listOfElements.pop_front();

            // earse from the map of elements (PK, PTR)
            _mapOfPKWithElement.erase(el->primaryKey());

            if(!el->getMarkSizeWiseCleanup())
            {
                // KEY have value OR KEY is NOT key
                // lets say: PK:3, Hard Limit: 40B, Size of PK:3 == 50B
                // *keyToSaveFromPurge == 3, el->primaryKey() == 3 (only and last element)
                if (!keyToSaveFromPurge                     ||  // ENTER: if nullptr
                    *keyToSaveFromPurge != el->primaryKey() )   // ENTER: if key == current key
                {
                    auto weakPointerEl = el->weakPointerElement();
                    auto shrPointerEl = weakPointerEl.lock();
                    if (shrPointerEl)
                    {
                        toClean.push_back(shrPointerEl);
                    }

                    _nTotalSizeOfCache -= el->size();
                }
            }
        }

        for (auto &elementToClean : toClean)
        {
            elementToClean->cleanup();
        }
    }

    virtual void removeSizeWiseMarker(const SPTR_CACHE_ELEMENT& cacheElement)
    {
        cacheElement->setMarkSizeWiseCleanup(false);
        _mapOfElementsOrderSize.erase(SizePKPair(cacheElement->size(), cacheElement->primaryKey()));
    }
};