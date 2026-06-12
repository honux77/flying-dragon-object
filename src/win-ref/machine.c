// wbml (Sega System 2) machine implementation.
#include "machine.h"
#include "mc8123.h"
#include "video.h"
#include <stdio.h>
#include <string.h>

// Main CPU runs at MASTER_CLOCK/5 = 4MHz; refresh ~60.06Hz.
#define MAIN_CLOCK   4000000
#define FPS          60.06
#define CYC_PER_FRAME ((unsigned)(MAIN_CLOCK / FPS))

#define RUN_SOUND_CPU 1
#define SOUND_XTAL    8000000              // SOUND_CLOCK
#define SOUND_CPU_HZ  (SOUND_XTAL / 2)     // sound Z80 = 4MHz
#define SCYC_PER_FRAME ((unsigned)(SOUND_CPU_HZ / FPS))

// ---- main CPU memory space ------------------------------------------------

static unsigned bankoff(system2 *m, uint16_t addr) {
    return 0x10000 + m->bank1 * 0x4000 + (addr - 0x8000);
}

static uint8_t main_fetch(void *ud, uint16_t addr) {
    system2 *m = (system2 *)ud;
    if (addr < 0x8000) return m->opcodes[addr];
    if (addr < 0xc000) return m->opcodes[bankoff(m, addr)];
    // opcode fetch from RAM/IO region: see data view
    return m->maincpu.read_byte(ud, addr);
}

static uint8_t videoram_read(system2 *m, uint16_t addr) {
    unsigned off = (addr & 0xfff) | (0x1000 * ((m->videoram_bank >> 1) & 3));
    return m->videoram[off & (VIDEORAM_SIZE - 1)];
}

static void videoram_write(system2 *m, uint16_t addr, uint8_t val) {
    unsigned off = (addr & 0xfff) | (0x1000 * ((m->videoram_bank >> 1) & 3));
    m->videoram[off & (VIDEORAM_SIZE - 1)] = val;
}

static uint8_t main_read(void *ud, uint16_t addr) {
    system2 *m = (system2 *)ud;
    if (addr < 0x8000) return m->data[addr];
    if (addr < 0xc000) return m->data[bankoff(m, addr)];
    if (addr < 0xd000) return m->ram[addr - 0xc000];
    if (addr < 0xd800) return m->spriteram[addr - 0xd000];
    if (addr < 0xe000) return m->paletteram[addr - 0xd800];
    if (addr < 0xf000) return videoram_read(m, addr);
    if (addr < 0xf400)  // mixer collision r
        return m->mix_collide[addr & 0x3f] | 0x7e | (m->mix_collide_summary << 7);
    if (addr < 0xf800) return 0xff;  // mixer collision reset (write-only)
    if (addr < 0xfc00)  // sprite collision r
        return m->sprite_collide[addr & 0x3ff] | 0x7e | (m->sprite_collide_summary << 7);
    return 0xff;  // sprite collision reset (write-only)
}

static void main_write(void *ud, uint16_t addr, uint8_t val) {
    system2 *m = (system2 *)ud;
    if (addr < 0xc000) return;  // ROM
    if (addr < 0xd000) { m->ram[addr - 0xc000] = val; return; }
    if (addr < 0xd800) { m->spriteram[addr - 0xd000] = val; return; }
    if (addr < 0xe000) { m->paletteram[addr - 0xd800] = val; return; }
    if (addr < 0xf000) { videoram_write(m, addr, val); return; }
    if (addr < 0xf400) { m->mix_collide[addr & 0x3f] = 0; return; }
    if (addr < 0xf800) { m->mix_collide_summary = 0; return; }
    if (addr < 0xfc00) { m->sprite_collide[addr & 0x3ff] = 0; return; }
    m->sprite_collide_summary = 0;
}

// ---- main CPU I/O space (8255 PPI), global_mask 0x1f ----------------------

int g_io_log = 0;  // set by diagnostic harness to trace PPI port B/C writes
int g_sn_solo = 0; // 0 = both chips, 1 = SN1 only, 2 = SN2 only (diagnostics)
int g_sn_log = 0;  // log SN register writes
FILE *g_sn_logfile = NULL;  // destination for SN write log (stderr or a file)
unsigned long g_sn_writes = 0, g_latch_reads = 0, g_snd_nmi = 0, g_snd_irq = 0, g_snd_irq_taken = 0, g_snd_loop = 0, g_snd_driver = 0;
unsigned long g_frame = 0;

static void videomode_w(system2 *m, uint8_t data) {
    if (g_io_log && ((m->video_mode ^ data) & 0x10))
        fprintf(stderr, "[vmode] PC=%04X %02X->%02X blank=%d\n",
                m->maincpu.pc, m->video_mode, data, (data >> 4) & 1);
    // banking (bank0c): bank bits are data bits 3,2
    m->bank1 = (data & 0x0c) >> 2;
    // common video mode
    m->video_mode = data;
    m->flip = (data & 0x80) ? 1 : 0;
}

static void sound_control_w(system2 *m, uint8_t data) {
    // PPI port C: low bits drive video RAM banking. In mode 2, bits 6/7 are
    // handshake lines for port A rather than normal output bits; bit 7 is the
    // active-low OBF line wired to the sound CPU NMI.
    int prev_clear = m->sound_nmi_mask;
    m->ppi_portc = data;
    m->videoram_bank = data;
    m->sound_nmi_mask = (data & 0x80) ? 1 : 0;
    if (prev_clear && !m->sound_nmi_mask) {
        g_snd_nmi++;
        z80_gen_nmi(&m->soundcpu);
    }
}

static uint8_t main_port_in(z80 *z, uint8_t port) {
    system2 *m = (system2 *)z->userdata;
    switch (port & 0x1f) {
        case 0x00: return (uint8_t)~m->in.p1;
        case 0x04: return (uint8_t)~m->in.p2;
        case 0x08: return (uint8_t)~m->in.system;
        case 0x0c: case 0x0e: return m->in.swa;
        case 0x0d: case 0x0f: return m->in.swb;
        case 0x10: case 0x11: case 0x12: case 0x13: return m->in.swb;
        // 8255 ports A/B/C are configured as outputs; reading returns the
        // output latch (read-modify-write code relies on this).
        case 0x14: return m->soundlatch;
        case 0x15: return (uint8_t)m->video_mode;
        case 0x16: return m->ppi_portc;
        default: return 0xff;
    }
}

static void main_port_out(z80 *z, uint8_t port, uint8_t val) {
    system2 *m = (system2 *)z->userdata;
    if (g_io_log && ((port & 0x1f) == 0x14 || (port & 0x1f) == 0x16 || (port & 0x1f) == 0x17))
        fprintf(stderr, "[io] PC=%04X OUT(%02X)=%02X portc=%02X\n",
                m->maincpu.pc, port & 0x1f, val, m->ppi_portc);
    switch (port & 0x1f) {
        case 0x14:
            m->soundlatch = val;
            if (g_sn_log && g_sn_logfile)
                fprintf(g_sn_logfile, "[cmd] f=%lu mcyc=%lu pc=%04X val=%02X portc=%02X 8072=%02X\n",
                        g_frame, m->maincpu.cyc, m->maincpu.pc, val, m->ppi_portc,
                        m->soundram[0x72]);
            // Port A write lowers OBF and clears the read-ack feedback until
            // the sound CPU consumes the latch.
            sound_control_w(m, (uint8_t)(m->ppi_portc & ~0xc0));
            break;
        case 0x15: videomode_w(m, val); break;            // PPI port B
        case 0x16:                                        // PPI port C
            sound_control_w(m, (uint8_t)((m->ppi_portc & 0xc0) | (val & 0x3f)));
            break;
        case 0x17:                                        // PPI control: BSR on port C
            if (!(val & 0x80)) {
                int bit = (val >> 1) & 7;
                uint8_t pc = m->ppi_portc;
                if (bit < 6) {
                    if (val & 1) pc |= (1 << bit); else pc &= ~(1 << bit);
                    sound_control_w(m, pc);
                }
            }
            break;
        default: break;
    }
}

// ---- sound CPU (optional; silent until SN76489 is added) ------------------

static uint8_t sound_read(void *ud, uint16_t addr) {
    system2 *m = (system2 *)ud;
    if (addr < 0x8000) return m->soundrom[addr];
    // 2KB SRAM chip: only A[10:0] decoded, mirrors across 0x8000-0x9FFF
    if (addr >= 0x8000 && addr < 0xA000) return m->soundram[addr & 0x7FF];
    if ((addr & 0xe000) == 0xe000) {
        g_latch_reads++;
        // sound CPU read of the latch toggles PPI port C bit6 (sound ack),
        // which the main CPU polls to confirm the command was consumed.
        m->ppi_portc &= ~0x40;
        sound_control_w(m, (uint8_t)(m->ppi_portc | 0xc0));
        return m->soundlatch;
    }
    return 0xff;
}

static void sound_write(void *ud, uint16_t addr, uint8_t val) {
    system2 *m = (system2 *)ud;
    // 2KB SRAM chip: only A[10:0] decoded, mirrors across 0x8000-0x9FFF
    if (addr >= 0x8000 && addr < 0xA000) {
        uint16_t idx = addr & 0x7FF;
        if (g_sn_log && g_sn_logfile && idx == 0x072)
            fprintf(g_sn_logfile, "[ram72] f=%lu scyc=%lu pc=%04X %02X->%02X (addr=%04X)\n",
                    g_frame, m->soundcpu.cyc, m->soundcpu.pc, m->soundram[0x72], val, addr);
        m->soundram[idx] = val;
        return;
    }
    if ((addr & 0xe000) == 0xa000) {
        g_sn_writes++;
        if (g_sn_log && g_sn_logfile)
            fprintf(g_sn_logfile, "[sn1] f=%lu scyc=%lu pc=%04X %02X\n",
                    g_frame, m->soundcpu.cyc, m->soundcpu.pc, val);
        sn_write(&m->sn1, val);
        return;
    }
    if ((addr & 0xe000) == 0xc000) {
        g_sn_writes++;
        if (g_sn_log && g_sn_logfile)
            fprintf(g_sn_logfile, "[sn2] f=%lu scyc=%lu pc=%04X %02X\n",
                    g_frame, m->soundcpu.cyc, m->soundcpu.pc, val);
        sn_write(&m->sn2, val);
        return;
    }
}

static uint8_t sound_port_in(z80 *z, uint8_t port) { (void)z; (void)port; return 0xff; }
static void sound_port_out(z80 *z, uint8_t port, uint8_t val) { (void)z; (void)port; (void)val; }

// ---- ROM loading ----------------------------------------------------------

static int load(const char *dir, const char *name, uint8_t *dst, unsigned off, unsigned len) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", dir, name);
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "missing ROM: %s\n", path); return 1; }
    size_t n = fread(dst + off, 1, len, f);
    fclose(f);
    if (n != len) { fprintf(stderr, "short read %s: %zu/%u\n", path, n, len); return 1; }
    return 0;
}

int machine_init(system2 *m, const char *romdir) {
    memset(m, 0, sizeof(*m));
    int err = 0;

    // main CPU ROMs (region layout per MAME wbml ROM_START)
    memset(m->mainrom, 0xff, MAINROM_SIZE);
    err |= load(romdir, "epr-11031a.90", m->mainrom, 0x00000, 0x8000);
    err |= load(romdir, "epr-11032.91",  m->mainrom, 0x10000, 0x8000);
    err |= load(romdir, "epr-11033.92",  m->mainrom, 0x18000, 0x8000);
    err |= load(romdir, "317-0043.key",  m->key,     0x00000, 0x2000);

    err |= load(romdir, "epr-11037.126", m->soundrom, 0x0000, 0x8000);

    err |= load(romdir, "epr-11034.4", m->tilerom, 0x00000, 0x8000);
    err |= load(romdir, "epr-11035.5", m->tilerom, 0x08000, 0x8000);
    err |= load(romdir, "epr-11036.6", m->tilerom, 0x10000, 0x8000);

    err |= load(romdir, "epr-11028.87", m->spriterom, 0x00000, 0x8000);
    err |= load(romdir, "epr-11027.86", m->spriterom, 0x08000, 0x8000);
    err |= load(romdir, "epr-11030.89", m->spriterom, 0x10000, 0x8000);
    err |= load(romdir, "epr-11029.88", m->spriterom, 0x18000, 0x8000);

    err |= load(romdir, "pr11026.20", m->color_prom, 0x0000, 0x0100);
    err |= load(romdir, "pr11025.14", m->color_prom, 0x0100, 0x0100);
    err |= load(romdir, "pr11024.8",  m->color_prom, 0x0200, 0x0100);
    err |= load(romdir, "pr5317.37",  m->lookup_prom, 0x0000, 0x0100);
    if (err) return err;

    // MC-8123 decode the whole main region into opcode/data views
    mc8123_decode(m->mainrom, m->key, m->opcodes, m->data, MAINROM_SIZE);

    // DIP switches default position (all 1 = MAME default / off)
    m->in.swa = 0xff;
    m->in.swb = 0xff;

    z80_init(&m->maincpu);
    m->maincpu.userdata = m;
    m->maincpu.read_byte = main_read;
    m->maincpu.write_byte = main_write;
    m->maincpu.fetch_opcode = main_fetch;
    m->maincpu.port_in = main_port_in;
    m->maincpu.port_out = main_port_out;

    z80_init(&m->soundcpu);
    m->soundcpu.userdata = m;
    m->soundcpu.read_byte = sound_read;
    m->soundcpu.write_byte = sound_write;
    m->soundcpu.port_in = sound_port_in;
    m->soundcpu.port_out = sound_port_out;

    // SN76489A clocks: SN1 = SOUND_CLOCK/4 = 2MHz, SN2 = SOUND_CLOCK/2 = 4MHz.
    sn_init(&m->sn1, SOUND_XTAL / 4, AUDIO_SAMPLE_RATE);
    sn_init(&m->sn2, SOUND_XTAL / 2, AUDIO_SAMPLE_RATE);
    m->ppi_portc = 0xc0;
    m->videoram_bank = m->ppi_portc;
    m->sound_nmi_mask = 1;
    m->sound_nmi_prev = 1;

    return 0;
}

void machine_reset(system2 *m) {
    // Preserve decoded ROMs and DIP switches, clear everything else.
    uint8_t swa = m->in.swa, swb = m->in.swb;

    memset(m->ram,          0, sizeof(m->ram));
    memset(m->spriteram,    0, sizeof(m->spriteram));
    memset(m->paletteram,   0, sizeof(m->paletteram));
    memset(m->videoram,     0, sizeof(m->videoram));
    memset(m->soundram,     0, sizeof(m->soundram));
    memset(m->mix_collide,    0, sizeof(m->mix_collide));
    memset(m->sprite_collide, 0, sizeof(m->sprite_collide));
    m->mix_collide_summary = 0;
    m->sprite_collide_summary = 0;

    m->bank1 = 0;
    m->videoram_bank = 0;
    m->video_mode = 0;
    m->flip = 0;
    m->soundlatch = 0;
    m->ppi_portc = 0xc0;
    m->videoram_bank = m->ppi_portc;
    m->sound_nmi_mask = 1;
    m->sound_nmi_prev = 1;

    z80_init(&m->maincpu);
    m->maincpu.userdata   = m;
    m->maincpu.read_byte  = main_read;
    m->maincpu.write_byte = main_write;
    m->maincpu.fetch_opcode = main_fetch;
    m->maincpu.port_in    = main_port_in;
    m->maincpu.port_out   = main_port_out;

    z80_init(&m->soundcpu);
    m->soundcpu.userdata   = m;
    m->soundcpu.read_byte  = sound_read;
    m->soundcpu.write_byte = sound_write;
    m->soundcpu.port_in    = sound_port_in;
    m->soundcpu.port_out   = sound_port_out;

    sn_init(&m->sn1, SOUND_XTAL / 4, AUDIO_SAMPLE_RATE);
    sn_init(&m->sn2, SOUND_XTAL / 2, AUDIO_SAMPLE_RATE);

    m->in.swa = swa;
    m->in.swb = swb;
}

// Run both CPUs interleaved in slices so the sound latch handshake and audio
// register writes line up with their timing; generate audio per slice.
#define SLICES 8

int machine_run_frame(system2 *m, uint32_t *framebuffer, int16_t *audio) {
    const int total_samples = AUDIO_SAMPLE_RATE / 60;  // ~735
    int32_t mix[AUDIO_MAX_FRAME];
    int produced = 0;

    for (int sl = 0; sl < SLICES; sl++) {
        unsigned long mtarget = m->maincpu.cyc + CYC_PER_FRAME / SLICES;
        while (m->maincpu.cyc < mtarget)
            z80_step(&m->maincpu);

#if RUN_SOUND_CPU
        unsigned long starget = m->soundcpu.cyc + SCYC_PER_FRAME / SLICES;
        while (m->soundcpu.cyc < starget) {
            z80_step(&m->soundcpu);
            if (m->soundcpu.pc == 0x0038) g_snd_irq_taken++;  // entered IRQ vector
            if (m->soundcpu.pc == 0x00DC) g_snd_loop++;       // main loop completed one turn
            if (m->soundcpu.pc == 0x03AF) g_snd_driver++;     // music driver entered
        }

        // sound IRQ fires on a scanline timer ~4x/frame (every other slice).
        // (sound NMI is handled immediately in sound_control_w, not here.)
        if ((sl & 1) == 0) {
            g_snd_irq++;
            z80_gen_int(&m->soundcpu, 0xff);
        }
#endif

        // render this slice's audio
        int n = total_samples * (sl + 1) / SLICES - produced;
        if (n > 0) {
            for (int i = 0; i < n; i++) mix[produced + i] = 0;
            // gain 0 keeps a chip's clock advancing while muting it (for solo diagnostics)
            sn_render_add(&m->sn1, mix + produced, n, g_sn_solo == 2 ? 0.0 : 0.40);
            sn_render_add(&m->sn2, mix + produced, n, g_sn_solo == 1 ? 0.0 : 0.60);
            produced += n;
        }
    }

    // VBLANK -> main CPU IRQ (IM1 -> RST 38), once per frame
    z80_gen_int(&m->maincpu, 0xff);

    // clamp mix to int16
    for (int i = 0; i < produced; i++) {
        int32_t v = mix[i];
        if (v > 32767) v = 32767;
        if (v < -32768) v = -32768;
        audio[i] = (int16_t)v;
    }

    video_render(m, framebuffer);
    g_frame++;
    return produced;
}

// ---------------------------------------------------------------------------
// Save / load state
// ---------------------------------------------------------------------------

void machine_save(const system2 *m, savestate *s) {
    memcpy(s->ram,        m->ram,        sizeof(s->ram));
    memcpy(s->spriteram,  m->spriteram,  sizeof(s->spriteram));
    memcpy(s->paletteram, m->paletteram, sizeof(s->paletteram));
    memcpy(s->soundram,   m->soundram,   sizeof(s->soundram));
    s->bank1        = m->bank1;
    s->videoram_bank = m->videoram_bank;
    s->video_mode   = m->video_mode;
    s->flip         = m->flip;
    s->soundlatch   = m->soundlatch;
    s->ppi_portc    = m->ppi_portc;
    s->valid        = 1;
}

void machine_load(system2 *m, const savestate *s) {
    if (!s->valid) return;
    memcpy(m->ram,        s->ram,        sizeof(s->ram));
    memcpy(m->spriteram,  s->spriteram,  sizeof(s->spriteram));
    memcpy(m->paletteram, s->paletteram, sizeof(s->paletteram));
    memcpy(m->soundram,   s->soundram,   sizeof(s->soundram));
    m->bank1        = s->bank1;
    m->videoram_bank = s->videoram_bank;
    m->video_mode   = s->video_mode;
    m->flip         = s->flip;
    m->soundlatch   = s->soundlatch;
    m->ppi_portc    = s->ppi_portc;
}

// ---------------------------------------------------------------------------
// Difficulty / cheat patches
// ---------------------------------------------------------------------------

void machine_set_difficulty(system2 *m, int difficulty) {
    // SWB bits 7-6 select DIP difficulty (active-low: 1=OFF, 0=ON).
    // Easy:   bits 7-6 = 11  (0xC0 mask, both OFF) → swb unchanged from 0xFF
    // Normal: bits 7-6 = 10  (bit 6 ON)            → clear bit 6
    // Hard:   bits 7-6 = 01  (bit 7 ON)            → clear bit 7
    uint8_t swb = 0xff;
    switch (difficulty) {
    case 0: swb = 0xff;         break;  // EASY:   DIP Easy
    case 1: swb = 0xff & ~0x40; break;  // NORMAL: DIP Normal
    case 2: swb = 0xff & ~0x80; break;  // HARD:   DIP Hard
    case 3: swb = 0xff;         break;  // CHEAT:  DIP Easy + RAM patches
    }
    m->in.swb = swb;
}

// RAM offsets (relative to m->ram, base address C000):
//   C202 (offset 0x202): current HP hearts (initialised from DIP, max=C203=5).
//   C201 (offset 0x201): HP sub-counter / animation state.
//
// Gold address: TBD — needs a live debugging session to confirm.
//   Candidate range: C210-C220 (16-bit BCD or binary).
//   To find it: run the game, collect a coin, diff the RAM snapshot.

#define RAM_HP         0x202  // current HP (byte, 0..max)
#define RAM_HP_MAX     0x203  // max HP     (byte, set to 5 at new game)
// Gold is stored as decimal digits: hundreds@C21D, tens@C21C, units@C21B
#define RAM_GOLD_UNITS 0x21B
#define RAM_GOLD_TENS  0x21C
#define RAM_GOLD_HUNDS 0x21D

static unsigned read_gold(system2 *m) {
    return m->ram[RAM_GOLD_HUNDS] * 100u
         + m->ram[RAM_GOLD_TENS]  * 10u
         + m->ram[RAM_GOLD_UNITS];
}

static void write_gold(system2 *m, unsigned g) {
    m->ram[RAM_GOLD_HUNDS] = (uint8_t)(g / 100 % 10);
    m->ram[RAM_GOLD_TENS]  = (uint8_t)(g /  10 % 10);
    m->ram[RAM_GOLD_UNITS] = (uint8_t)(g        % 10);
}

void machine_easy_tick(system2 *m) {
    // --- Damage reduction: halve each HP loss ---
    static uint8_t s_prev_hp = 0xFF;
    uint8_t hp = m->ram[RAM_HP];
    if (s_prev_hp != 0xFF && hp < s_prev_hp) {
        unsigned loss    = s_prev_hp - hp;
        unsigned reduced = (loss > 1) ? (loss + 1) / 2 : loss;  // ceil(loss/2)
        m->ram[RAM_HP] = (uint8_t)(s_prev_hp - reduced);
        hp = m->ram[RAM_HP];
    }
    s_prev_hp = hp;

    // --- Coin minimum: each pickup adds between 15 and 66 gold ---
    static unsigned s_prev_gold = 0;
    unsigned gold = read_gold(m);
    if (gold > s_prev_gold) {
        unsigned gain = gold - s_prev_gold;
        if (gain < 15) gold = s_prev_gold + 15;
        if (gain > 66) gold = s_prev_gold + 66;
        write_gold(m, gold);
    }
    s_prev_gold = read_gold(m);
}
