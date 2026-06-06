#pragma once
#include <string>
#include <list>
#include <unordered_map>
#include <mutex>
#include <optional>

struct CacheEntry {
    std::string data;
    std::string content_type;
};

class LRUCache {
public:
    explicit LRUCache(size_t max_size_bytes = 32 * 1024 * 1024); // 32MB default

    void put(const std::string& key, const CacheEntry& entry);
    std::optional<CacheEntry> get(const std::string& key);
    void clear();
    size_t current_size() const;

private:
    size_t max_size_bytes_;
    size_t current_size_ = 0;
    mutable std::mutex mutex_;

    using ListIt = std::list<std::pair<std::string, CacheEntry>>::iterator;
    std::list<std::pair<std::string, CacheEntry>> items_;
    std::unordered_map<std::string, ListIt> map_;

    void evict();
};
