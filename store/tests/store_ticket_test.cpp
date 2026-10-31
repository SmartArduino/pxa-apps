#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "store_client.h"
#include "store_json.h"
#include "store_size.hpp"
#include "pxa_store_installer.h"

int main(void) {
    uint8_t progress_payload[24] = {};
    pxa_store_token_t identity=UINT64_C(0x123400000000002a);
    pxa_store_u64(progress_payload,identity);
    pxa_store_u64(progress_payload+8,50);
    pxa_store_u64(progress_payload+16,100);
    pxa_event_t progress_event = {};
    uint64_t progress_request;
    uint64_t received, total;
    progress_event.service = PXA_SERVICE_STORE_INSTALLER;
    progress_event.opcode = PXA_STORE_DOWNLOAD_PROGRESS;
    progress_event.payload = progress_payload;
    progress_event.payload_size = sizeof(progress_payload);
    assert(pxa_store_parse_download_progress(&progress_event, &progress_request,
                                              &received, &total));
    assert(progress_request == identity && received == 50 && total == 100);
    progress_payload[8] = 101;
    assert(!pxa_store_parse_download_progress(&progress_event, &progress_request,
                                               &received, &total));
    static const char ticket[] =
        "/api/v2/artifacts/3/download?expires=1790157630&signature="
        "3c93b01415b64e5f12741f0ca5918a99a9a49afcac761c648a0fc00b629f95ad";
    static const char detail[] =
        "{\"payload\":{\"item\":{\"app_id\":\"demo\",\"size\":538882,"
        "\"requires_resources\":true,\"download_ticket_url\":\""
        "/api/v2/artifacts/3/download?expires=1790157630&signature="
        "3c93b01415b64e5f12741f0ca5918a99a9a49afcac761c648a0fc00b629f95ad"
        "\"}}}";
    store_app_t app = {};
    char url[512];
    assert(store_client_profile_for_device(url, sizeof(url), "esp32-s3", "xtensa", "wamr", 3));
    assert(strcmp(url, "esp32-s3-pxa-0.4.0-r3") == 0);
    assert(store_client_profile_for_device(url, sizeof(url), "esp32-s31", "riscv32", "wamr", 3));
    assert(strcmp(url, "esp32-s31-pxa-0.4.0-r3") == 0);
    assert(store_client_profile_for_device(url, sizeof(url), "linux-x86_64", "x86_64", "wamr", 3));
    assert(strcmp(url, "linux-x86_64-pxa-0.4.0-r3") == 0);
    assert(!store_client_profile_for_device(url, sizeof(url), "esp32-s31", "xtensa", "wamr", 3));
    const char root[]="17982acd08493944059713aee9a56135dd4a080ecaaf7aa77df11fdf3102d171";
    assert(store_client_build_identity_url(url,sizeof(url),"esp32-s31-pxa-0.4.0-r3","pxa-nes",root,""));
    assert(strstr(url,"&lineage_root_key_id=17982acd08493944"));
    assert(!store_client_build_identity_url(url,sizeof(url),"p","pxa-nes","short",""));
    assert(!store_client_build_identity_url(url,100,"p","pxa-nes",root,""));
    assert(store_client_build_catalog_url(url, sizeof(url), "esp32-s3-pxa-0.3.0", "", 0, "", "", "", "downloads", 6));
    assert(strstr(url, "page_size=6") != NULL);
    assert(strstr(url, "&sort=downloads") != NULL);
    assert(!store_client_build_catalog_url(url, sizeof(url), "p", "", 0, "", "", "", "invalid", 6));
    assert(!store_client_build_catalog_url(url, sizeof(url), "esp32-s3-pxa-0.3.0", "", 0, "", "", "", "updated", 0));
    assert(!store_client_build_catalog_url(url, sizeof(url), "esp32-s3-pxa-0.3.0", "", 0, "", "", "", "name", 13));
    assert(store_json_parse_app_detail((const uint8_t *)detail,
                                       sizeof(detail) - 1u, &app));
    assert(strcmp(app.download_ticket_url, ticket) == 0);
    assert(app.requires_resources);
    uint64_t download_size;
    assert(!app.resource_size_known && !store::package_download_size(app, download_size));
    static const char with_resources[] =
        "{\"payload\":{\"item\":{\"resource_size\":81901993,\"size\":3251965,"
        "\"requires_resources\":true,\"download_size\":85153958}}}";
    assert(store_json_parse_app_detail((const uint8_t *)with_resources,
                                       sizeof(with_resources) - 1, &app));
    assert(store::package_download_size(app, download_size) && download_size == 85153958);
    assert(app.size == 3251965 && app.resource_size == 81901993);
    static const char missing_resources[] =
        "{\"payload\":{\"item\":{\"size\":3251965,\"requires_resources\":true,"
        "\"resource_size\":null,\"download_size\":null}}}";
    assert(store_json_parse_app_detail((const uint8_t *)missing_resources,
                                       sizeof(missing_resources) - 1, &app));
    assert(!store::package_download_size(app, download_size) && !app.resource_size_known);
    static const char code_only[] = "{\"payload\":{\"item\":{\"size\":1234}}}";
    assert(store_json_parse_app_detail((const uint8_t *)code_only, sizeof(code_only) - 1, &app));
    assert(store::package_download_size(app, download_size) && download_size == 1234);
    assert(!app.requires_resources && !app.resource_size_known && app.resource_size == 0);
    assert(store_client_profile_for_device(url, sizeof(url), "esp32-s31", "riscv32", "wamr", 3, 3));
    assert(strcmp(url, "esp32-s31-pxa-0.5.0-r2") == 0);
    assert(store_client_profile_for_device(url, sizeof(url), "esp32-s31", "riscv32", "wamr", 3,
        PXA_STORE_CAP_RESOURCE_INSTALL | PXA_STORE_CAP_VOLUME_PLACEMENT | PXA_STORE_CAP_INDEXED_CONTAINER));
    assert(strcmp(url, "esp32-s31-pxa-0.7.0") == 0);
    assert(store_client_profile_for_device(url, sizeof(url), "esp32-s3", "xtensa", "wamr", 3,
        PXA_STORE_CAP_INDEXED_CONTAINER));
    assert(strcmp(url, "esp32-s3-pxa-0.4.0-r3") == 0);
    assert(store_client_profile_for_device(url, sizeof(url), "esp32-s31", "riscv32", "wamr", 3, 1));
    assert(strcmp(url, "esp32-s31-pxa-0.4.0-r3") == 0);
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
