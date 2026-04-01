#pragma once

#include <cstdlib>
#include "lib/string.h"
#include "lib/unordered_map.h"
#include "lib/logger.h"
#include "lib/deque.h"
#include <thread>
#include <shared_mutex>

enum CrawlStatus {
    ALLOWED = 0,
    DISALLOWED = 1,
    PENDING = 2
};

// TODO(charlie): move in robots.txt hw assignment parsing logic here
// TODO(charlie): figure out request enqueue/dequeue semantics
    // TODO(charlie): define fetch requests
class RobotsManager {
private:
    struct Node {
        string domain = string("");
        RobotsRules* rules;
        Node* prev;
        Node* next;
    };
public:
    
    RobotsManager(size_t cache_capacity) : capacity(cache_capacity) {
        head = new Node();
        tail = new Node();
        head->next = tail;
        tail->prev = head;
        worker_thread = std::thread(&RobotsManager::workerLoop, this);
    }

    ~RobotsManager();

    // The main entry point for Crawler Threads
    // Returns: ALLOWED, DISALLOWED, or PENDING (try again later)
    CrawlStatus checkStatus(const string& domain, const string& url);

private:
    // 1. The Cache Structures
    size_t capacity;
    
    Node *head, *tail; // Dummy head and tail for LRU list
    unordered_map<string, Node*> cache_map;
    // 2. Synchronization
    std::shared_mutex cache_mutex; // C++17 Shared Mutex for Readers/Writer
    
    // 3. The Background Worker Thread
    std::thread worker_thread;
    deque<FetchRequest> request_queue;
    std::atomic<bool> stop_worker{false};

    // The function the background thread runs
    void workerLoop() {
        while (!stop_worker) {
            auto request = request_queue.wait_and_pop();
            if (request) {
                RobotsRules rules = fetchAndParse(request->domain);
                {
                    std::unique_lock lock(cache_mutex);
                    // Evict if at capacity
                    if (cache_map.size() >= capacity) {
                        Node* lru = tail->prev;
                        cache_map.erase(lru->domain);
                        lru->prev->next = tail;
                        tail->prev = lru->prev;
                        delete lru->rules;
                        delete lru;
                    }
                    // Insert new rules at head
                    Node* new_node = new Node{request->domain, new RobotsRules(rules), nullptr, nullptr};
                    new_node->next = head->next;
                    new_node->prev = head;
                    head->next->prev = new_node;
                    head->next = new_node;
                    cache_map[request->domain] = new_node;
                }
            }
        }
    }
    
    // Helper to perform the actual HTTP fetch and parse
    RobotsRules fetchAndParse(const string& domain);
};