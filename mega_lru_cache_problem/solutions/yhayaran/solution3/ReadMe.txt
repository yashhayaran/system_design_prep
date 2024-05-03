Tasks:

1. Successfully implemented a basic string class
   PLEASE review the code with comments of stringType.cpp

2. A)   Successfully able to compile the code for LRUCache. Got the output
        Is simply caches least recenlty used elements
        Output explained:
            soft 20 bytes, hard 40 bytes
            first element added 10 -> 10
            second element added 10 -> 20
            third element added 10 -> 30
            (wait for 2 secs, during this cleanup thread works)
            removed first element (due to total 30 bytes > soft = 20 bytes)
            fourth element added 10 -> 30
            updated second element (so, it comes to the last in the List of recently used elements)
            fifth element added 10 -> 40 (causes cleanup call because total > hard limit is reached)
            third element removed 
            fourth element removed
            only second and fifth left


2. B)   Successfully implemented the extended version of LRUCache to LRUCacheSizeOrder, to get the new feature as per task.txt.
        PLEASE review the code with comments of lru_size_order.h for new implememtion. 
        Please run 'test2' in main() to execute the test case. 