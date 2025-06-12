/**
 * @file bat_hash_table.h
 * @brief Simple hash‐table API for mapping uint16_t keys to void* values.
 *
 * Provides functions to:
 *   - initialize a fixed‐size table with optional value cleanup callback
 *   - insert or update entries (bat_hash_table_set)
 *   - retrieve entries (bat_hash_table_get / try_get)
 *   - remove entries (bat_hash_table_remove)
 *   - clean up all entries (bat_hash_table_cleanup)
 *
 * Uses separate chaining for collision resolution and a bitmap to track
 * which buckets are in use. Not thread‐safe—external synchronization required.
 */
#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stddef.h>

/**
 * @file bat_hash_table.h
 * @brief Simple hash‐table API for mapping uint16_t keys to void* values.
 *
 * Provides functions to:
 *   - initialize a fixed‐size table with optional value cleanup callback
 *   - insert or update entries (bat_hash_table_set)
 *   - retrieve entries (bat_hash_table_get / try_get)
 *   - remove entries (bat_hash_table_remove)
 *   - clean up all entries (bat_hash_table_cleanup)
 *
 * Uses separate chaining for collision resolution and a bitmap to track
 * which buckets are in use. Not thread‐safe—external synchronization required.
 */
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Represents a single entry in the hash table.
 * 
 * Each entry contains a key, a value pointer, and a pointer to the next entry
 * for handling collisions via chaining.
 */
typedef struct bat_hash_entry_t {
    uint16_t key;                        ///< The key associated with the entry.
    void *pValue;                        ///< Pointer to the value stored in the entry.
    struct bat_hash_entry_t *pNext;  ///< Pointer to the next entry in the chain (for collisions).
} bat_hash_entry_t;

/**
 * @brief Callback function type for cleaning up values.
 * 
 * This callback is invoked when a value is replaced or removed from the hash table.
 * 
 * @param pValue Pointer to the value being cleaned up.
 * @param pContext User-defined context passed during hash table initialization.
 */
typedef void (*bat_hash_value_cleanup_cb_t)(void *pValue, void *pContext);

/**
 * @brief Represents the hash table structure.
 * 
 * The hash table uses a bitmap to track used buckets and supports collision handling
 * via chaining. The first entry in each bucket is pre-allocated, while additional
 * entries are dynamically allocated.
 */
typedef struct {
    size_t size;                         ///< Number of buckets in the hash table.
    void *pContext;                      ///< User-defined context for cleanup callbacks.
    uint8_t *pUsedEntries;               ///< Bitmap to track used buckets.
    bat_hash_entry_t *pEntries;      ///< Array of pre-allocated entries (one per bucket).
    bat_hash_value_cleanup_cb_t value_cleanup_cb; ///< Callback for cleaning up values.
} bat_hash_table_t;

/**
 * @brief Computes a hash value for a given key.
 * 
 * Uses Robert Sedgwick's simple hash function for `uint16_t` keys.
 * 
 * @param key The key to hash.
 * @param size The size of the hash table (number of buckets).
 * @return The computed hash value (bucket index).
 */
uint16_t bat_rs_hash_uint16(uint16_t key, size_t size);

/**
 * @brief Initializes the hash table.
 * 
 * Allocates memory for the hash table's entries and bitmap, and sets up the cleanup callback.
 * 
 * @param pTable Pointer to the hash table structure to initialize.
 * @param size Number of buckets in the hash table.
 * @param value_cleanup_cb Callback for cleaning up values.
 * @param pContext User-defined context for the cleanup callback.
 * @return `ESP_OK` on success, or an error code on failure.
 */
esp_err_t bat_hash_table_init(bat_hash_table_t *pTable, size_t size, 
    bat_hash_value_cleanup_cb_t value_cleanup_cb, void *pContext);

/**
 * @brief Cleans up the hash table.
 * 
 * Frees all allocated memory, including chained entries, and invokes the cleanup callback
 * for all stored values.
 * 
 * @param pTable Pointer to the hash table structure to clean up.
 */
void bat_hash_table_cleanup(bat_hash_table_t *pTable);

/**
 * @brief Removes an entry from the hash table.
 * 
 * Frees the memory associated with the entry and invokes the cleanup callback for its value.
 * 
 * @param pTable Pointer to the hash table structure.
 * @param key The key of the entry to remove.
 * @return `ESP_OK` on success, or an error code if the key is not found.
 */
esp_err_t bat_hash_table_remove(bat_hash_table_t *pTable, uint16_t key);

/**
 * @brief Adds or updates an entry in the hash table.
 * 
 * If the key already exists, its value is updated. Otherwise, a new entry is added.
 * 
 * @param pTable Pointer to the hash table structure.
 * @param key The key of the entry to add or update.
 * @param pValue Pointer to the value to store.
 * @return `ESP_OK` on success, or an error code on failure (e.g., memory allocation failure).
 */
esp_err_t bat_hash_table_set(bat_hash_table_t *pTable, uint16_t key, void *pValue);

/**
 * @brief Retrieves an entry from the hash table.
 * 
 * Searches for the specified key and returns its associated value.
 * 
 * @param pTable Pointer to the hash table structure.
 * @param key The key to search for.
 * @param ppValue Pointer to store the retrieved value.
 * @return `ESP_OK` on success, or an error code if the key is not found.
 */
esp_err_t bat_hash_table_get(bat_hash_table_t *pTable, uint16_t key, void **ppValue);

/**
 * @brief Attempts to retrieve an entry from the hash table.
 * 
 * Similar to `bat_hash_table_get`, but returns the value directly or `NULL` if not found.
 * 
 * @param pTable Pointer to the hash table structure.
 * @param key The key to search for.
 * @return Pointer to the value if found, or `NULL` if not found.
 */
void *bat_hash_table_try_get(bat_hash_table_t *pTable, uint16_t key);

#ifdef __cplusplus
}
#endif
