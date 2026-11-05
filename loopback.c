/* SPDX-License-Identifier: GPL-2.0-or-later */
/* MIDI LOOPBACK for the Digitone mk1 and Digitone Keys (core-dn1 5.0.1): an internal cable from the MIDI tracks'
 * output to the unit's own input, on one channel.
 *
 * The MIDI tracks' messages leave through the sequencer's MIDI engine, which writes them straight to DIN and USB:
 * they never pass the port switch that ev_midi_out sees, so this mod takes them at ev_seq_midi, where the engine
 * queues each whole message. A channel message on the chosen channel is taken there (it is not sent out) and
 * kept in a ring; the UI task's next tick (30 a second) plays each into the unit with core_midi_receive, as if it
 * had come in on DIN: the stock applies it by MIDI CONFIG > CHANNELS, so a CC on a track's channel turns that
 * track's parameter, one on the FX channel a master effect's, and notes play the track. So a MIDI track set to
 * that channel sequences and p-locks the Digitone itself, its master effects included.
 *
 * ev_seq_midi runs in the engine's tick, the ring's writer; the UI task is its reader. A message the stock sends
 * while it applies a looped one (an echo) goes out, not round again. LOOPBACK in the Mod Menu picks the channel:
 * OFF (the stock: everything goes out) or 1-16, saved with the project ("LOOP"). */
#include "digitone-mk1/core3.h"

typedef void (*rect_fn)(void *bmp, int x0, int y0, int x1, int y1, int colour);
typedef void (*text_fn)(void *bmp, const void *font, int x, int y, int maxlen, const char *fmt, ...);

#define RING  128                        /* messages; a power of two */

struct save {
    int32_t channel;                     /* 1-16, 0 off */
};
static struct save save;

static volatile uint32_t ring[RING];     /* len << 24 | the message's three bytes, msg[0] highest */
static volatile uint32_t head, tail;     /* head: ev_seq_midi's, tail: the UI task's */
static volatile int32_t applying;        /* the UI task is playing a message in */
static volatile uint32_t dropped;        /* a full ring's, counted */
static int32_t shown;                    /* frames the corner shows the channel */

/* The engine's tick: a whole message, before it is queued for DIN and USB. Nonzero: not sent. */
int32_t loop_seq(const uint8_t *msg, int32_t len, int32_t buffer)
{
    uint32_t st, h;
    (void)buffer;
    if (save.channel <= 0 || applying || len < 1 || len > 3)
        return 0;
    st = msg[0];
    if (st < 0x80 || st >= 0xF0 || (int32_t)(st & 15) != save.channel - 1)
        return 0;
    h = head;
    if (h - tail >= RING) {
        dropped++;
        return 1;                        /* the loop's own: not sent, even when it cannot keep up */
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

/* ---- the Mod Menu: LOOPBACK, OFF or a channel ---------------------------------------------------- */
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

#define CH(n) { "CH " #n, 0, CORE_MENU_ICON, loop_icon }
static const struct item channels[17] = {
    { "OFF", 0, CORE_MENU_ICON, loop_icon },
    CH(1), CH(2), CH(3), CH(4), CH(5), CH(6), CH(7), CH(8),
    CH(9), CH(10), CH(11), CH(12), CH(13), CH(14), CH(15), CH(16)
};
static const void *const channel_list[18] = {
    &channels[0], &channels[1], &channels[2], &channels[3], &channels[4], &channels[5], &channels[6],
    &channels[7], &channels[8], &channels[9], &channels[10], &channels[11], &channels[12], &channels[13],
    &channels[14], &channels[15], &channels[16], 0
};

static void channel_pick(void *brain, void *event, int32_t track, int32_t index)
{
    (void)brain; (void)event; (void)track;
    save.channel = index;                /* 0 OFF, 1-16 */
    shown = 30;
}

static void loop_open(void *brain, void *event, int32_t track)
{
    (void)brain; (void)event; (void)track;
    core_menu_open(channel_list, save.channel >= 0 && save.channel <= 16 ? save.channel : 0, channel_pick);
}

const struct item loop_menu = { "LOOPBACK", loop_open, CORE_MENU_ICON, loop_icon };

static void loop_loaded(int32_t found)
{
    (void)found;                         /* none: zeros, OFF */
    if (save.channel < 0 || save.channel > 16)
        save.channel = 0;
}
const struct core_projdata loop_projdata = { 0x4c4f4f50u /* "LOOP" */, sizeof(save), &save, loop_loaded };

void loop_draw(void *bmp, void *ctrl)
{
    (void)ctrl;
    if (shown <= 0)
        return;
    ((rect_fn)fw_fillrect)(bmp, 84, 54, 127, 63, 0);
    if (save.channel)
        ((text_fn)fw_textf)(bmp, (const void *)fw_font5, 87, 57, -1, "LOOP CH %d", (int)save.channel);
    else
        ((text_fn)fw_textf)(bmp, (const void *)fw_font5, 87, 57, -1, "LOOP OFF");
    ((rect_fn)fw_fillrect)(bmp, 84, 54, 127, 63, -1);
}
