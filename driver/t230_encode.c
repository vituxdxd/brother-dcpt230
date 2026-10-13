/*
 * t230_encode — print stream generator for the Brother DCP-T230 family.
 *
 * Reads PPM (P6) images from stdin — one or more concatenated pages, as
 * Ghostscript's ppmraw device produces — and writes the printer's native
 * stream: a PJL preamble followed by the PWG-raster variant the printer
 * accepts. See docs/PROTOCOL.md for the full wire format.
 *
 * Page 1 layout:
 *   [PJL text from the wrapper] + "RaS2"
 *   for each page: 1796-byte page header ("PwgRaster"...), then line blocks
 *   line block: [rc][RLE of one scanline]
 *     rc = number of extra identical lines (0 or 1)
 *     RLE: 0x00        -> one literal pixel
 *          0x01..0x7F  -> repeat the next pixel (value + 1) times
 *          0x81..0xFF  -> (0x101 - value) literal pixels (2..128)
 *   trailer: ESC%-12345X (+ "@PJL EOJ NAME=...") + ESC%-12345X
 *
 * Usage (normally called by the brother-t230 wrapper):
 *   t230_encode --width N --height N [--pt WxH] [--dpi N] [--mono]
 *               [--pjl FILE] [--job-start] [--job-end] [--eoj NAME]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

#define MAX_LINE_BYTES (1 << 16)

static const char *progname = "t230_encode";

static void die(const char *msg)
{
    fprintf(stderr, "%s: %s\n", progname, msg);
    exit(1);
}

/* ---------- big-endian helpers ---------- */
static void wbe32(unsigned char *p, uint32_t v)
{
    p[0] = (unsigned char)(v >> 24);
    p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);
    p[3] = (unsigned char)(v);
}

static int write_all(const void *buf, size_t len)
{
    const unsigned char *p = buf;
    while (len) {
        size_t n = fwrite(p, 1, len, stdout);
        if (n == 0)
            return 0;
        p += n;
        len -= n;
    }
    return 1;
}

/* ---------- PPM (P6) reader ---------- */
struct ppm {
    int width, height, maxval;
    unsigned char *row;     /* current scanline (RGB) */
};

/* read one integer token, skipping whitespace and '#' comments */
static int ppm_read_int(FILE *f, long *out)
{
    int c;
    for (;;) {
        c = fgetc(f);
        if (c == '#') {
            while (c != '\n' && c != EOF)
                c = fgetc(f);
        } else if (!isspace(c)) {
            break;
        }
        if (c == EOF)
            return 0;
    }
    if (!isdigit(c))
        return 0;
    long v = 0;
    while (isdigit(c)) {
        v = v * 10 + (c - '0');
        c = fgetc(f);
    }
    if (c != EOF)
        ungetc(c, f);
    *out = v;
    return 1;
}

/* read a P6 header; 0 = clean EOF, 1 = ok, -1 = malformed */
static int ppm_read_header(FILE *f, struct ppm *p)
{
    int c = fgetc(f);
    if (c == EOF)
        return 0;
    if (c != 'P' || fgetc(f) != '6')
        return -1;
    long w, h, m;
    if (!ppm_read_int(f, &w) || !ppm_read_int(f, &h) || !ppm_read_int(f, &m))
        return -1;
    if (w <= 0 || h <= 0 || m <= 0 || m > 255)
        return -1;
    c = fgetc(f);
    if (!isspace(c))
        return -1;
    p->width = (int)w;
    p->height = (int)h;
    p->maxval = (int)m;
    p->row = realloc(p->row, (size_t)p->width * 3);
    if (!p->row)
        die("out of memory");
    return 1;
}

/* ---------- RLE ---------- */
struct obuf {
    unsigned char *d;
    size_t len, cap;
};

static void obuf_reserve(struct obuf *b, size_t extra)
{
    if (b->len + extra <= b->cap)
        return;
    size_t cap = b->cap ? b->cap : 65536;
    while (cap < b->len + extra)
        cap *= 2;
    b->d = realloc(b->d, cap);
    if (!b->d)
        die("out of memory");
    b->cap = cap;
}

static void obuf_put(struct obuf *b, const void *src, size_t n)
{
    obuf_reserve(b, n);
    memcpy(b->d + b->len, src, n);
    b->len += n;
}

static void obuf_byte(struct obuf *b, unsigned char v)
{
    obuf_reserve(b, 1);
    b->d[b->len++] = v;
}

/*
 * Encode one scanline (width pixels of `unit` bytes each) with the printer's
 * run-length scheme: a run of equal pixels uses a repeat code, anything else
 * is emitted as a literal run. Runs are capped at 128 pixels.
 */
static void rle_encode_line(struct obuf *out, const unsigned char *px,
                            int width, int unit)
{
    int i = 0;
    while (i < width) {
        const unsigned char *cur = px + (size_t)i * unit;
        if (i + 1 < width && memcmp(cur, cur + unit, unit) == 0) {
            int n = 2;
            while (i + n < width && n < 128 &&
                   memcmp(cur, cur + (size_t)n * unit, unit) == 0)
                n++;
            obuf_byte(out, (unsigned char)(n - 1));
            obuf_put(out, cur, unit);
            i += n;
        } else {
            int n = 1;
            while (i + n < width && n < 128 &&
                   !(i + n + 1 < width &&
                     memcmp(px + (size_t)(i + n) * unit,
                            px + (size_t)(i + n + 1) * unit, unit) == 0))
                n++;
            if (n == 1)
                obuf_byte(out, 0x00);
            else
                obuf_byte(out, (unsigned char)(0x101 - n));
            obuf_put(out, cur, (size_t)n * unit);
            i += n;
        }
    }
}

/* ---------- 1796-byte page header ---------- */
static void build_page_header(unsigned char *h, int pt_w, int pt_h,
                              int px_w, int px_h, int dpi, int mono)
{
    memset(h, 0, 1796);
    memcpy(h + 0x000, "PwgRaster", 9);
    memcpy(h + 0x080, "auto", 4);
    wbe32(h + 0x114, (uint32_t)dpi);
    wbe32(h + 0x118, (uint32_t)dpi);
    wbe32(h + 0x160, (uint32_t)pt_w);
    wbe32(h + 0x164, (uint32_t)pt_h);
    wbe32(h + 0x174, (uint32_t)px_w);
    wbe32(h + 0x178, (uint32_t)px_h);
    wbe32(h + 0x180, 8);
    wbe32(h + 0x184, mono ? 8 : 24);
    wbe32(h + 0x188, (uint32_t)(mono ? px_w : px_w * 3));
    wbe32(h + 0x190, mono ? 18 : 19);
    wbe32(h + 0x1a4, mono ? 1 : 3);
}

/* ---------- main ---------- */
int main(int argc, char **argv)
{
    int width = 0, height = 0, pt_w = 0, pt_h = 0, dpi = 300;
    int mono = 0, job_start = 0, job_end = 0;
    const char *pjl_file = NULL, *eoj = NULL;

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (!strcmp(a, "--width") && i + 1 < argc)
            width = atoi(argv[++i]);
        else if (!strcmp(a, "--height") && i + 1 < argc)
            height = atoi(argv[++i]);
        else if (!strcmp(a, "--pt") && i + 1 < argc)
            sscanf(argv[++i], "%dx%d", &pt_w, &pt_h);
        else if (!strcmp(a, "--dpi") && i + 1 < argc)
            dpi = atoi(argv[++i]);
        else if (!strcmp(a, "--mono"))
            mono = 1;
        else if (!strcmp(a, "--pjl") && i + 1 < argc)
            pjl_file = argv[++i];
        else if (!strcmp(a, "--job-start"))
            job_start = 1;
        else if (!strcmp(a, "--job-end"))
            job_end = 1;
        else if (!strcmp(a, "--eoj") && i + 1 < argc)
            eoj = argv[++i];
        else
            die("unknown argument");
    }
    if (width <= 0 || height <= 0)
        die("--width and --height are required");

    const int unit = mono ? 1 : 3;
    if ((size_t)width * unit > MAX_LINE_BYTES)
        die("scanline too long");

    /* PJL preamble + magic, once per job */
    if (job_start) {
        if (pjl_file) {
            FILE *f = fopen(pjl_file, "rb");
            if (!f)
                die("cannot open PJL file");
            unsigned char buf[8192];
            size_t n;
            while ((n = fread(buf, 1, sizeof buf, f)) > 0)
                if (!write_all(buf, n))
                    die("write error");
            fclose(f);
        }
        if (!write_all("RaS2", 4))
            die("write error");
    }

    unsigned char header[1796];
    build_page_header(header, pt_w, pt_h, width, height, dpi, mono);

    struct ppm p = {0};
    struct obuf rle = {0};
    unsigned char *prev = malloc((size_t)width * unit);
    unsigned char *mono_row = mono ? malloc((size_t)width) : NULL;
    if (!prev || (mono && !mono_row))
        die("out of memory");

    int have_prev = 0, rc = 0;
    int pages = 0;
    int st;

    while ((st = ppm_read_header(stdin, &p)) == 1) {
        if (p.width != width || p.height != height)
            die("PPM page size does not match the selected paper size");

        /* page header (the last four bytes are already zero) */
        if (!write_all(header, sizeof header))
            die("write error");
        pages++;
        have_prev = 0;
        rc = 0;

        for (int y = 0; y < height; y++) {
            size_t got = fread(p.row, 3, (size_t)width, stdin);
            if (got != (size_t)width)
                die("truncated PPM data");

            unsigned char *line;
            if (mono) {
                for (int x = 0; x < width; x++) {
                    int r = p.row[3 * x];
                    int g = p.row[3 * x + 1];
                    int b = p.row[3 * x + 2];
                    int yv = (int)((double)r * 0.3 + (double)g * 0.59 +
                                   (double)b * 0.11);
                    mono_row[x] = (unsigned char)yv;
                }
                line = mono_row;
            } else {
                line = p.row;
            }

            if (have_prev && rc == 0 &&
                memcmp(prev, line, (size_t)width * unit) == 0) {
                rc = 1;             /* block now covers two identical lines */
                continue;
            }
            if (have_prev) {
                rle.len = 0;
                rle_encode_line(&rle, prev, width, unit);
                unsigned char rcbyte = (unsigned char)rc;
                if (!write_all(&rcbyte, 1) || !write_all(rle.d, rle.len))
                    die("write error");
            }
            memcpy(prev, line, (size_t)width * unit);
            have_prev = 1;
            rc = 0;
        }
        /* flush the pending block at end of page */
        if (have_prev) {
            rle.len = 0;
            rle_encode_line(&rle, prev, width, unit);
            unsigned char rcbyte = (unsigned char)rc;
            if (!write_all(&rcbyte, 1) || !write_all(rle.d, rle.len))
                die("write error");
            have_prev = 0;
        }
    }
    if (st < 0)
        die("input is not a P6 PPM stream");
    if (pages == 0)
        die("no pages found");

    if (job_end) {
        if (!write_all("\033%-12345X", 9))
            die("write error");
        if (eoj && *eoj) {
            char buf[1024];
            int n = snprintf(buf, sizeof buf, "@PJL EOJ NAME=\"%s\"\r\n", eoj);
            if (!write_all(buf, (size_t)n))
                die("write error");
        }
        if (!write_all("\033%-12345X", 9))
            die("write error");
    }

    fflush(stdout);
    return 0;
}
