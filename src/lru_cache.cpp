#include "lru_cache.h"
#include "logger.h"

LRUCache::LRUCache(size_t max_size_bytes)
    : max_size_bytes_(max_size_bytes), current_size_(0) {}

void LRUCache::put(const std::string& key, const CacheEntry& entry) {
    std::unique_lock<std::mutex> lock(mutex_);

    size_t entry_size = key.size() + entry.data.size() + entry.content_type.size();

    // If this single entry is larger than the cache, don't cache it
    if (entry_size > max_size_bytes_) {
        LOG_WARN("LRUCache: entry too large to cache: " + key);
        return;
    }

    // Remove existing entry if present
    auto it = map_.find(key);
    if (it != map_.end()) {
        current_size_ -= it->second->first.size() + it->second->second.data.size();
        items_.erase(it->second);
        map_.erase(it);
    }

    // Evict until there's room
    while (current_size_ + entry_size > max_size_bytes_ && !items_.empty()) {
        evict();
    }

    items_.push_front({key, entry});
    map_[key] = items_.begin();
    current_size_ += entry_size;
}

std::optional<CacheEntry> LRUCache::get(const std::string& key) {
    std::unique_lock<std::mutex> lock(mutex_);

    auto it = map_.find(key);
    if (it == map_.end()) return std::nullopt;

    // Move to front (most recently used)
    items_.splice(items_.begin(), items_, it->second);
    return it->second->second;
}

void LRUCache::evict() {
    // Remove least recently used (back of list)
    auto last = items_.end();
    --last;
    current_size_ -= last->first.size() + last->second.data.size();
    map_.erase(last->first);
    items_.pop_back();
}

void LRUCache::clear() {
    std::unique_lock<std::mutex> lock(mutex_);
    items_.clear();
    map_.clear();
    current_size_ = 0;
}

size_t LRUCache::current_size() const {
    std::unique_lock<std::mutex> lock(mutex_);
    return current_size_;
}
