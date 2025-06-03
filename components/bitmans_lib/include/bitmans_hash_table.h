#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bitmans_hash_entry_t {
    uint16_t key;
    void *pValue;
    struct bitmans_hash_entry_t *pNext; 
} bitmans_hash_entry_t;

typedef struct {
    size_t size;
    uint8_t *pUsedEntries; 
    bitmans_hash_entry_t *pEntries;
} bitmans_hash_table_t;

uint16_t bitmans_rs_hash(uint16_t key, size_t size);
esp_err_t bitmans_hash_table_cleanup(bitmans_hash_table_t *);
esp_err_t bitmans_hash_table_init(bitmans_hash_table_t *, size_t size);
esp_err_t bitmans_hash_table_remove(bitmans_hash_table_t *, uint16_t key);
esp_err_t bitmans_hash_table_set(bitmans_hash_table_t *, uint16_t key, void *pValue);
esp_err_t bitmans_hash_table_get(bitmans_hash_table_t *, uint16_t key, void **ppValue);

#ifdef __cplusplus
}
#endif
