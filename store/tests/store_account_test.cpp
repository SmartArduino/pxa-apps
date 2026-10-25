#include "pxa_control_capture.h"
#include "store_account.hpp"
#include "store_json.h"
#include "pxa_net.h"
#include "pxa_storage.h"
#include <cassert>
#include <cstring>
#include <string>
#include <algorithm>

static uint64_t network_request, storage_request;
static unsigned requests, closed;
static uint16_t method;
static std::string url, auth, data, response, saved;
static size_t offset;
static bool block_once;
int32_t pxa_test_control(const uint8_t *bytes, uint32_t size) {
    pxa_event_t event{}; assert(pxa_parse_event(bytes, size, &event));
    if (event.service == PXA_NET_SERVICE) {
        network_request = event.token; ++requests;
        url.clear(); auth.clear(); data.clear();
        size_t pos = 0;
        while (pos < event.payload_size) {
            pxa_wire_record_view_t r; size_t n;
            assert(pxa_wire_record_decode(event.payload + pos, event.payload_size - pos, &r, &n)); pos += n;
            if (r.tag == 1) url.assign(reinterpret_cast<const char *>(r.payload), r.payload_size);
            if (r.tag == 2) method = pxa_load_u16(r.payload);
            if (r.tag == 10) data.assign(reinterpret_cast<const char *>(r.payload), r.payload_size);
            if (r.tag == 9) {
                pxa_wire_record_view_t name, value; size_t a,b;
                assert(pxa_wire_record_decode(r.payload,r.payload_size,&name,&a));
                assert(pxa_wire_record_decode(r.payload+a,r.payload_size-a,&value,&b));
                if (name.payload_size == 13 && !memcmp(name.payload,"authorization",13))
                    auth.assign(reinterpret_cast<const char *>(value.payload),value.payload_size);
            }
        }
        assert(url.starts_with(PXA_STORE_ORIGIN "/api/"));
    } else if (event.service == PXA_STORAGE_SERVICE) {
        storage_request = event.token;
        if (event.opcode == PXA_STORAGE_SET) {
            pxa_wire_record_view_t record; size_t n;
            assert(pxa_wire_record_decode(event.payload, event.payload_size, &record, &n));
            assert(pxa_wire_record_decode(event.payload+n, event.payload_size-n, &record, &n));
            saved.assign(reinterpret_cast<const char *>(record.payload), record.payload_size);
        } else if (event.opcode == PXA_STORAGE_REMOVE) saved.clear();
    } else { assert(event.service == PXA_CORE_SERVICE); if (event.opcode == PXA_CORE_CLOSE_HANDLE) ++closed; }
    return 0;
}
extern "C" int32_t pxa_io(uint64_t handle, uint32_t operation, uint8_t *out, uint32_t length) {
    assert(handle == UINT64_C(0xabc0000000000001) && operation == PXA_NET_IO_READ);
    if (block_once) { block_once = false; return PXA_STATUS_WOULD_BLOCK; }
    size_t count = std::min<size_t>({response.size() - offset, length, 31});
    memcpy(out, response.data() + offset, count); offset += count; return count;
}
static void complete(store::Account &account, unsigned status, std::string text, uint64_t now) {
    response = text; offset = 0;
    uint8_t payload[96]{}; size_t pos=4, n=0; uint8_t scalar[8]{};
    pxa_store_u16(scalar,status); assert(pxa_wire_record_encode(payload+pos,sizeof(payload)-pos,5,scalar,2,&n));pos+=n;
    assert(pxa_wire_record_encode(payload+pos,sizeof(payload)-pos,6,reinterpret_cast<const uint8_t *>("application/json"),16,&n));pos+=n;
    pxa_store_u64(scalar,UINT64_C(0xabc0000000000001));assert(pxa_wire_record_encode(payload+pos,sizeof(payload)-pos,7,scalar,8,&n));pos+=n;
    pxa_store_u64(scalar,text.size());assert(pxa_wire_record_encode(payload+pos,sizeof(payload)-pos,12,scalar,8,&n));pos+=n;
    pxa_store_u32(scalar,3);assert(pxa_wire_record_encode(payload+pos,sizeof(payload)-pos,13,scalar,4,&n));pos+=n;
    pxa_event_t event{PXA_NET_SERVICE,PXA_NET_HTTP_REQUEST,network_request,payload,(uint32_t)pos};
    assert(account.event(event,now));
}
static void restore(store::Account &account, const std::string &token, uint64_t now) {
    uint8_t payload[128]{}; size_t n=0;
    if (token.empty()) pxa_store_u32(payload,static_cast<uint32_t>(PXA_STATUS_NOT_FOUND));
    else assert(pxa_wire_record_encode(payload+4,sizeof(payload)-4,2,
                                     reinterpret_cast<const uint8_t *>(token.data()),token.size(),&n));
    pxa_event_t event{PXA_STORAGE_SERVICE,PXA_STORAGE_GET,storage_request,payload,(uint32_t)(4+n)};
    assert(account.event(event,now));
}
int main() {
    store_account_response_t result{};
    const char *duplicate = "{\"count\":1,\"count\":2,\"liked\":true}";
    assert(!store_json_parse_account(reinterpret_cast<const uint8_t *>(duplicate),strlen(duplicate),&result));
    store::Account account;
    account.start(); restore(account,"",0);
    account.network(UINT64_C(0xabc0000000000002),0);
    assert(account.clock_period() == 0 && requests == 0);
    account.login(0); assert(url.ends_with("/authorize") && auth.empty() && method == PXA_NET_POST);
    const std::string secret = "pxac_" + std::string(43,'a');
    const std::string token = "pxad_" + std::string(43,'b');
    complete(account,200,"{\"device_code\":\""+secret+"\",\"user_code\":\"ABCD-EFGH\",\"interval\":5,\"expires_in\":600}",0);
    assert(account.phase == store::AccountPhase::waiting && account.clock_period() == 1000);
    unsigned before = requests;
    account.tick(4999); assert(requests == before);
    account.tick(5000); assert(requests == before+1 && url.ends_with("/token") && auth.empty());
    complete(account,400,"{\"error\":\"authorization_pending\"}",5000);
    account.tick(10000);
    complete(account,200,"{\"access_token\":\""+token+"\",\"display_name\":\"测试用户\"}",10000);
    assert(account.authenticated() && account.clock_period() == 0 && saved == token);
    store_app_t item{}; strcpy(item.app_id,"pxa-nes");memset(item.lineage_root_key_id,'a',64);item.lineage_root_key_id[64]=0;
    account.target(&item,10000); assert(auth == "Bearer "+token && method == PXA_NET_GET);
    block_once=true;
    complete(account,200,"{\"count\":7,\"liked\":false}",10000);
    assert(account.clock_period()==50);account.tick(10050);
    assert(account.like_ready && account.like_count==7 && !account.liked && account.clock_period()==0);
    account.toggle_like(10050);assert(method==PXA_NET_PUT);
    assert(!account.liked); // Only a confirmed response changes state.
    complete(account,200,"{\"count\":8,\"liked\":true}",10050);
    assert(account.liked && account.like_count==8);
    account.toggle_like(10100);assert(method==PXA_NET_DELETE);
    complete(account,500,"{}",10100);assert(account.liked && account.like_error);
    account.toggle_like(10200);assert(method==PXA_NET_GET);
    store_app_t other=item;strcpy(other.app_id,"another");
    account.target(&other,10200);
    complete(account,200,"{\"count\":99,\"liked\":true}",10200);
    assert(!account.like_ready && url.find("another/likes")!=std::string::npos);
    complete(account,200,"{\"count\":2,\"liked\":false}",10200);
    assert(account.like_count==2 && !account.liked);
    account.logout(10300);assert(method==PXA_NET_DELETE && url.ends_with("/session"));
    assert(!account.has_credential() && saved.empty());
    complete(account,200,"{\"revoked\":true}",10300);
    account.target(nullptr,10300);before=requests;account.tick(9999999);assert(requests==before && account.clock_period()==0);
    account.stop();
    store::Account restored;
    restored.start(); restore(restored,token,0);restored.network(UINT64_C(0xabc0000000000002),0);
    assert(url.ends_with("/session") && auth=="Bearer "+token);
    complete(restored,401,"{}",0); assert(!restored.has_credential() && saved.empty());
    restored.login(0);complete(restored,200,"{\"device_code\":\""+secret+"\",\"user_code\":\"ABCD-EFGH\",\"interval\":5,\"expires_in\":600}",0);
    restored.tick(600001);assert(restored.phase==store::AccountPhase::expired && restored.clock_period()==0);
    restored.stop();
    puts("Device account: authorization, pacing, private storage, restore/revoke, likes, stale replies, stream draining and idle clock passed");
}
