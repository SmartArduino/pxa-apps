#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "store_client.h"
#include "store_json.h"
#include "pxa_store_installer.h"

int main(void) {
    uint8_t progress_payload[20] = {42, 0, 0, 0, 50, 0, 0, 0, 0, 0, 0, 0,
                                    100, 0, 0, 0, 0, 0, 0, 0};
    pxa_event_t progress_event = {0};
    uint32_t progress_request;
    uint64_t received, total;
    progress_event.service = PXA_SERVICE_STORE_INSTALLER;
    progress_event.opcode = PXA_STORE_DOWNLOAD_PROGRESS;
    progress_event.payload = progress_payload;
    progress_event.payload_length = sizeof(progress_payload);
    assert(pxa_store_parse_download_progress(&progress_event, &progress_request,
                                              &received, &total));
    assert(progress_request == 42 && received == 50 && total == 100);
    progress_payload[4] = 101;
    assert(!pxa_store_parse_download_progress(&progress_event, &progress_request,
                                               &received, &total));
    static const char ticket[] =
        "/api/v2/artifacts/3/download?expires=1790157630&signature="
        "3c93b01415b64e5f12741f0ca5918a99a9a49afcac761c648a0fc00b629f95ad";
    static const char detail[] =
        "{\"payload\":{\"item\":{\"app_id\":\"demo\",\"size\":538882,"
        "\"download_ticket_url\":\""
        "/api/v2/artifacts/3/download?expires=1790157630&signature="
        "3c93b01415b64e5f12741f0ca5918a99a9a49afcac761c648a0fc00b629f95ad"
        "\"}}}";
    store_app_t app = {0};
    char url[512];
    assert(store_client_profile_for_device(url, sizeof(url), "esp32-s3", "xtensa", "wamr", 3));
    assert(strcmp(url, "esp32-s3-wamr-2.4.0") == 0);
    assert(store_client_profile_for_device(url, sizeof(url), "esp32-s31", "riscv32", "wamr", 3));
    assert(strcmp(url, "esp32-s31-wamr-2.4.0") == 0);
    assert(store_client_profile_for_device(url, sizeof(url), "linux-x86_64", "x86_64", "wamr", 3));
    assert(strcmp(url, "linux-x86_64-wamr-2.4.0") == 0);
    assert(!store_client_profile_for_device(url, sizeof(url), "esp32-s31", "xtensa", "wamr", 3));
    assert(store_client_build_catalog_url(url, sizeof(url), "esp32-s3-wamr-2.4.0", "", 0, "", "", "", 6));
    assert(strstr(url, "page_size=6") != NULL);
    assert(!store_client_build_catalog_url(url, sizeof(url), "esp32-s3-wamr-2.4.0", "", 0, "", "", "", 0));
    assert(!store_client_build_catalog_url(url, sizeof(url), "esp32-s3-wamr-2.4.0", "", 0, "", "", "", 13));
    assert(store_json_parse_app_detail((const uint8_t *)detail,
                                       sizeof(detail) - 1u, &app));
    assert(strcmp(app.download_ticket_url, ticket) == 0);
    assert(store_client_build_download_url(url, sizeof(url), ticket));
    assert(strncmp(url, PXA_STORE_ORIGIN, strlen(PXA_STORE_ORIGIN)) == 0);
    assert(strcmp(url + strlen(PXA_STORE_ORIGIN), ticket) == 0);
    assert(!store_client_build_download_url(url, sizeof(url),
           "https://evil.example/api/v2/artifacts/3/download?expires=1&signature=abcd"));
    assert(!store_client_build_download_url(url, sizeof(url),
           "/api/v2/artifacts/../download?expires=1&signature=abcd"));
    assert(!store_client_build_download_url(url, sizeof(url),
           "/api/v2/artifacts/3/download?expires=1&signature=abcd"));
    assert(!store_client_build_download_url(url, sizeof(url),
           "/api/v2/artifacts/3/download?expires=1790157630&signature="
           "3c93b01415b64e5f12741f0ca5918a99a9a49afcac761c648a0fc00b629f95ad#x"));
    assert(!store_client_build_download_url(url, sizeof(url),
           "/api/v2/artifacts/3/../download?expires=1790157630&signature="
           "3c93b01415b64e5f12741f0ca5918a99a9a49afcac761c648a0fc00b629f95ad"));
    assert(!store_client_build_download_url(url, 10u, ticket));
    return 0;
}
