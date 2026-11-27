#include "img.h"
#include <stdlib.h>
#include <string.h>
#ifndef SONION_KERNEL
#include <stdio.h>
#endif

static uint8_t clamp8(int v) { return (v < 0) ? 0 : (v > 255 ? 255 : (uint8_t)v); }
static uint16_t rd16le(const uint8_t *p) { return (uint16_t)p[0] | ((uint16_t)p[1] << 8); }
static uint32_t rd32le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint32_t rd32be(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static int decode_bmp(const uint8_t *b, size_t len, Image *out) {
    if (len < 54 || b[0] != 'B' || b[1] != 'M') return 0;
    uint32_t off = rd32le(b + 10);
    int w = (int)rd32le(b + 18), hr = (int)rd32le(b + 22);
    int top = (hr < 0), h = top ? -hr : hr, bpp = rd16le(b + 28);
    if (w <= 0 || h <= 0 || (bpp != 24 && bpp != 32)) return 0;
    int step = bpp / 8;
    size_t stride = ((size_t)w * step + 3) & ~3ULL;
    if (off + stride * (size_t)h > len) return 0;
    uint32_t *px = (uint32_t *)malloc((size_t)w * h * 4);
    if (!px) return 0;
    for (int y = 0; y < h; y++) {
        const uint8_t *row = b + off + (size_t)(top ? y : (h - 1 - y)) * stride;
        for (int x = 0; x < w; x++) {
            const uint8_t *p = row + x * step;
            uint8_t a = (bpp == 32) ? p[3] : 0xFF;
            px[y * w + x] = ((uint32_t)a << 24) | ((uint32_t)p[2] << 16) | ((uint32_t)p[1] << 8) | p[0];
        }
    }
    out->width = w; out->height = h; out->pixels = px;
    return 1;
}

static int decode_tga(const uint8_t *b, size_t len, Image *out) {
    if (len < 18) return 0;
    uint8_t idl = b[0], cm = b[1], it = b[2];
    int w = rd16le(b + 12), h = rd16le(b + 14), bpp = b[16], desc = b[17];
    if (cm != 0 || (it != 2 && it != 10) || w <= 0 || h <= 0 || (bpp != 24 && bpp != 32)) return 0;
    size_t pos = 18 + idl;
    int step = bpp / 8, tot = w * h, idx = 0;
    uint32_t *px = (uint32_t *)malloc((size_t)tot * 4);
    if (!px) return 0;
    while (idx < tot && pos + step <= len) {
        int cnt = 1, rle = 0;
        if (it == 10) { uint8_t hd = b[pos++]; cnt = (hd & 0x7F) + 1; rle = (hd & 0x80) != 0; }
        for (int i = 0; i < cnt && idx < tot; i++) {
            if (pos + step > len) { free(px); return 0; }
            uint8_t a = (bpp == 32) ? b[pos + 3] : 0xFF;
            int y = idx / w, x = idx % w, dy = (desc & 0x20) ? y : (h - 1 - y);
            px[dy * w + x] = ((uint32_t)a << 24) | ((uint32_t)b[pos + 2] << 16) | ((uint32_t)b[pos + 1] << 8) | b[pos];
            idx++;
            if (!rle) pos += step;
        }
        if (rle) pos += step;
    }
    if (idx != tot) { free(px); return 0; }
    out->width = w; out->height = h; out->pixels = px;
    return 1;
}

typedef struct { const uint8_t *s; size_t len, pos; uint32_t bb; int bc; } BitStr;
static int bs_bits(BitStr *bs, int n) {
    while (bs->bc < n) {
        if (bs->pos >= bs->len) return -1;
        bs->bb |= (uint32_t)bs->s[bs->pos++] << bs->bc;
        bs->bc += 8;
    }
    int v = (int)(bs->bb & ((1U << n) - 1));
    bs->bb >>= n; bs->bc -= n;
    return v;
}

typedef struct { uint16_t cnt[16], sym[288]; } PngHuff;
static void ph_build(PngHuff *h, const uint8_t *lens, int n) {
    uint16_t offs[16];
    for (int i = 0; i < 16; i++) h->cnt[i] = 0;
    for (int i = 0; i < n; i++) if (lens[i]) h->cnt[lens[i]]++;
    offs[1] = 0;
    for (int i = 1; i < 15; i++) offs[i + 1] = offs[i] + h->cnt[i];
    for (int i = 0; i < n; i++) if (lens[i]) h->sym[offs[lens[i]]++] = (uint16_t)i;
}
static int ph_dec(BitStr *bs, const PngHuff *h) {
    int code = 0, first = 0, idx = 0;
    for (int l = 1; l <= 15; l++) {
        int bit = bs_bits(bs, 1); if (bit < 0) return -1;
        code |= bit;
        int c = h->cnt[l];
        if (code - c < first) return h->sym[idx + (code - first)];
        idx += c; first = (first + c) << 1; code <<= 1;
    }
    return -1;
}

static int png_inflate(const uint8_t *z, size_t zlen, uint8_t *out, size_t out_cap) {
    static const uint16_t lbase[29] = {3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
    static const uint8_t  lext[29]  = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
    static const uint16_t dbase[30] = {1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
    static const uint8_t  dext[30]  = {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};
    static const uint8_t  clord[19] = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
    if (zlen < 6) return 0;
    BitStr bs = { z + 2, zlen - 2, 0, 0, 0 };
    size_t op = 0; int bfinal = 0;
    while (!bfinal) {
        bfinal = bs_bits(&bs, 1);
        int btype = bs_bits(&bs, 2);
        if (bfinal < 0 || btype < 0 || btype == 3) return 0;
        if (btype == 0) {
            bs.bb = 0; bs.bc = 0;
            if (bs.pos + 4 > bs.len) return 0;
            uint16_t len = rd16le(bs.s + bs.pos); bs.pos += 4;
            if (bs.pos + len > bs.len || op + len > out_cap) return 0;
            memcpy(out + op, bs.s + bs.pos, len); op += len; bs.pos += len;
        } else {
            PngHuff hl, hd;
            if (btype == 1) {
                uint8_t ll[288], dl[32];
                for (int i = 0; i < 144; i++) ll[i] = 8;
                for (int i = 144; i < 256; i++) ll[i] = 9;
                for (int i = 256; i < 280; i++) ll[i] = 7;
                for (int i = 280; i < 288; i++) ll[i] = 8;
                for (int i = 0; i < 32; i++) dl[i] = 5;
                ph_build(&hl, ll, 288); ph_build(&hd, dl, 32);
            } else {
                int hlit = bs_bits(&bs, 5) + 257, hdist = bs_bits(&bs, 5) + 1, hclen = bs_bits(&bs, 4) + 4;
                uint8_t cl[19] = {0}, lens[320] = {0};
                for (int i = 0; i < hclen; i++) cl[clord[i]] = (uint8_t)bs_bits(&bs, 3);
                PngHuff hc; ph_build(&hc, cl, 19);
                int idx = 0, tot = hlit + hdist;
                while (idx < tot) {
                    int s = ph_dec(&bs, &hc); if (s < 0) return 0;
                    if (s < 16) lens[idx++] = (uint8_t)s;
                    else if (s == 16) { int r = bs_bits(&bs, 2) + 3; uint8_t p = idx ? lens[idx - 1] : 0; while (r-- && idx < tot) lens[idx++] = p; }
                    else if (s == 17) { int r = bs_bits(&bs, 3) + 3; while (r-- && idx < tot) lens[idx++] = 0; }
                    else if (s == 18) { int r = bs_bits(&bs, 7) + 11; while (r-- && idx < tot) lens[idx++] = 0; }
                }
                ph_build(&hl, lens, hlit); ph_build(&hd, lens + hlit, hdist);
            }
            while (1) {
                int sym = ph_dec(&bs, &hl); if (sym < 0) return 0;
                if (sym < 256) { if (op >= out_cap) return 0; out[op++] = (uint8_t)sym; }
                else if (sym == 256) break;
                else {
                    int li = sym - 257; if (li >= 29) return 0;
                    int len = lbase[li] + bs_bits(&bs, lext[li]);
                    int di = ph_dec(&bs, &hd); if (di < 0 || di >= 30) return 0;
                    int dist = dbase[di] + bs_bits(&bs, dext[di]);
                    if ((size_t)dist > op || op + (size_t)len > out_cap) return 0;
                    for (int k = 0; k < len; k++) { out[op] = out[op - dist]; op++; }
                }
            }
        }
    }
    return op == out_cap;
}

static uint8_t paeth(int a, int b, int c) {
    int p = a + b - c;
    int pa = p > a ? p - a : a - p, pb = p > b ? p - b : b - p, pc = p > c ? p - c : c - p;
    return (uint8_t)((pa <= pb && pa <= pc) ? a : (pb <= pc ? b : c));
}

static int decode_png(const uint8_t *b, size_t len, Image *out) {
    static const uint8_t sig[8] = {0x89,0x50,0x4E,0x47,0x0D,0x0A,0x1A,0x0A};
    if (len < 33 || memcmp(b, sig, 8) != 0) return 0;
    int w = 0, h = 0, bd = 0, ct = 0;
    uint8_t pal[256 * 3] = {0}, trns[256];
    for (int i = 0; i < 256; i++) trns[i] = 0xFF;
    uint8_t *zbuf = (uint8_t *)malloc(len);
    size_t zlen = 0, pos = 8;
    if (!zbuf) return 0;
    while (pos + 12 <= len) {
        uint32_t clen = rd32be(b + pos), ctype = rd32be(b + pos + 4);
        pos += 8;
        if (pos + clen + 4 > len) { free(zbuf); return 0; }
        if (ctype == 0x49484452 && clen >= 13) {
            w = (int)rd32be(b + pos); h = (int)rd32be(b + pos + 4);
            bd = b[pos + 8]; ct = b[pos + 9];
        } else if (ctype == 0x504C5445) {
            size_t n = clen < sizeof(pal) ? clen : sizeof(pal);
            memcpy(pal, b + pos, n);
        } else if (ctype == 0x74524E53) {
            size_t n = clen < 256 ? clen : 256;
            memcpy(trns, b + pos, n);
        } else if (ctype == 0x49444154) {
            memcpy(zbuf + zlen, b + pos, clen); zlen += clen;
        } else if (ctype == 0x49454E44) { break; }
        pos += clen + 4;
    }
    int ch = (ct == 0 || ct == 3) ? 1 : (ct == 4 ? 2 : (ct == 2 ? 3 : (ct == 6 ? 4 : 0)));
    if (w <= 0 || h <= 0 || bd != 8 || ch == 0) { free(zbuf); return 0; }
    size_t stride = (size_t)w * ch, raw_sz = (size_t)h * (stride + 1);
    uint8_t *raw = (uint8_t *)malloc(raw_sz);
    if (!raw || !png_inflate(zbuf, zlen, raw, raw_sz)) { free(zbuf); free(raw); return 0; }
    free(zbuf);
    uint32_t *px = (uint32_t *)malloc((size_t)w * h * 4);
    if (!px) { free(raw); return 0; }
    for (int y = 0; y < h; y++) {
        uint8_t f = raw[(size_t)y * (stride + 1)];
        uint8_t *cur = raw + (size_t)y * (stride + 1) + 1;
        uint8_t *prv = (y > 0) ? (raw + (size_t)(y - 1) * (stride + 1) + 1) : 0;
        for (size_t i = 0; i < stride; i++) {
            int a = (i >= (size_t)ch) ? cur[i - ch] : 0;
            int up = prv ? prv[i] : 0;
            int ul = (prv && i >= (size_t)ch) ? prv[i - ch] : 0;
            if (f == 1) cur[i] = (uint8_t)(cur[i] + a);
            else if (f == 2) cur[i] = (uint8_t)(cur[i] + up);
            else if (f == 3) cur[i] = (uint8_t)(cur[i] + ((a + up) >> 1));
            else if (f == 4) cur[i] = (uint8_t)(cur[i] + paeth(a, up, ul));
        }
        for (int x = 0; x < w; x++) {
            const uint8_t *p = cur + x * ch;
            uint8_t r = 0, g = 0, bl = 0, al = 0xFF;
            if (ct == 0) { r = g = bl = p[0]; }
            else if (ct == 2) { r = p[0]; g = p[1]; bl = p[2]; }
            else if (ct == 3) { uint8_t idx = p[0]; r = pal[idx*3]; g = pal[idx*3+1]; bl = pal[idx*3+2]; al = trns[idx]; }
            else if (ct == 4) { r = g = bl = p[0]; al = p[1]; }
            else if (ct == 6) { r = p[0]; g = p[1]; bl = p[2]; al = p[3]; }
            px[y * w + x] = ((uint32_t)al << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | bl;
        }
    }
    free(raw);
    out->width = w; out->height = h; out->pixels = px;
    return 1;
}

static const uint8_t j_zz[64] = {
     0,  1,  8, 16,  9,  2,  3, 10, 17, 24, 32, 25, 18, 11,  4,  5,
    12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13,  6,  7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63
};

typedef struct { uint16_t mn[17], mx[17]; int vp[17]; uint8_t v[256]; } JHuff;
typedef struct {
    const uint8_t *d; size_t sz, pos; uint32_t bb; int bc, eof;
    int w, h, nc, mh, mv, ri;
    struct { uint8_t id, hs, vs, tq, td, ta; int dc; } c[4];
    uint16_t qt[4][64];
    JHuff ht[2][4];
} JDec;

static int j_bit(JDec *j) {
    if (j->bc == 0) {
        if (j->pos >= j->sz) { j->eof = 1; return 0; }
        uint8_t b = j->d[j->pos++];
        if (b == 0xFF && (j->pos >= j->sz || j->d[j->pos++] != 0x00)) { j->eof = 1; return 0; }
        j->bb = b; j->bc = 8;
    }
    return (j->bb >> (--j->bc)) & 1;
}
static int j_bits(JDec *j, int n) { int v = 0; while (n--) v = (v << 1) | j_bit(j); return v; }
static int j_hdec(JDec *j, JHuff *h) {
    int code = 0;
    for (int l = 1; l <= 16; l++) {
        code = (code << 1) | j_bit(j);
        if (j->eof) return 0;
        if (h->mx[l] != 0xFFFF && code <= h->mx[l])
            return h->v[h->vp[l] + (code - h->mn[l])];
    }
    return 0;
}
static int j_ext(int v, int n) { return (n && v < (1 << (n - 1))) ? (v + (-1 << n) + 1) : v; }

static void j_idct(int *blk, uint8_t *out) {
    static const int cs[8] = { 1024, 1004, 946, 851, 724, 569, 392, 200 };
    int tmp[64];
    for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++) {
        int s = 0;
        for (int u = 0; u < 8; u++) {
            int a = u ? 1024 : 724, ang = ((2 * x + 1) * u) & 31, sg = 1;
            if (ang >= 16) { ang -= 16; sg = -1; }
            if (ang > 8) { ang = 16 - ang; sg = -sg; }
            s += ((a * (ang == 8 ? 0 : sg * cs[ang])) >> 10) * blk[y * 8 + u];
        }
        tmp[y * 8 + x] = s >> 11;
    }
    for (int x = 0; x < 8; x++) for (int y = 0; y < 8; y++) {
        int s = 0;
        for (int v = 0; v < 8; v++) {
            int a = v ? 1024 : 724, ang = ((2 * y + 1) * v) & 31, sg = 1;
            if (ang >= 16) { ang -= 16; sg = -1; }
            if (ang > 8) { ang = 16 - ang; sg = -sg; }
            s += ((a * (ang == 8 ? 0 : sg * cs[ang])) >> 10) * tmp[v * 8 + x];
        }
        out[y * 8 + x] = clamp8((s >> 12) + 128);
    }
}

static int decode_jpeg(const uint8_t *buf, size_t len, Image *out) {
    if (len < 4 || buf[0] != 0xFF || buf[1] != 0xD8) return 0;
    JDec *j = (JDec *)calloc(1, sizeof(JDec));
    if (!j) return 0;
    j->d = buf; j->sz = len; j->pos = 2;
    while (j->pos + 4 <= j->sz) {
        while (j->pos < j->sz && j->d[j->pos] != 0xFF) j->pos++;
        while (j->pos < j->sz && j->d[j->pos] == 0xFF) j->pos++;
        if (j->pos >= j->sz) break;
        uint8_t mk = j->d[j->pos++];
        if (mk == 0xD9) break;
        if (mk == 0x01 || (mk >= 0xD0 && mk <= 0xD7)) continue;
        uint16_t ml = ((uint16_t)j->d[j->pos] << 8) | j->d[j->pos + 1];
        size_t nxt = j->pos + ml; j->pos += 2;
        if (nxt > j->sz) break;
        if (mk == 0xDB) {
            while (j->pos < nxt) {
                uint8_t pt = j->d[j->pos++]; int pq = pt >> 4, tq = pt & 3;
                for (int i = 0; i < 64; i++) {
                    j->qt[tq][i] = pq ? (((uint16_t)j->d[j->pos] << 8) | j->d[j->pos + 1]) : j->d[j->pos];
                    j->pos += pq ? 2 : 1;
                }
            }
        } else if (mk == 0xC0) {
            j->pos++;
            j->h = ((int)j->d[j->pos] << 8) | j->d[j->pos + 1];
            j->w = ((int)j->d[j->pos + 2] << 8) | j->d[j->pos + 3];
            j->nc = j->d[j->pos + 4]; j->pos += 5;
            for (int c = 0; c < j->nc && c < 4; c++) {
                j->c[c].id = j->d[j->pos]; j->c[c].hs = j->d[j->pos + 1] >> 4;
                j->c[c].vs = j->d[j->pos + 1] & 0xF; j->c[c].tq = j->d[j->pos + 2] & 3;
                if (j->c[c].hs > j->mh) j->mh = j->c[c].hs;
                if (j->c[c].vs > j->mv) j->mv = j->c[c].vs;
                j->pos += 3;
            }
        } else if (mk == 0xC4) {
            while (j->pos < nxt) {
                uint8_t tt = j->d[j->pos++]; JHuff *h = &j->ht[(tt >> 4) & 1][tt & 3];
                uint8_t bt[17]; int tot = 0;
                for (int l = 1; l <= 16; l++) { bt[l] = j->d[j->pos++]; tot += bt[l]; }
                for (int i = 0; i < tot && i < 256; i++) h->v[i] = j->d[j->pos++];
                uint16_t cd = 0; int id = 0;
                for (int l = 1; l <= 16; l++) {
                    if (!bt[l]) h->mx[l] = 0xFFFF;
                    else { h->vp[l] = id; h->mn[l] = cd; cd += bt[l] - 1; h->mx[l] = cd++; id += bt[l]; }
                    cd <<= 1;
                }
            }
        } else if (mk == 0xDD) { j->ri = ((int)j->d[j->pos] << 8) | j->d[j->pos + 1]; }
        else if (mk == 0xDA) {
            int ns = j->d[j->pos++];
            for (int s = 0; s < ns; s++) {
                uint8_t cid = j->d[j->pos], dt = j->d[j->pos + 1];
                for (int c = 0; c < j->nc; c++) if (j->c[c].id == cid) { j->c[c].td = (dt >> 4) & 3; j->c[c].ta = dt & 3; }
                j->pos += 2;
            }
            j->pos += 3; break;
        }
        j->pos = nxt;
    }
    if (j->w <= 0 || j->h <= 0 || !j->mh || !j->mv) { free(j); return 0; }
    int mw = j->mh * 8, mh = j->mv * 8, mx_n = (j->w + mw - 1) / mw, my_n = (j->h + mh - 1) / mh;
    int pw = mx_n * mw, ph = my_n * mh;
    uint8_t *pl[3];
    for (int c = 0; c < 3; c++) pl[c] = (uint8_t *)calloc(1, (size_t)pw * ph);
    int mc = 0;
    for (int my = 0; my < my_n; my++) for (int mx = 0; mx < mx_n; mx++) {
        if (j->ri > 0 && mc > 0 && (mc % j->ri) == 0) {
            j->bc = 0; if (j->pos + 2 <= j->sz && j->d[j->pos] == 0xFF) j->pos += 2;
            for (int c = 0; c < 4; c++) j->c[c].dc = 0;
        }
        for (int c = 0; c < j->nc && c < 3; c++) {
            int hs = j->c[c].hs, vs = j->c[c].vs, sx = j->mh / hs, sy = j->mv / vs;
            for (int v = 0; v < vs; v++) for (int h = 0; h < hs; h++) {
                int blk[64] = {0}, s = j_hdec(j, &j->ht[0][j->c[c].td]);
                j->c[c].dc += j_ext(j_bits(j, s), s);
                blk[0] = j->c[c].dc * j->qt[j->c[c].tq][0];
                for (int k = 1; k < 64; k++) {
                    int rs = j_hdec(j, &j->ht[1][j->c[c].ta]); if (!rs) break;
                    k += (rs >> 4); int sz = rs & 0xF, ac = j_ext(j_bits(j, sz), sz);
                    if (k < 64) blk[j_zz[k]] = ac * j->qt[j->c[c].tq][k];
                }
                uint8_t pix[64]; j_idct(blk, pix);
                for (int py = 0; py < 8; py++) for (int px = 0; px < 8; px++) {
                    uint8_t val = pix[py * 8 + px];
                    for (int dy = 0; dy < sy; dy++) for (int dx = 0; dx < sx; dx++) {
                        int ox = mx * mw + (h * 8 + px) * sx + dx, oy = my * mh + (v * 8 + py) * sy + dy;
                        if (ox < pw && oy < ph) pl[c][oy * pw + ox] = val;
                    }
                }
            }
        }
        mc++;
    }
    uint32_t *px = (uint32_t *)malloc((size_t)j->w * j->h * 4);
    if (px) {
        for (int y = 0; y < j->h; y++) for (int x = 0; x < j->w; x++) {
            int idx = y * pw + x, Y = pl[0][idx];
            int Cb = (j->nc >= 3) ? ((int)pl[1][idx] - 128) : 0;
            int Cr = (j->nc >= 3) ? ((int)pl[2][idx] - 128) : 0;
            uint8_t r = clamp8(Y + ((1436 * Cr) >> 10));
            uint8_t g = clamp8(Y - ((352 * Cb + 731 * Cr) >> 10));
            uint8_t b = clamp8(Y + ((1815 * Cb) >> 10));
            px[y * j->w + x] = 0xFF000000U | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
        }
        out->width = j->w; out->height = j->h; out->pixels = px;
    }
    for (int c = 0; c < 3; c++) free(pl[c]);
    int ok = (px != 0); free(j); return ok;
}

int img_decode(const uint8_t *buf, size_t len, Image *out) {
    if (!buf || !out || len < 4) return 0;
    out->width = 0; out->height = 0; out->pixels = 0;
    if (buf[0] == 'B' && buf[1] == 'M') return decode_bmp(buf, len, out);
    if (buf[0] == 0x89 && buf[1] == 'P' && buf[2] == 'N' && buf[3] == 'G') return decode_png(buf, len, out);
    if (buf[0] == 0xFF && buf[1] == 0xD8) return decode_jpeg(buf, len, out);
    return decode_tga(buf, len, out);
}

#ifndef SONION_KERNEL
int img_load(const char *path, Image *out) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return 0; }
    uint8_t *buf = (uint8_t *)malloc((size_t)sz);
    if (!buf) { fclose(f); return 0; }
    size_t rd = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    int ok = img_decode(buf, rd, out);
    free(buf);
    return ok;
}
#endif

void img_blit(uint32_t *dst, int dst_w, int dst_h,
              const Image *src, int dx, int dy, int dw, int dh) {
    if (!dst || !src || !src->pixels || dw <= 0 || dh <= 0) return;
    for (int y = 0; y < dh; y++) {
        int ty = dy + y; if (ty < 0 || ty >= dst_h) continue;
        int sy = (y * src->height) / dh;

        for (int x = 0; x < dw; x++) {
            int tx = dx + x; if (tx < 0 || tx >= dst_w) continue;
            int sx = (x * src->width) / dw;
            uint32_t sp = src->pixels[sy * src->width + sx], a = sp >> 24;
            if (a == 0) continue;
            if (a == 255) { dst[ty * dst_w + tx] = sp & 0x00FFFFFFU; continue; }
            uint32_t dp = dst[ty * dst_w + tx], ia = 255 - a;
            uint32_t r = (((sp >> 16) & 0xFF) * a + ((dp >> 16) & 0xFF) * ia) >> 8;
            uint32_t g = (((sp >> 8)  & 0xFF) * a + ((dp >> 8)  & 0xFF) * ia) >> 8;
            uint32_t b = (( sp        & 0xFF) * a + ( dp        & 0xFF) * ia) >> 8;
            dst[ty * dst_w + tx] = (r << 16) | (g << 8) | b;
        }
    }
}
void img_free(Image *img) {
    if (img && img->pixels) { free(img->pixels); img->pixels = 0; }
}
