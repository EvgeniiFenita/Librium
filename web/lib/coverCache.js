'use strict';

const MAX_CACHE_SIZE = 50 * 1024 * 1024; // 50 MB

let currentCacheSize = 0;
const coverCache = new Map();

function getCoverFromCache(bookId) {
    if (coverCache.has(bookId)) {
        const data = coverCache.get(bookId);
        coverCache.delete(bookId);
        coverCache.set(bookId, data); // Move to end (MRU)
        return data;
    }
    return null;
}

function addCoverToCache(bookId, data) {
    if (coverCache.has(bookId)) {
        currentCacheSize -= coverCache.get(bookId).data.length;
        coverCache.delete(bookId);
    }
    coverCache.set(bookId, data);
    currentCacheSize += data.data.length;

    while (currentCacheSize > MAX_CACHE_SIZE) {
        const firstKey = coverCache.keys().next().value;
        currentCacheSize -= coverCache.get(firstKey).data.length;
        coverCache.delete(firstKey);
    }
}

module.exports = { getCoverFromCache, addCoverToCache };
