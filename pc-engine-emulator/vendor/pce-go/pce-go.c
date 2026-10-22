// pce-go.c - Entry file to start/stop/reset/save emulation
//
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "pce-go.h"
#include "gfx.h"
#include "psg.h"
#include "pce.h"

/**
 * Save state file description.
 */
#define TWO_PART_ROM 0x0001
#define ONBOARD_RAM  0x0100
#define US_ENCODED   0x0010

static const struct {
	const uint32_t CRC;
	const char *Name;
	const uint32_t Flags;
} romFlags[] = {
	{0xF0ED3094, "Blazing Lazers", TWO_PART_ROM},
	{0xB4A1B0F6, "Blazing Lazers", TWO_PART_ROM},
	{0x55E9630D, "Legend of Hero Tonma", US_ENCODED},
	{0x083C956A, "Populous", ONBOARD_RAM},
	{0x0A9ADE99, "Populous", ONBOARD_RAM},
	{0x00000000, "Unknown", 0},
};

static bool running = false;

/**
 * Load card into memory and set its memory map
 * NOTE: This function takes ownership of `data`
 */
static int load_card(uint8_t *data,size_t size,bool paged,uint32_t crc)
{
	if ((!data && !paged) || size < 0x2000 || size > 0x1000000)
	{
		MESSAGE_ERROR("Invalid rom data received\n");
		return -1;
	}

	/* Existing image is released by the PXA frontend before replacement. */

	int offset = size & 0x1fff;

	// read ROM
	PCE.ROM = data;
	PCE.ROM_SIZE = (size - offset) / 0x2000;
	PCE.ROM_DATA = paged?NULL:PCE.ROM+offset;
	PCE.ROM_CRC = paged?crc:crc32_le(0,PCE.ROM,size);

	uint32_t IDX = 0;
	uint32_t ROM_MASK = 1;

	while (ROM_MASK < PCE.ROM_SIZE) ROM_MASK <<= 1;
	ROM_MASK--;

	MESSAGE_INFO("ROM LOADED: OFFSET=%d, BANKS=%d, MASK=%03X, CRC=%08X\n",
		offset, (int)PCE.ROM_SIZE, (int)ROM_MASK, (int)PCE.ROM_CRC);

	while (romFlags[IDX].CRC) {
		if (PCE.ROM_CRC == romFlags[IDX].CRC)
			break;
		IDX++;
	}

	MESSAGE_INFO("Game Name: %s\n", romFlags[IDX].Name);

	if(paged && (romFlags[IDX].Flags & US_ENCODED)){extern void pxa_pce_rom_encoded(bool);pxa_pce_rom_encoded(true);}
	// US Encrypted
	if (!paged && ((romFlags[IDX].Flags & US_ENCODED) || PCE.ROM_DATA[0x1FFF] < 0xE0))
	{
		MESSAGE_INFO("This rom is probably US encrypted, decrypting...\n");

		unsigned char inverted_nibble[16] = {
			0, 8, 4, 12, 2, 10, 6, 14,
			1, 9, 5, 13, 3, 11, 7, 15
		};

		for (int x = 0; x < PCE.ROM_SIZE * 0x2000; x++) {
			unsigned char temp = PCE.ROM_DATA[x] & 15;

			PCE.ROM_DATA[x] &= ~0x0F;
			PCE.ROM_DATA[x] |= inverted_nibble[PCE.ROM_DATA[x] >> 4];

			PCE.ROM_DATA[x] &= ~0xF0;
			PCE.ROM_DATA[x] |= inverted_nibble[temp] << 4;
		}
	}

	// For example with Devil Crush 512Ko
	if (romFlags[IDX].Flags & TWO_PART_ROM)
		PCE.ROM_SIZE = 0x30;

	// Game ROM
	memset(pxa_pce_rom_banks,255,sizeof(pxa_pce_rom_banks));
	for (int i = 0; i < 0x80; i++) {
		if (PCE.ROM_SIZE == 0x30) {
			switch (i & 0x70) {
			case 0x00:
			case 0x10:
			case 0x50:
				pxa_pce_rom_banks[i] = ((i & ROM_MASK) % PCE.ROM_SIZE);
                PCE.MemoryMapR[i] = paged?PCE.NULLRAM:PCE.ROM_DATA+(size_t)pxa_pce_rom_banks[i]*8192;
				break;
			case 0x20:
			case 0x60:
				pxa_pce_rom_banks[i] = ((i - 0x20) & ROM_MASK);
                PCE.MemoryMapR[i] = paged?PCE.NULLRAM:PCE.ROM_DATA+(size_t)pxa_pce_rom_banks[i]*8192;
				break;
			case 0x30:
			case 0x70:
				pxa_pce_rom_banks[i] = ((i - 0x10) & ROM_MASK);
                PCE.MemoryMapR[i] = paged?PCE.NULLRAM:PCE.ROM_DATA+(size_t)pxa_pce_rom_banks[i]*8192;
				break;
			case 0x40:
				pxa_pce_rom_banks[i] = ((i - 0x20) & ROM_MASK);
                PCE.MemoryMapR[i] = paged?PCE.NULLRAM:PCE.ROM_DATA+(size_t)pxa_pce_rom_banks[i]*8192;
				break;
			}
		} else {
			pxa_pce_rom_banks[i] = ((i & ROM_MASK) % PCE.ROM_SIZE);
                PCE.MemoryMapR[i] = paged?PCE.NULLRAM:PCE.ROM_DATA+(size_t)pxa_pce_rom_banks[i]*8192;
		}
		PCE.MemoryMapW[i] = PCE.NULLRAM;
	}

	// Allocate the card's onboard ram
	if (romFlags[IDX].Flags & ONBOARD_RAM) {
        for(unsigned i=0x40;i<0x44;++i)pxa_pce_rom_banks[i]=-1;
		if (!PCE.ExRAM)
			PCE.ExRAM = calloc(1,0x8000);
		if (!PCE.ExRAM) return -1;
		PCE.MemoryMapR[0x40] = PCE.MemoryMapW[0x40] = PCE.ExRAM;
		PCE.MemoryMapR[0x41] = PCE.MemoryMapW[0x41] = PCE.ExRAM + 0x2000;
		PCE.MemoryMapR[0x42] = PCE.MemoryMapW[0x42] = PCE.ExRAM + 0x4000;
		PCE.MemoryMapR[0x43] = PCE.MemoryMapW[0x43] = PCE.ExRAM + 0x6000;
	}

	// Mapper for roms >= 1.5MB (SF2, homebrews)
	if (PCE.ROM_SIZE >= 192)
		PCE.MemoryMapW[0x00] = PCE.IOAREA;

	ResetPCE(0);

	return 0;
}

int16_t pxa_pce_rom_banks[128];
int LoadCard(uint8_t *data,size_t size){return load_card(data,size,false,0);}
int LoadCardPaged(size_t size,uint32_t crc){return load_card(NULL,size,true,crc);}


/**
 * Load card into memory and set its memory map
 */



/**
 * Reset the emulator
 */
void
ResetPCE(bool hard)
{
	gfx_reset(hard);
	pce_reset(hard);
}


/**
 * Initialize the emulator (allocate memory, call osd_init* functions)
 */
int
InitPCE(int samplerate, bool stereo)
{
	if (gfx_init())
		return 1;

	if (psg_init(samplerate, stereo))
		return 1;

	if (pce_init())
		return 1;

	return 0;
}


/**
 * Returns a 256 colors palette in the chosen depth
 */



/**
 * Start the emulation
 */



/**
 * Load saved state
 */



/**
 * Save current state
 */



/**
 * Cleanup and quit (not used in retro-go)
 */
void
ShutdownPCE()
{
	gfx_term();
	psg_term();
	pce_term();
}
