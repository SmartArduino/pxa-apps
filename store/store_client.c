/* Guest-side transport for the device catalog API. */

#include "store_client.h"

#include "pxa_device.h"
#include "pxa_net.h"
#include "pxa_permission.h"
#include "store_model.h"

#define STORE_URL_CAPACITY 512
#define STORE_RESPONSE_TIMEOUT_MS UINT32_C(15000)
#define STORE_STREAM_TICK_MS UINT32_C(50)

static const char device_permission_name[] = "device.identity";
static const char device_permission_scope[] = "mac.wifi.station.hardware";
static const char network_permission_name[] = "net.client";
static const char catalog_path[] = "/api/v2/catalog?profile=";
static const char artifact_path[] = "/api/v2/artifacts/";

static int append_text(char *output, size_t capacity, size_t *offset,
                       const char *value) {
    size_t index = 0;
    while (value[index] != '\0') {
        if (*offset + 1u >= capacity) return 0;
        output[(*offset)++] = value[index++];
    }
    output[*offset] = '\0';
    return 1;
}

static int append_u64(char *output, size_t capacity, size_t *offset,
                      uint64_t value) {
    char reverse[20];
    size_t count = 0;
    do {
        reverse[count++] = (char)('0' + value % 10u);
        value /= 10u;
    } while (value != 0 && count < sizeof(reverse));
    while (count != 0) {
        if (*offset + 1u >= capacity) return 0;
        output[(*offset)++] = reverse[--count];
    }
    output[*offset] = '\0';
    return 1;
}

/* Percent-encodes every byte outside the RFC 3986 unreserved set. */
static int append_query_value(char *output, size_t capacity, size_t *offset,
                              const char *value) {
    static const char hex[] = "0123456789ABCDEF";
    size_t index = 0;
    while (value[index] != '\0') {
        uint8_t byte = (uint8_t)value[index++];
        int unreserved = (byte >= 'a' && byte <= 'z') ||
                         (byte >= 'A' && byte <= 'Z') ||
                         (byte >= '0' && byte <= '9') || byte == '-' ||
                         byte == '.' || byte == '_' || byte == '~';
        if (unreserved) {
            if (*offset + 1u >= capacity) return 0;
            output[(*offset)++] = (char)byte;
        } else {
            if (*offset + 3u >= capacity) return 0;
            output[(*offset)++] = '%';
            output[(*offset)++] = hex[byte >> 4];
            output[(*offset)++] = hex[byte & 0x0fu];
        }
    }
    output[*offset] = '\0';
    return 1;
}

static int close_handle(uint32_t handle, uint8_t *packet,
                        size_t packet_capacity) {
    uint8_t payload[4];
    pxa_writer_t writer;
    if (handle == 0) return 1;
    payload[0] = (uint8_t)handle;
    payload[1] = (uint8_t)(handle >> 8);
    payload[2] = (uint8_t)(handle >> 16);
    payload[3] = (uint8_t)(handle >> 24);
    pxa_writer_init(&writer, packet, packet_capacity);
    return pxa_message(&writer, PXA_SERVICE_CORE, PXA_CORE_CLOSE_HANDLE, 0,
                       payload, sizeof(payload)) &&
           pxa_control(writer.data, (uint32_t)writer.length) == PXA_STATUS_OK;
}

void store_client_init(store_client_t *client) {
    client->device_permission = 0;
    client->network_permission = 0;
    client->body_handle = 0;
    client->response_size = 0;
    client->response_flags = 0;
    client->response_length = 0;
    client->http_status = 0;
    client->stream_waiting = 0;
    client->has_device_permission = 0;
    client->has_network_permission = 0;
    client->has_mac = 0;
    for (size_t index = 0; index < sizeof(client->mac); ++index)
        client->mac[index] = 0;
}

int store_client_acquire_device_permission(store_client_t *client,
                                           uint8_t *payload,
                                           size_t payload_capacity,
                                           uint8_t *packet,
                                           size_t packet_capacity) {
    (void)client;
    return pxa_permission_acquire(
        STORE_REQUEST_DEVICE_PERMISSION, device_permission_name,
        sizeof(device_permission_name) - 1u,
        (const uint8_t *)device_permission_scope,
        sizeof(device_permission_scope) - 1u, payload, payload_capacity, packet,
        packet_capacity);
}

int store_client_acquire_network_permission(store_client_t *client,
                                            uint8_t *payload,
                                            size_t payload_capacity,
                                            uint8_t *packet,
                                            size_t packet_capacity) {
    static const char scope[] = PXA_STORE_ORIGIN;
    (void)client;
    return pxa_permission_acquire(
        STORE_REQUEST_NETWORK_PERMISSION, network_permission_name,
        sizeof(network_permission_name) - 1u, (const uint8_t *)scope,
        sizeof(scope) - 1u, payload, payload_capacity, packet,
        packet_capacity);
}

int store_client_fetch_mac(store_client_t *client, uint8_t *payload,
                           size_t payload_capacity, uint8_t *packet,
                           size_t packet_capacity) {
    return pxa_device_get_mac(
        STORE_REQUEST_DEVICE_MAC, PXA_DEVICE_MAC_KIND_WIFI_STATION_HARDWARE,
        client->device_permission, payload, payload_capacity, packet,
        packet_capacity);
}

int store_client_build_catalog_url(char *output, size_t capacity,
                                   const char *device_id, uint64_t cursor,
                                   const char *query, const char *kind,
                                   const char *category, uint8_t page_size) {
    size_t offset = 0;
    if (output == NULL || capacity == 0 || capacity > STORE_URL_CAPACITY ||
        page_size == 0 || page_size > STORE_MAX_APPS)
        return 0;
    output[0] = '\0';
    if (!append_text(output, capacity, &offset, PXA_STORE_ORIGIN) ||
        !append_text(output, capacity, &offset, catalog_path) ||
        !append_query_value(output, capacity, &offset, PXA_STORE_PROFILE) ||
        !append_text(output, capacity, &offset, "&channel=") ||
        !append_query_value(output, capacity, &offset, PXA_STORE_CHANNEL) ||
        !append_text(output, capacity, &offset, "&view=compact&page_size=") ||
        !append_u64(output, capacity, &offset, page_size))
        return 0;
    if (kind != NULL && kind[0] != '\0') {
        if (!append_text(output, capacity, &offset, "&kind=") ||
            !append_query_value(output, capacity, &offset, kind))
            return 0;
    }
    if (category != NULL && category[0] != '\0') {
        if (!append_text(output, capacity, &offset, "&category=") ||
            !append_query_value(output, capacity, &offset, category))
            return 0;
    }
    if (device_id != NULL && device_id[0] != '\0') {
        if (!append_text(output, capacity, &offset, "&device_id=") ||
            !append_query_value(output, capacity, &offset, device_id))
            return 0;
    }
    if (cursor != 0) {
        if (!append_text(output, capacity, &offset, "&cursor=") ||
            !append_u64(output, capacity, &offset, cursor))
            return 0;
    }
    if (query != NULL && query[0] != '\0') {
        if (!append_text(output, capacity, &offset, "&q=") ||
            !append_query_value(output, capacity, &offset, query))
            return 0;
    }
    return offset != 0;
}

int store_client_build_app_url(char *output, size_t capacity,
                               const char *app_id, const char *device_id) {
    size_t offset = 0;
    if (output == NULL || capacity == 0 || capacity > STORE_URL_CAPACITY ||
        app_id == NULL || app_id[0] == '\0')
        return 0;
    output[0] = '\0';
    if (!append_text(output, capacity, &offset, PXA_STORE_ORIGIN) ||
        !append_text(output, capacity, &offset, "/api/v2/apps/") ||
        !append_query_value(output, capacity, &offset, app_id) ||
        !append_text(output, capacity, &offset, "?profile=") ||
        !append_query_value(output, capacity, &offset, PXA_STORE_PROFILE) ||
        !append_text(output, capacity, &offset, "&channel=") ||
        !append_query_value(output, capacity, &offset, PXA_STORE_CHANNEL) ||
        !append_text(output, capacity, &offset, "&view=compact"))
        return 0;
    if (device_id != NULL && device_id[0] != '\0') {
        if (!append_text(output, capacity, &offset, "&device_id=") ||
            !append_query_value(output, capacity, &offset, device_id))
            return 0;
    }
    return offset != 0;
}

int store_client_build_download_url(char *output, size_t capacity,
                                    const char *ticket_path) {
    size_t offset = 0;
    size_t index = 0;
    static const char download_suffix[] = "/download?expires=";
    static const char signature_key[] = "&signature=";
    if (output == NULL || ticket_path == NULL || capacity == 0 ||
        capacity > STORE_URL_CAPACITY) return 0;
    output[0] = '\0';
    while (index < STORE_MAX_DOWNLOAD_TICKET && ticket_path[index] != '\0')
        ++index;
    if (index == STORE_MAX_DOWNLOAD_TICKET) return 0;
    index = 0;
    while (artifact_path[index] != '\0') {
        if (ticket_path[index] != artifact_path[index]) return 0;
        ++index;
    }
    if (ticket_path[index] < '0' || ticket_path[index] > '9') return 0;
    while (ticket_path[index] >= '0' && ticket_path[index] <= '9') ++index;
    for (size_t suffix = 0; download_suffix[suffix] != '\0'; ++suffix)
        if (ticket_path[index++] != download_suffix[suffix]) return 0;
    if (ticket_path[index] < '0' || ticket_path[index] > '9') return 0;
    while (ticket_path[index] >= '0' && ticket_path[index] <= '9') ++index;
    for (size_t key = 0; signature_key[key] != '\0'; ++key)
        if (ticket_path[index++] != signature_key[key]) return 0;
    for (size_t digit = 0; digit < 64u; ++digit) {
        uint8_t byte = (uint8_t)ticket_path[index++];
        if (!((byte >= '0' && byte <= '9') ||
              (byte >= 'a' && byte <= 'f'))) return 0;
    }
    if (index >= STORE_MAX_DOWNLOAD_TICKET || ticket_path[index] != '\0' ||
        !append_text(output, capacity, &offset, PXA_STORE_ORIGIN) ||
        !append_text(output, capacity, &offset, ticket_path)) return 0;
    return 1;
}

int store_client_fetch(store_client_t *client, uint32_t request_id,
                       const char *url, size_t url_length,
                       uint32_t max_response_bytes, uint8_t *payload,
                       size_t payload_capacity, uint8_t *packet,
                       size_t packet_capacity) {
    static const char accept_name[] = "accept";
    static const uint8_t accept_value[] = "application/json";
    static const pxa_net_header_t headers[] = {
        {accept_name, sizeof(accept_name) - 1u, accept_value,
         sizeof(accept_value) - 1u},
    };
    pxa_net_http_request_t request = {0};
    client->response_size = 0;
    client->response_length = 0;
    client->response_flags = 0;
    client->http_status = 0;
    client->stream_waiting = 0;
    request.method = PXA_NET_METHOD_GET;
    request.url = url;
    request.url_length = (uint16_t)url_length;
    request.permission_handle = client->network_permission;
    request.max_response_bytes = max_response_bytes;
    request.timeout_ms = STORE_RESPONSE_TIMEOUT_MS;
    request.headers = headers;
    request.header_count = 1;
    return pxa_net_http_request(request_id, &request, payload,
                                payload_capacity, packet, packet_capacity);
}

int store_client_close_body(store_client_t *client, uint8_t *packet,
                            size_t packet_capacity) {
    int ok = close_handle(client->body_handle, packet, packet_capacity);
    client->body_handle = 0;
    client->stream_waiting = 0;
    return ok;
}

int store_client_consume_body(store_client_t *client, uint8_t *body,
                              size_t body_capacity, uint8_t *packet,
                              size_t packet_capacity) {
    uint8_t overflow;
    for (;;) {
        uint32_t remaining =
            (uint32_t)(body_capacity - client->response_size);
        uint8_t *output = remaining == 0 ? &overflow
                                         : body + client->response_size;
        uint32_t capacity = remaining == 0 ? 1u : remaining;
        int32_t count = pxa_io(client->body_handle, PXA_IO_READ, output,
                               capacity);
        if (count == PXA_STATUS_WOULD_BLOCK) {
            client->stream_waiting = 1;
            (void)pxa_clock_set_period(STORE_STREAM_TICK_MS);
            return STORE_BODY_WAITING;
        }
        if (count < 0 || (remaining == 0 && count != 0) ||
            (uint32_t)(count < 0 ? 0 : count) > remaining) {
            (void)store_client_close_body(client, packet, packet_capacity);
            return remaining == 0 ? STORE_BODY_LIMIT : STORE_BODY_ERROR;
        }
        if (count == 0) {
            client->stream_waiting = 0;
            return STORE_BODY_DONE;
        }
        client->response_size += (uint32_t)count;
    }
}
