#ifndef PXA_STORE_JSON_H
#define PXA_STORE_JSON_H

#include <stddef.h>
#include <stdint.h>

#include "store_model.h"

/* Decodes the signed device catalog envelope produced by
 * GET /api/v2/catalog. The envelope signature is not verified; only the
 * embedded payload is decoded. Items are appended to catalog->apps starting at
 * catalog->count, so a caller can accumulate several pages. */
int store_json_parse_catalog(const uint8_t *data, size_t size,
                             store_catalog_t *catalog,
                             store_taxonomy_t *taxonomy);

/* Decodes the single-item envelope produced by
 * GET /api/v2/apps/{app_id}. */
int store_json_parse_app_detail(const uint8_t *data, size_t size,
                                store_app_t *app, store_gallery_t *gallery = nullptr);

/* Small, bounded unsigned account response, separate from signed catalog data. */
typedef struct {
    char device_code[96], user_code[24], access_token[96], display_name[128], error[40];
    uint64_t expires_in, interval, count;
    uint8_t liked;
    uint16_t fields;
} store_account_response_t;
int store_json_parse_account(const uint8_t *data, size_t size, store_account_response_t *out);

#endif
