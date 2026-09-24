#ifndef PXA_STORE_CLIENT_H
#define PXA_STORE_CLIENT_H

#include <stddef.h>
#include <stdint.h>

#include "pxa.h"

/* The store origin is part of the signed net.client permission scope, so it
 * must stay byte-identical to the scope in package.json. Rebuild with
 * PXA_APP_DEFINES=PXA_STORE_ORIGIN=... when pointing at another server. */
#ifndef PXA_STORE_ORIGIN
#define PXA_STORE_ORIGIN "https://app.doit.am"
#endif
#ifndef PXA_STORE_PROFILE
#define PXA_STORE_PROFILE "esp32-s3-wamr-2.4.0"
#endif
#ifndef PXA_STORE_CHANNEL
#define PXA_STORE_CHANNEL "stable"
#endif
/* Kind shown before the device reports its taxonomy. */
#ifndef PXA_STORE_DEFAULT_KIND
#define PXA_STORE_DEFAULT_KIND "app"
#endif

#define STORE_REQUEST_DEVICE_PERMISSION UINT32_C(1)
#define STORE_REQUEST_DEVICE_MAC UINT32_C(2)
#define STORE_REQUEST_NETWORK_PERMISSION UINT32_C(3)
#define STORE_REQUEST_CATALOG UINT32_C(4)
#define STORE_REQUEST_DETAIL UINT32_C(5)
#define STORE_REQUEST_WINDOW_SNAPSHOT UINT32_C(6)
#define STORE_REQUEST_DOWNLOAD UINT32_C(7)
#define STORE_REQUEST_INSTALL UINT32_C(8)
#define STORE_REQUEST_INSTALLED UINT32_C(9)
#define STORE_REQUEST_DOWNLOADS UINT32_C(10)
#define STORE_REQUEST_DELETE UINT32_C(11)
#define STORE_REQUEST_LAUNCH UINT32_C(12)
#define STORE_REQUEST_UPDATE_CHECK UINT32_C(13)
#define STORE_REQUEST_UNINSTALL UINT32_C(14)

#define STORE_BODY_ERROR 0
#define STORE_BODY_DONE 1
#define STORE_BODY_WAITING 2
#define STORE_BODY_LIMIT 3

typedef struct {
    uint32_t device_permission;
    uint32_t network_permission;
    uint32_t body_handle;
    uint32_t response_size;
    uint32_t response_flags;
    uint64_t response_length;
    uint16_t http_status;
    uint8_t stream_waiting;
    uint8_t has_device_permission;
    uint8_t has_network_permission;
    uint8_t has_mac;
    uint8_t mac[6];
} store_client_t;

void store_client_init(store_client_t *client);

int store_client_acquire_device_permission(store_client_t *client,
                                           uint8_t *payload,
                                           size_t payload_capacity,
                                           uint8_t *packet,
                                           size_t packet_capacity);

int store_client_acquire_network_permission(store_client_t *client,
                                            uint8_t *payload,
                                            size_t payload_capacity,
                                            uint8_t *packet,
                                            size_t packet_capacity);

int store_client_fetch_mac(store_client_t *client, uint8_t *payload,
                           size_t payload_capacity, uint8_t *packet,
                           size_t packet_capacity);

/* Builds "<origin>/api/v2/catalog?...". query, kind and category may be NULL
 * or empty; the store ignores unknown or empty filters. */
int store_client_build_catalog_url(char *output, size_t capacity,
                                   const char *device_id, uint64_t cursor,
                                   const char *query, const char *kind,
                                   const char *category, uint8_t page_size);

/* Builds "<origin>/api/v2/apps/<app_id>?..." for one catalog entry. */
int store_client_build_app_url(char *output, size_t capacity,
                               const char *app_id, const char *device_id);

int store_client_build_download_url(char *output, size_t capacity,
                                    const char *ticket_path);

int store_client_fetch(store_client_t *client, uint32_t request_id,
                       const char *url, size_t url_length,
                       uint32_t max_response_bytes, uint8_t *payload,
                       size_t payload_capacity, uint8_t *packet,
                       size_t packet_capacity);

int store_client_close_body(store_client_t *client, uint8_t *packet,
                            size_t packet_capacity);

/* Streams the response body into the caller buffer. Returns STORE_BODY_DONE,
 * STORE_BODY_WAITING when the Host needs another clock tick, STORE_BODY_LIMIT
 * when the response exceeds the buffer or STORE_BODY_ERROR on transport
 * failure. */
int store_client_consume_body(store_client_t *client, uint8_t *body,
                              size_t body_capacity, uint8_t *packet,
                              size_t packet_capacity);

#endif
