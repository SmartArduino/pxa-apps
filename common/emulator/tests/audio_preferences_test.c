/* Storage ordering and restart semantics, using real wire codecs. */
#include "../audio_preferences.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned gets, sets, applies;
static bool applied, last_write;
static int32_t submit_error;
int32_t pxa_call(uint32_t route, uint64_t token, const uint8_t *data, uint32_t size) {
    assert((route & 0xffff) == PXA_STORAGE_SERVICE);
    unsigned op = route >> 16;
    if (op == PXA_STORAGE_GET) { assert(token == EMULATOR_AUDIO_LOAD); ++gets; }
    else {
        assert(op == PXA_STORAGE_SET && token == EMULATOR_AUDIO_SAVE);
        pxa_wire_record_view_t record; size_t consumed;
        assert(pxa_wire_record_decode(data,size,&record,&consumed));
        assert(record.raw_tag == 1 && record.payload_size == sizeof(emulator_audio_key)-1);
        assert(!memcmp(record.payload,emulator_audio_key,record.payload_size));
        data+=consumed; size-=consumed;
        assert(pxa_wire_record_decode(data,size,&record,&consumed));
        assert(record.raw_tag == 2 && record.payload_size == 1);
        last_write=record.payload[0]; ++sets;
    }
    return submit_error;
}
static void apply(bool value) { ++applies; applied=value; }
static void reply(emulator_audio_preferences_t *p, bool get, int32_t status, int value) {
    uint8_t data[32]; pxa_store_u32(data,(uint32_t)status); size_t n=0;
    if (get && !status) { uint8_t v=(uint8_t)value; assert(pxa_wire_record_encode(data+4,28,2,&v,1,&n)); }
    pxa_event_t e={.service=PXA_STORAGE_SERVICE,.opcode=get?PXA_STORAGE_GET:PXA_STORAGE_SET,
        .token=get?EMULATOR_AUDIO_LOAD:EMULATOR_AUDIO_SAVE,.payload=data,.payload_size=4+(uint32_t)n};
    assert(emulator_audio_preferences_event(p,&e,apply));
}
int main(void) {
    emulator_audio_preferences_t p;
    emulator_audio_preferences_start(&p,apply);
    assert(gets==1 && !applies && !sets);
    reply(&p,true,PXA_STATUS_NOT_FOUND,0); assert(applied && p.ready && !sets);
    emulator_audio_preferences_set(&p,false); assert(sets==1 && !last_write && p.saving);
    reply(&p,false,0,0); assert(!p.saving && p.persisted_valid && !p.persisted);
    emulator_audio_preferences_start(&p,apply); reply(&p,true,0,0);
    assert(!applied && !p.enabled && sets==1);
    emulator_audio_preferences_start(&p,apply);
    emulator_audio_preferences_set(&p,false); reply(&p,false,0,0);
    unsigned old=applies; reply(&p,true,0,1);
    assert(!p.enabled && !p.persisted && applies==old);
    emulator_audio_preferences_set(&p,true); unsigned pending=sets;
    emulator_audio_preferences_set(&p,false); assert(sets==pending);
    reply(&p,false,0,0); assert(p.saving && !last_write && sets==pending+1);
    reply(&p,false,0,0); assert(!p.saving && !p.persisted);
    emulator_audio_preferences_set(&p,true); reply(&p,false,PXA_STATUS_QUOTA_EXCEEDED,0);
    assert(p.error && !p.saving && p.enabled && !p.persisted);
    submit_error=PXA_STATUS_UNSUPPORTED;
    emulator_audio_preferences_start(&p,apply); assert(applied && p.ready && p.error);
    puts("PASS: default sound, remembered mute, late load, coalesced toggles and storage failure");
}
