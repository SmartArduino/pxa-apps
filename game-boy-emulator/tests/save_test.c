/* Cartridge lifecycle, native RTC saves, bank wrap and borrowed Surface guards. */
#include "core.h"
#include "vendor/gnuboy/hw.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
void gb_core_test_clock(uint64_t value);
int main(int argc,char **argv){
    assert(argc==2);FILE *fp=fopen(argv[1],"rb");assert(fp);fseek(fp,0,SEEK_END);size_t size=ftell(fp);rewind(fp);
    uint8_t *rom=malloc(size);assert(rom && fread(rom,1,size,fp)==size);fclose(fp);
    gb_core_test_clock(1700000000);
    for(unsigned pass=0;pass<24;++pass){
        rom[0x143]=pass&1?0xc0:0;rom[0x146]=pass&2?3:0;rom[0x147]=pass&4?0x10:3;
        assert(!gb_core_load(rom,size));assert(gb_core_model()==(pass&1?GB_BOARD_CGB:GB_BOARD_DMG));
        for(unsigned i=0;i<10;++i)gb_core_frame(0);
        assert(gb_core_sram_size()==8192);size_t bytes=gb_core_save_size();assert(bytes==8192+(pass&4?48:0));
        uint8_t *save=malloc(bytes),*round=malloc(bytes);assert(save&&round);
        if(pass&4){cart.rtc.s=12;cart.rtc.m=34;cart.rtc.h=5;cart.rtc.d=323;cart.rtc.flags=1;}
        assert(gb_core_save_export(save,bytes));assert(save[0]==0x42&&save[1]==0x43);save[11]=pass+19;
        assert(gb_core_save_import(save,bytes));assert(gb_core_save_export(round,bytes));assert(!memcmp(save,round,bytes));
        assert(!gb_core_save_import(save,bytes-1));assert(!gb_core_save_export(round,bytes-1));
        uint32_t crc=gb_core_checksum();assert(gb_core_load(rom,1024));assert(gb_core_checksum()==crc);
        gb_core_close();assert(!gb_core_load(rom,size));assert(gb_core_save_import(save,bytes));assert(gb_core_sram()[11]==pass+19);
        if(pass&4){
            assert(cart.rtc.s==12 && cart.rtc.d==323);
            gb_core_test_clock(1700000061);gnuboy_rtc_sync();assert(cart.rtc.s==13&&cart.rtc.m==35);
            cart.rtc.flags|=0x40;gb_core_test_clock(1700000121);gnuboy_rtc_sync();assert(cart.rtc.s==13);
            gb_core_test_clock(1700000000);
        }
        /* 16-bit wrapping reads/writes must never index page 16 or trap on odd addresses. */
        writew(0xffff,0x1234);(void)readw(0xffff);writew(0xc001,0xabcd);assert(readw(0xc001)==0xabcd);
        free(save);free(round);gb_core_close();gb_core_close();
    }
    rom[0x143]=0xc0;rom[0x146]=0;rom[0x147]=3;
    static uint16_t reference[160*144],direct[327*160];
    assert(!gb_core_load(rom,size));for(unsigned i=0;i<40;++i)gb_core_frame(0);memcpy(reference,gb_core_video(),sizeof(reference));gb_core_close();
    for(unsigned i=0;i<327*160;++i)direct[i]=0xa55a;
    assert(!gb_core_load(rom,size));for(unsigned i=0;i<40;++i)gb_core_frame_to(0,direct+5*327+11,327);
    for(unsigned y=0;y<160;++y)for(unsigned x=0;x<327;++x)assert(direct[y*327+x]==(y>=5&&y<149&&x>=11&&x<171?reference[(y-5)*160+x-11]:0xa55a));
    gb_core_frame(0);assert(GB.video.buffer16==gb_core_video());gb_core_close();
    for(unsigned model=0;model<2;++model) {
        rom[0x143]=model?0xc0:0;rom[0x147]=3;
        assert(!gb_core_load(rom,size));for(unsigned i=0;i<10;++i)gb_core_frame(0);
        writeb(0xff40,0xf1);writeb(0xff4a,0);
        for(unsigned wx=0;wx<=8;++wx){writeb(0xff4b,wx);for(unsigned i=0;i<3;++i)gb_core_frame(0);}
        gb_core_close();
    }
    rom[0x147]=6;rom[0x149]=0;assert(!gb_core_load(rom,size));assert(gb_core_sram_size()==512);
    writeb(0,0x0a);writeb(0xa123,0xab);assert(readb(0xa323)==0xfb);gb_core_close();
    rom[0x149]=2;
    rom[0x147]=0xff;assert(!gb_core_validate(rom,size));rom[0x147]=0xfd;assert(gb_core_validate(rom,size));
    free(rom);puts("PASS: 24 cartridge/RTC lifecycles, clock halt, page wrap, unaligned access and Surface stride guards");
}
