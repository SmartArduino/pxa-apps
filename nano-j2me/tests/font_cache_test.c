/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "../system_font.c"
#include <assert.h>
static unsigned calls;
static uint8_t reply[20+40*8];
const pxa_event_t *port_rpc(uint16_t service,uint16_t opcode,const void *payload,uint32_t size) {
    assert(service==3 && opcode==13 && size==8);
    unsigned ch=pxa_load_u32(payload),width=ch=='A'+128?40:8;
    memset(reply,0,sizeof(reply));pxa_store_u16(reply+4,14);pxa_store_u16(reply+6,3);
    pxa_store_u16(reply+8,width);pxa_store_u16(reply+10,width);pxa_store_u16(reply+12,8);
    memset(reply+20,255,width*8);
    static pxa_event_t event;
    event=(pxa_event_t){3,13,1,reply,20+width*8};calls++;return &event;
}
void port_log(const char *format,...) {(void)format;}
int main(void) {
    Face *face=&faces[0];face->pixels=12;face->font.height=14;face->font.ascent=11;
    CachedGlyph *first=get(face,'A');assert(first && first->width==8 && calls==1);
    assert(get(face,'A')==first && calls==1);
    /* Same hash bucket, valid Host glyph wider than this app's cache cell. */
    assert(!get(face,'A'+128) && calls==2);
    assert(get(face,'A')==first && calls==2);
    assert(first->width==8 && first->advance==8 && first->height==14);
    /* Four repeatedly used glyphs in the old direct-mapped collision bucket
     * must stay warm without additional Host glyph RPCs. */
    const unsigned characters[]={'A','A'+256,'A'+512,'A'+768};
    for(unsigned i=0;i<4;++i)assert(get(face,characters[i]));
    unsigned warmed=calls;
    for(unsigned frame=0;frame<20;++frame)for(unsigned i=0;i<4;++i)assert(get(face,characters[i]));
    assert(calls==warmed);
    return 0;
}
