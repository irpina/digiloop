/* SPDX-License-Identifier: GPL-2.0-or-later */
/* MIDI LOOPBACK for the Digitone mk1 and Digitone Keys (core-dn1 5.1): an internal cable from a MIDI track's output
 * to the unit's own input.
 *
 * LOOP is knob E on the MIDI tracks' TRIG page (one of its three empty boxes), a core lane parameter: OFF or ON, a
 * value per MIDI track and per pattern (it takes no p-locks). The LOOPBACK tile in the Mod Menu opens the same four
 * switches, one per MIDI track.
 *
 * The MIDI tracks' messages leave through the sequencer's MIDI engine, which writes them straight to DIN and USB:
 * they never pass the port switch that ev_midi_out sees, so this mod takes them at ev_seq_midi, where the engine
 * queues each whole message. A channel message on the channel of a MIDI track whose LOOP is ON (its CHAN, when
 * that is on) is taken there (it is not sent out) and kept in a ring; the UI task's next tick
 * (30 a second) plays each into the unit with core_midi_receive, as if it had come in on DIN: the stock applies it
 * by MIDI CONFIG > CHANNELS, so a CC on a synth track's channel turns that track's parameter, one on the FX channel
 * a master effect's, and notes play the track. So a MIDI track sequences and p-locks the Digitone itself, its
 * master effects included.
 *
 * ev_seq_midi runs in the engine's tick, the ring's writer; the UI task is its reader. A message the stock sends
 * while it applies a looped one (an echo) goes out, not round again. */
#include "digitone-mk1/core3.h"

typedef void (*rect_fn)(void *bmp, int x0, int y0, int x1, int y1, int colour);
typedef void (*text_fn)(void *bmp, const void *font, int x, int y, int maxlen, const char *fmt, ...);

#define LOOP_ID         186             /* the parameter, a lane (param:186): 0 OFF, 1 ON */
/* A MIDI track's settings are its kit sound's bytes from +0xaf, by the parameter's slot: CHAN (id 142, slot 17) at
 * +0xc0, 0-15, and on (FUNC + its knob) while bit 0 of +0x141 is set. */
#define KIT_SOUND(kit, t)  ((kit) + 0x18 + 326 * (t))
#define CHAN(s)            ((s)[0xc0])
#define CHAN_ON(s)         ((s)[0x141] & 1)
#define RING            128             /* messages; a power of two */

static volatile uint32_t ring[RING];    /* len << 24 | the message's three bytes, msg[0] highest */
static volatile uint32_t head, tail;    /* head: ev_seq_midi's, tail: the UI task's */
static volatile int32_t applying;       /* the UI task is playing a message in */
static volatile uint32_t dropped;       /* a full ring's, counted */
static int32_t shown, shown_track;      /* frames the corner shows a switch */

/* Does a MIDI track with LOOP on send on channel ch (0-15)? */
static int32_t looped(int32_t ch)
{
    const uint8_t *kit = fw_kit;
    if (!kit)
        return 0;
    for (int32_t t = 4; t < 8; t++) {
        const uint8_t *s = KIT_SOUND(kit, t);
        if (core_lane_track(LOOP_ID, t) > 0 && CHAN_ON(s) && CHAN(s) == ch)
            return 1;
    }
    return 0;
}

/* The engine's tick: a whole message, before it is queued for DIN and USB. Nonzero: not sent. */
int32_t loop_seq(const uint8_t *msg, int32_t len, int32_t buffer)
{
    uint32_t st, h;
    (void)buffer;
    if (applying || len < 1 || len > 3)
        return 0;
    st = msg[0];
    if (st < 0x80 || st >= 0xF0 || !looped((int32_t)(st & 15)))
        return 0;
    h = head;
    if (h - tail >= RING) {
        dropped++;
        return 1;                       /* the loop's own: not sent, even when it cannot keep up */
    }
    ring[h & (RING - 1)] = (uint32_t)len << 24 | st << 16 | (len > 1 ? msg[1] << 8 : 0) | (len > 2 ? msg[2] : 0);
    head = h + 1;
    return 1;
}

/* The UI task, 30 times a second: what the engine queued since, played into the unit in order. */
void loop_tick(void *ctrl)
{
    uint32_t t = tail;
    if (shown > 0) {
        shown--;
        *((unsigned char *)ctrl + 0x20) = 1;
    }
    while (t != head) {
        uint32_t m = ring[t & (RING - 1)];
        uint8_t msg[4];
        msg[0] = m >> 16;
        msg[1] = m >> 8;
        msg[2] = m;
        msg[3] = 0;
        applying = 1;
        core_midi_receive(msg, (int32_t)(m >> 24));
        applying = 0;
        t++;
        tail = t;
    }
}

/* ---- the knob ------------------------------------------------------------------------------------ */
static void loop_format(int32_t value, char *buf)
{
    if (value >> 8) {
        buf[0] = 'O'; buf[1] = 'N'; buf[2] = 0;
    } else {
        buf[0] = 'O'; buf[1] = 'F'; buf[2] = 'F'; buf[3] = 0;
    }
}

const struct core_param loop_param = {
    LOOP_ID, 16, CORE_SLOT_LANE, 0, 1 << 8, 0, 0, -1, -1, -1, 0,
    "Loopback", (const char *)fw_str_empty, "LOOP", loop_format, (const char *)fw_str_empty, 0, CORE_LANE_NOLOCK
};
const struct core_knob loop_knob = { CORE_PAGE_MIDI_TRIG, 4, LOOP_ID };

/* ---- the Mod Menu: LOOPBACK opens the four MIDI tracks' switches ---------------------------------- */
struct item {
    const char *name;
    void (*open)(void *brain, void *ev, int32_t track);
    unsigned long tag;
    const unsigned short *icon;
};

static const unsigned short loop_icon[16] = {          /* a cable out and back in */
    0x0000, 0x3ffc, 0x4002, 0x4002, 0x47e2, 0x4812, 0x4812, 0x4812,
    0x4812, 0x47e2, 0x4002, 0x7f3e, 0x0000, 0x0000, 0xffff, 0x0000
};
static const unsigned short off_icon[16] = {           /* the cable, cut */
    0x0000, 0x3ffc, 0x4002, 0x4002, 0x47e2, 0x4812, 0x4812, 0x4002,
    0x4002, 0x4812, 0x4002, 0x7f3e, 0x0000, 0x0000, 0xffff, 0x0000
};

static const char *const names[2][4] = {
    { "MIDI 1 OFF", "MIDI 2 OFF", "MIDI 3 OFF", "MIDI 4 OFF" },
    { "MIDI 1 LOOP", "MIDI 2 LOOP", "MIDI 3 LOOP", "MIDI 4 LOOP" },
};
static struct item rows[4];
static const void *row_list[5];

static void row_pick(void *brain, void *event, int32_t track, int32_t index)
{
    (void)brain; (void)event; (void)track;
    if (index < 0 || index > 3)
        return;
    core_lane_set(LOOP_ID, 4 + index, -1, core_lane_track(LOOP_ID, 4 + index) > 0 ? 0 : 1);
    shown_track = index;
    shown = 30;
}

static void loop_open(void *brain, void *event, int32_t track)
{
    (void)brain; (void)event;
    for (int32_t i = 0; i < 4; i++) {
        int32_t on = core_lane_track(LOOP_ID, 4 + i) > 0;
        rows[i].name = names[on][i];
        rows[i].open = 0;
        rows[i].tag = CORE_MENU_ICON;
        rows[i].icon = on ? loop_icon : off_icon;
        row_list[i] = &rows[i];
    }
    row_list[4] = 0;
    core_menu_open(row_list, (track & 4) ? (track & 3) : 0, row_pick);
}

const struct item loop_menu = { "LOOPBACK", loop_open, CORE_MENU_ICON, loop_icon };

void loop_draw(void *bmp, void *ctrl)
{
    (void)ctrl;
    if (shown <= 0)
        return;
    ((rect_fn)fw_fillrect)(bmp, 76, 54, 127, 63, 0);
    ((text_fn)fw_textf)(bmp, (const void *)fw_font5, 79, 57, -1, "M%d LOOP %s", (int)shown_track + 1,
                        core_lane_track(LOOP_ID, 4 + shown_track) > 0 ? "ON" : "OFF");
    ((rect_fn)fw_fillrect)(bmp, 76, 54, 127, 63, -1);
}
