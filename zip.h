#ifndef ZIP_H
#define ZIP_H

// .zip-ondersteuning voor roms/ en dsk/: een zip wordt bij selectie éénmalig
// uitgepakt naar cache/ op de kaart, waarna alle bestaande paden (RAM-load,
// flash-staging, sector-writes op .dsk) ongewijzigd werken. Streaming
// inflate (miniz/tinfl) met een door de aanroeper geleverd 32KB-venster —
// op de Pico is dat de vrije staart van de vdp_arena.

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define ZIP_WINDOW_SIZE 32768

// Eindigt de naam op .zip (hoofdletterongevoelig)?
bool zip_is_zip(const char *name);

// Zoek de eerste bruikbare entry in dir/zipname met een extensie uit `exts`
// (NULL-getermineerde lijst, bv {".rom",".mx1",NULL}); mappen, __MACOSX en
// dotfiles worden overgeslagen. Retourneert de entrynaam en uitgepakte
// grootte. out_entry/out_usize mogen NULL zijn.
bool zip_find(const char *dir, const char *zipname, const char *const *exts,
              char *out_entry, size_t out_entry_n, uint32_t *out_usize);

// Pak de eerste passende entry uit naar cache/<zipbasenaam><entry-ext>.
// Bestaat het cachebestand al met dezelfde grootte, dan wordt het
// hergebruikt (geen herextractie). window32k = ZIP_WINDOW_SIZE bytes
// werkruimte van de aanroeper. De cachenaam komt in out_cache_name.
bool zip_extract_cached(const char *dir, const char *zipname,
                        const char *const *exts, uint8_t *window32k,
                        char *out_cache_name, size_t out_n);

#endif // ZIP_H
