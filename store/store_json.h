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
                                store_app_t *app);

#endif
