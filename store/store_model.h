#ifndef PXA_STORE_MODEL_H
#define PXA_STORE_MODEL_H

#include <stddef.h>
#include <stdint.h>

/* The device API caps one network response at 4096 bytes. A compact catalog
 * item is about 1 KiB, so two items per page leave headroom for long names. */
#define STORE_PAGE_SIZE 2
#define STORE_MAX_APPS 12
#define STORE_MAX_QUERY 32
#define STORE_MAX_APP_ID 64
#define STORE_MAX_NAME 48
#define STORE_MAX_SUMMARY 120
#define STORE_MAX_VERSION 24
#define STORE_MAX_CHANGELOG 160
#define STORE_MAX_TAGS 72
#define STORE_MAX_KEY_ID 65
#define STORE_MAX_DIGEST 65
#define STORE_MAX_DOWNLOAD_TICKET 256
#define STORE_MAX_PROFILE 40
#define STORE_MAX_PLATFORMS 64
#define STORE_MAX_KINDS 4
#define STORE_MAX_TAXONOMY 20
#define STORE_MAX_TAXONOMY_VALUE 16
#define STORE_MAX_TAXONOMY_LABEL 24
#define STORE_MAX_PERMISSIONS 8
#define STORE_MAX_PERMISSION_NAME 32
#define STORE_MAX_PERMISSION_SCOPE 56
#define STORE_MAX_SERVICES 8
#define STORE_MAX_SERVICE_NAME 16

typedef struct {
    char name[STORE_MAX_PERMISSION_NAME];
    char scope[STORE_MAX_PERMISSION_SCOPE];
    uint8_t required;
} store_permission_t;

typedef struct {
    char name[STORE_MAX_SERVICE_NAME];
    uint16_t service_id;
    uint16_t min_major;
    uint16_t min_minor;
    uint16_t max_major;
    uint16_t max_minor;
} store_service_t;

typedef struct {
    char app_id[STORE_MAX_APP_ID];
    char name[STORE_MAX_NAME];
    char summary[STORE_MAX_SUMMARY];
    char version[STORE_MAX_VERSION];
    char changelog[STORE_MAX_CHANGELOG];
    char tags[STORE_MAX_TAGS];
    char publisher_key_id[STORE_MAX_KEY_ID];
    char sha256[STORE_MAX_DIGEST];
    char download_ticket_url[STORE_MAX_DOWNLOAD_TICKET];
    char target_profile[STORE_MAX_PROFILE];
    char platforms[STORE_MAX_PLATFORMS];
    char architectures[STORE_MAX_PLATFORMS];
    char min_sdk[16];
    char target_sdk[16];
    char channel[8];
    uint64_t size;
    uint64_t release_sequence;
    uint8_t permission_count;
    store_permission_t permissions[STORE_MAX_PERMISSIONS];
    uint8_t service_count;
    store_service_t services[STORE_MAX_SERVICES];
} store_app_t;

typedef struct {
    char kind[STORE_MAX_TAXONOMY_VALUE];
    char value[STORE_MAX_TAXONOMY_VALUE];
    char label[STORE_MAX_TAXONOMY_LABEL];
    char label_en[STORE_MAX_TAXONOMY_LABEL];
} store_taxonomy_entry_t;

typedef struct {
    store_taxonomy_entry_t kinds[STORE_MAX_KINDS];
    uint8_t kind_count;
    store_taxonomy_entry_t categories[STORE_MAX_TAXONOMY];
    uint8_t category_count;
} store_taxonomy_t;

typedef struct {
    /* Ring window: the oldest entry is overwritten once the window is full, so
     * memory stays bounded no matter how large the catalog grows. */
    store_app_t apps[STORE_MAX_APPS];
    uint8_t head;
    uint8_t count;
    uint8_t dropped;
    uint8_t has_more;
    uint64_t next_cursor;
} store_catalog_t;

static inline store_app_t *store_catalog_slot(store_catalog_t *catalog) {
    store_app_t *slot;
    if (catalog->count < STORE_MAX_APPS) {
        slot = &catalog->apps[(catalog->head + catalog->count) % STORE_MAX_APPS];
        ++catalog->count;
        return slot;
    }
    slot = &catalog->apps[catalog->head];
    catalog->head = (uint8_t)((catalog->head + 1u) % STORE_MAX_APPS);
    catalog->dropped = 1;
    return slot;
}

static inline store_app_t *store_catalog_at(store_catalog_t *catalog,
                                            uint8_t index) {
    return &catalog->apps[(catalog->head + index) % STORE_MAX_APPS];
}

static inline const store_app_t *store_catalog_at_const(
    const store_catalog_t *catalog, uint8_t index) {
    return &catalog->apps[(catalog->head + index) % STORE_MAX_APPS];
}

#endif
