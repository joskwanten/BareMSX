// Zie zip.h. Eigen minimale zip-parser (EOCD -> central directory -> local
// header) bovenop storage_read_at, met miniz/tinfl als inflate-motor. Alleen
// lezen; methods "stored" (0) en "deflate" (8). CRC32 wordt gecontroleerd —
// een SD-leesfout mag nooit stil een corrupt ROM/dsk opleveren.

#include "zip.h"
#include "storage.h"
#include "miniz.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// ---- helpers ----

static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static bool ieq(char a, char b)
{
    if (a >= 'A' && a <= 'Z') a += 32;
    if (b >= 'A' && b <= 'Z') b += 32;
    return a == b;
}

static bool ends_with(const char *s, const char *suffix)
{
    size_t ls = strlen(s), lf = strlen(suffix);
    if (lf > ls) return false;
    for (size_t i = 0; i < lf; i++)
        if (!ieq(s[ls - lf + i], suffix[i])) return false;
    return true;
}

bool zip_is_zip(const char *name) { return ends_with(name, ".zip"); }

// ---- zip-structuur ----

#define EOCD_SIG  0x06054b50u
#define CDH_SIG   0x02014b50u
#define LFH_SIG   0x04034b50u

typedef struct {
    char name[STORAGE_MAX_NAME];
    uint16_t method;      // 0 = stored, 8 = deflate
    uint32_t crc32;
    uint32_t csize, usize;
    uint32_t data_off;    // absolute offset van de data in het zipbestand
} zip_entry_t;

// Vind de End Of Central Directory. Meestal de laatste 22 bytes (geen
// comment); anders tot 66KB terugscannen in blokken.
static bool find_eocd(const char *dir, const char *zipname, long fsize,
                      uint32_t *cd_off, uint32_t *cd_count)
{
    uint8_t buf[512 + 3]; // +3: signature kan een blokgrens overlappen
    long max_back = fsize < 66000 ? fsize : 66000;
    for (long back = 22; back <= max_back; back += 512 - 3) {
        long off = fsize - back;
        long want = back < (long)sizeof buf ? back : (long)sizeof buf;
        if (off < 0) { want += off; off = 0; }
        if (storage_read_at(dir, zipname, (uint32_t)off, buf, (size_t)want) != want)
            return false;
        for (long i = want - 22; i >= 0; i--) {
            if (rd32(&buf[i]) == EOCD_SIG) {
                *cd_count = rd16(&buf[i + 10]);
                *cd_off = rd32(&buf[i + 16]);
                return true;
            }
        }
    }
    return false;
}

static bool name_wanted(const char *name, const char *const *exts)
{
    size_t n = strlen(name);
    if (n == 0 || name[n - 1] == '/') return false;      // map
    if (strstr(name, "__MACOSX")) return false;          // macOS-resourcejunk
    const char *base = strrchr(name, '/');
    base = base ? base + 1 : name;
    if (base[0] == '.') return false;                    // dotfile
    for (int i = 0; exts[i]; i++)
        if (ends_with(name, exts[i])) return true;
    return false;
}

// Loop de central directory af; retourneert de (skip+1)-de passende entry.
static bool find_entry_n(const char *dir, const char *zipname,
                         const char *const *exts, int skip, zip_entry_t *e)
{
    long fsize = storage_size(dir, zipname);
    if (fsize < 22) return false;
    uint32_t cd_off, count;
    if (!find_eocd(dir, zipname, fsize, &cd_off, &count)) return false;

    uint8_t h[46];
    uint32_t off = cd_off;
    for (uint32_t i = 0; i < count; i++) {
        if (storage_read_at(dir, zipname, off, h, 46) != 46) return false;
        if (rd32(h) != CDH_SIG) return false;
        uint16_t nlen = rd16(&h[28]), xlen = rd16(&h[30]), clen = rd16(&h[32]);

        char name[STORAGE_MAX_NAME];
        uint16_t copy = nlen < sizeof name - 1 ? nlen : (uint16_t)(sizeof name - 1);
        if (storage_read_at(dir, zipname, off + 46, (uint8_t *)name, copy) != copy)
            return false;
        name[copy] = 0;

        if (nlen == copy && name_wanted(name, exts) && skip-- == 0) {
            e->method = rd16(&h[10]);
            e->crc32 = rd32(&h[16]);
            e->csize = rd32(&h[20]);
            e->usize = rd32(&h[24]);
            uint32_t lho = rd32(&h[42]);
            snprintf(e->name, sizeof e->name, "%s", name);
            if (e->method != 0 && e->method != 8) goto next; // exotisch: overslaan

            // Local header: de data begint na diens (eigen!) naam+extra.
            uint8_t lh[30];
            if (storage_read_at(dir, zipname, lho, lh, 30) != 30) return false;
            if (rd32(lh) != LFH_SIG) return false;
            e->data_off = lho + 30 + rd16(&lh[26]) + rd16(&lh[28]);
            return true;
        }
    next:
        off += 46u + nlen + xlen + clen;
    }
    return false;
}

bool zip_find(const char *dir, const char *zipname, const char *const *exts,
              char *out_entry, size_t out_entry_n, uint32_t *out_usize)
{
    zip_entry_t e;
    if (!find_entry_n(dir, zipname, exts, 0, &e)) return false;
    if (out_entry) snprintf(out_entry, out_entry_n, "%s", e.name);
    if (out_usize) *out_usize = e.usize;
    return true;
}

// ---- extractie ----

// cache-naam: <zipbasenaam zonder .zip><extensie van de entry>
static void cache_name_for(const char *zipname, const char *entry,
                           char *out, size_t n)
{
    char base[STORAGE_MAX_NAME];
    snprintf(base, sizeof base, "%s", zipname);
    size_t bl = strlen(base);
    if (bl > 4) base[bl - 4] = 0; // ".zip" eraf
    const char *dot = strrchr(entry, '.');
    snprintf(out, n, "%s%s", base, dot ? dot : "");
}

#define IN_CHUNK_ZIP 4096

static bool inflate_to_cache(const char *dir, const char *zipname,
                             const zip_entry_t *e, uint8_t *window,
                             const char *cache_name)
{
    tinfl_decompressor *inf = malloc(sizeof *inf);
    uint8_t *inbuf = malloc(IN_CHUNK_ZIP);
    bool ok = false;
    if (!inf || !inbuf) goto out;
    tinfl_init(inf);

    uint32_t in_off = e->data_off, in_left = e->csize;
    size_t avail_in = 0;
    const uint8_t *next_in = inbuf;
    size_t dict_ofs = 0;
    uint32_t written = 0;
    uint32_t crc = MZ_CRC32_INIT;

    for (;;) {
        if (avail_in == 0 && in_left) {
            size_t want = in_left < IN_CHUNK_ZIP ? in_left : IN_CHUNK_ZIP;
            if (storage_read_at(dir, zipname, in_off, inbuf, want) != (long)want)
                goto out;
            in_off += (uint32_t)want;
            in_left -= (uint32_t)want;
            next_in = inbuf;
            avail_in = want;
        }

        size_t in_bytes = avail_in;
        size_t out_bytes = ZIP_WINDOW_SIZE - dict_ofs;
        tinfl_status st = tinfl_decompress(inf, next_in, &in_bytes,
                                           window, window + dict_ofs, &out_bytes,
                                           in_left ? TINFL_FLAG_HAS_MORE_INPUT : 0);
        next_in += in_bytes;
        avail_in -= in_bytes;
        dict_ofs += out_bytes;

        // Venster vol -> als blok wegschrijven (weinig, grote writes: FatFs
        // opent/sluit per write). Bij DONE het restant [0..dict_ofs).
        bool done = (st == TINFL_STATUS_DONE);
        if (dict_ofs == ZIP_WINDOW_SIZE || (done && dict_ofs)) {
            crc = (uint32_t)mz_crc32(crc, window, dict_ofs);
            if (storage_write_at(SD_CACHE, cache_name, written, window, dict_ofs) != (long)dict_ofs)
                goto out;
            written += (uint32_t)dict_ofs;
            dict_ofs = 0;
        }
        if (done) break;
        if (st < TINFL_STATUS_DONE) goto out; // corrupt
        if (st == TINFL_STATUS_NEEDS_MORE_INPUT && !in_left && !avail_in)
            goto out; // afgekapt bestand
    }

    ok = (written == e->usize) && (crc == e->crc32);
out:
    free(inbuf);
    free(inf);
    return ok;
}

static bool copy_stored_to_cache(const char *dir, const char *zipname,
                                 const zip_entry_t *e, uint8_t *window,
                                 const char *cache_name)
{
    uint32_t left = e->csize, off = 0;
    uint32_t crc = MZ_CRC32_INIT;
    while (left) {
        uint32_t n = left < ZIP_WINDOW_SIZE ? left : ZIP_WINDOW_SIZE;
        if (storage_read_at(dir, zipname, e->data_off + off, window, n) != (long)n)
            return false;
        crc = (uint32_t)mz_crc32(crc, window, n);
        if (storage_write_at(SD_CACHE, cache_name, off, window, n) != (long)n)
            return false;
        off += n;
        left -= n;
    }
    return crc == e->crc32;
}

// Pak één gevonden entry uit naar cache/cname (met hergebruik-check).
static bool extract_entry(const char *dir, const char *zipname,
                          const zip_entry_t *e, uint8_t *window,
                          const char *cname)
{
    if (storage_size(SD_CACHE, cname) == (long)e->usize)
        return true; // al uitgepakt
    if (!storage_create(SD_CACHE, cname)) return false;
    bool ok = (e->method == 0)
        ? copy_stored_to_cache(dir, zipname, e, window, cname)
        : inflate_to_cache(dir, zipname, e, window, cname);
    if (!ok)
        printf("[zip] uitpakken van %s (%s) mislukt\n", zipname, e->name);
    return ok;
}

int zip_extract_all_cached(const char *dir, const char *zipname,
                           const char *const *exts, uint8_t *window32k,
                           char names[][128], int max)
{
    char base[STORAGE_MAX_NAME];
    snprintf(base, sizeof base, "%s", zipname);
    size_t bl = strlen(base);
    if (bl > 4) base[bl - 4] = 0; // ".zip" eraf

    int n = 0;
    for (int i = 0; i < max; i++) {
        zip_entry_t e;
        if (!find_entry_n(dir, zipname, exts, i, &e)) break;
        const char *dot = strrchr(e.name, '.');
        char cname[STORAGE_MAX_NAME];
        snprintf(cname, sizeof cname, "%s-%d%s", base, i + 1, dot ? dot : "");
        if (!extract_entry(dir, zipname, &e, window32k, cname)) return 0;
        snprintf(names[n], 128, "%s", cname);
        n++;
    }
    return n;
}

bool zip_extract_cached(const char *dir, const char *zipname,
                        const char *const *exts, uint8_t *window32k,
                        char *out_cache_name, size_t out_n)
{
    zip_entry_t e;
    if (!find_entry_n(dir, zipname, exts, 0, &e)) return false;

    char cname[STORAGE_MAX_NAME];
    cache_name_for(zipname, e.name, cname, sizeof cname);
    // (Een .dsk in de cache kan door de emulator beschreven zijn; dat is de
    // bedoeling — het origineel in de zip blijft onaangeroerd.)
    if (!extract_entry(dir, zipname, &e, window32k, cname)) return false;
    snprintf(out_cache_name, out_n, "%s", cname);
    return true;
}
