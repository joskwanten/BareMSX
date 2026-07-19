#ifndef WD2793_H
#define WD2793_H

// WD2793 floppy disk controller + één drive, zoals bedraad in de Philips
// NMS-8245 (en de meeste Philips/Sony MSX'en): de FDC-registers zijn
// memory-mapped bovenin de DISK.ROM-pagina (0x7FF8-0x7FFF).
// Port van msx_rs/src/fdc.rs, met één verschil: het .DSK-image zit niet in
// RAM maar achter sector-callbacks — op de Pico worden sectors on-demand
// van de SD-kaart gelezen/geschreven (een 720KB-image past niet in RAM).
//
// Registerkaart (Philips-interface; openMSX `PhilipsFDC`-layout):
//   0x7FF8  R: status      W: command
//   0x7FF9  R/W: track     0x7FFA R/W: sector    0x7FFB R/W: data
//   0x7FFC  R/W: side (bit 0)    0x7FFD R/W: drive/motor
//   0x7FFF  R: bit 7 = !DRQ, bit 6 = !INTRQ — het poll-doel van de driver
//
// Commando's zijn meteen klaar: de seek/rotatie-vertragingen van de echte
// WD2793 bestaan om mechanische redenen die een sector-image niet heeft.

#include <stdint.h>
#include <stdbool.h>

#define WD_SECTOR_SIZE 512

// Sector-IO: lees of schrijf 512-byte sector `lba` (0-based, logisch:
// (track * sides + side) * 9 + sector-1). Return 0 bij succes, <0 bij fout.
typedef int (*wd_sector_io_t)(void *ctx, uint32_t lba, uint8_t *buf, bool write);

typedef struct {
    // Disk-geometrie: sides == 0 betekent "geen disk in de drive".
    uint8_t sides;
    uint32_t total_sectors;
    void *io_ctx;
    wd_sector_io_t io;

    // FDC-registers.
    uint8_t track, sector, data, status;
    uint8_t side;   // side-select latch (0x7FFC bit 0)
    uint8_t drive;  // drive-select/motor latch (0x7FFD); bit 0 = drive B = afwezig
    bool type1_status; // statusregister-layout van het laatste commando
    bool intrq;
    bool index_flip;   // index-puls-toggle voor type-I statusbit 1

    // Lopende transfer door het dataregister.
    enum { WD_T_NONE, WD_T_READ, WD_T_WRITE, WD_T_FORMAT } tmode;
    uint16_t tpos, tlen;
    uint32_t tlba;             // doelsector van een write
    uint8_t buf[WD_SECTOR_SIZE];
} wd2793_t;

// sides = 0 -> lege drive (drive not ready). io mag dan NULL zijn.
void wd2793_init(wd2793_t *fdc, uint8_t sides, uint32_t total_sectors,
                 void *io_ctx, wd_sector_io_t io);

// Memory-mapped registertoegang; addr wordt op de laatste 3 bits gemaskeerd.
uint8_t wd2793_read(wd2793_t *fdc, uint16_t addr);
void wd2793_write(wd2793_t *fdc, uint16_t addr, uint8_t value);

#endif // WD2793_H
