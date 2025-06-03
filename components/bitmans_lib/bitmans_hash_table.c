#include "bitmans_hash_table.h"
#include <stdlib.h>
#include <string.h>

// Robert Sedgwicks's simple hash function for uint16_t
uint16_t bitmans_rs_hash(uint16_t key, size_t size)
{
    uint32_t hash = 0;
    uint32_t a = 63689;
    uint32_t b = 378551;

    hash = hash * a + (key & 0xFF);
    a = a * b;
    hash = hash * a + ((key >> 8) & 0xFF);

    return (uint16_t)(hash % size);
}

esp_err_t bitmans_hash_table_init(bitmans_hash_table_t *pTable, size_t size)
{
    if (pTable == NULL || size == 0)
        return ESP_ERR_INVALID_ARG;

    pTable->pEntries = (bitmans_hash_entry_t *)calloc(size, sizeof(bitmans_hash_entry_t));
    if (pTable->pEntries != NULL)
    {
        // Using a bitmap to track used entries.
        size_t bitmap_size = (size / 8) + ((size % 8) ? 1 : 0); // OR, more efficient: (size + 7) / 8
        pTable->pUsedEntries = (uint8_t *)calloc(bitmap_size, 1);
        if (pTable->pUsedEntries != NULL)
        {
            pTable->size = size;
            return ESP_OK;
        }
    }

    free(pTable->pEntries);
    pTable->pEntries = NULL;
    return ESP_ERR_NO_MEM;
}

esp_err_t bitmans_hash_table_cleanup(bitmans_hash_table_t *pTable)
{
    if (pTable == NULL || pTable->pEntries == NULL)
        return ESP_ERR_INVALID_ARG;

    // Free chained entries
    for (size_t i = 0; i < pTable->size; ++i)
    {
        bitmans_hash_entry_t *pEntry = pTable->pEntries[i].pNext;
        while (pEntry != NULL)
        {
            bitmans_hash_entry_t *pNext = pEntry->pNext;
            free(pEntry);
            pEntry = pNext;
        }
    }

    free(pTable->pEntries);
    free(pTable->pUsedEntries);

    pTable->size = 0;
    pTable->pEntries = NULL;
    pTable->pUsedEntries = NULL;

    return ESP_OK;
}

// Helper macros check the bitmap for used/unused entries.
#define BITMANS_IS_BUCKET_USED(bitmap, idx) (bitmap[(idx) / 8] & (1 << ((idx) % 8)))
#define BITMANS_IS_BUCKET_EMPTY(bitmap, idx) (BITMANS_IS_BUCKET_USED(bitmap, idx) == 0)
#define BITMANS_SET_BUCKET_USED(bitmap, idx) (bitmap[(idx) / 8] |= (1 << ((idx) % 8)))
#define BITMANS_SET_BUCKET_UNUSED(bitmap, idx) (bitmap[(idx) / 8] &= ~(1 << ((idx) % 8)))

typedef struct
{
    bitmans_hash_entry_t *pPrev;
    bitmans_hash_entry_t *pFound;
} bitmans_hash_find_result_t;

static bitmans_hash_find_result_t bitmans_hash_table_find_entry(bitmans_hash_entry_t *pEntry, uint16_t key)
{
    bitmans_hash_find_result_t result = {NULL, NULL};

    for (; pEntry != NULL; pEntry = pEntry->pNext)
    {
        if (pEntry->key == key)
        {
            result.pFound = pEntry;
            return result;
        }

        result.pPrev = pEntry;
    }

    result.pFound = NULL;
    return result;
}

esp_err_t bitmans_hash_table_set(bitmans_hash_table_t *pTable, uint16_t key, void *pValue)
{
    if (pTable == NULL || pTable->pEntries == NULL || pTable->pUsedEntries == NULL)
        return ESP_ERR_INVALID_ARG;

    uint16_t idx = bitmans_rs_hash(key, pTable->size);
    bitmans_hash_entry_t *pFirstEntry = &pTable->pEntries[idx];

    // Optimal case.
    if (BITMANS_IS_BUCKET_EMPTY(pTable->pUsedEntries, idx))
    {
        pFirstEntry->key = key;
        pFirstEntry->pNext = NULL;
        pFirstEntry->pValue = pValue;
        BITMANS_SET_BUCKET_USED(pTable->pUsedEntries, idx);
        return ESP_OK;
    }

    bitmans_hash_find_result_t findResult = bitmans_hash_table_find_entry(pFirstEntry, key);
    if (findResult.pFound != NULL)
        findResult.pFound->pValue = pValue; // Key already exists, update value
    else
    {
        // Key doesn't exist, add new.
        bitmans_hash_entry_t *pNewEntry = (bitmans_hash_entry_t *)calloc(1, sizeof(bitmans_hash_entry_t));
        if (pNewEntry == NULL)
            return ESP_ERR_NO_MEM;

        pNewEntry->key = key;
        pNewEntry->pValue = pValue;

        if (findResult.pPrev != NULL)
            findResult.pPrev->pNext = pNewEntry; // Add to the end of the list
        else
        {
            // Add it to the chain.
            pNewEntry->pNext = pFirstEntry->pNext;
            pFirstEntry->pNext = pNewEntry;
        }
        return ESP_OK;
    }

    return ESP_OK;
}

esp_err_t bitmans_hash_table_get(bitmans_hash_table_t *pTable, uint16_t key, void **ppValue)
{
    if (pTable == NULL || pTable->pEntries == NULL || pTable->pUsedEntries == NULL || ppValue == NULL)
        return ESP_ERR_INVALID_ARG;

    uint16_t idx = bitmans_rs_hash(key, pTable->size);
    if (BITMANS_IS_BUCKET_USED(pTable->pUsedEntries, idx))
    {
        bitmans_hash_entry_t *pFirstEntry = &pTable->pEntries[idx];
        bitmans_hash_find_result_t findResult = bitmans_hash_table_find_entry(pFirstEntry, key);
        if (findResult.pFound != NULL)
        {
            *ppValue = findResult.pFound->pValue;
            return ESP_OK;
        }
    }

    *ppValue = NULL;
    return ESP_ERR_NOT_FOUND;
}

static void bitmans_hash_table_free_entry(bitmans_hash_entry_t *pPrev, bitmans_hash_entry_t *pEntry)
{
    if (pPrev == NULL) // Don't delete the first entry in the chain.
    {
        pEntry->key = 0;
        pEntry->pNext = NULL; 
        pEntry->pValue = NULL;
    }
    else
    {
        pPrev->pNext = pEntry->pNext; // Remove from chain, delete the entrie.
        free(pEntry);
    }
}

esp_err_t bitmans_hash_table_remove(bitmans_hash_table_t *pTable, uint16_t key)
{
    if (pTable == NULL || pTable->pEntries == NULL || pTable->pUsedEntries == NULL)
        return ESP_ERR_INVALID_ARG;

    uint16_t idx = bitmans_rs_hash(key, pTable->size);
    if (BITMANS_IS_BUCKET_USED(pTable->pUsedEntries, idx))
    {
        bitmans_hash_entry_t *pFirstEntry = &pTable->pEntries[idx];
        bitmans_hash_find_result_t findResult = bitmans_hash_table_find_entry(pFirstEntry, key);

        if (findResult.pFound != NULL)
        {
            bitmans_hash_table_free_entry(findResult.pPrev, findResult.pFound);

            // We were the first *AND* last...
            if (findResult.pPrev == NULL && pFirstEntry->pNext == NULL)
                BITMANS_SET_BUCKET_UNUSED(pTable->pUsedEntries, idx);
        }
    }

    return ESP_OK;
}
