/* GPL-2.0-or-later. Rendering budget; never changes the emulated clock. */
#pragma once
#include <stdint.h>

typedef struct {
    uint64_t checked_at, previous_debt, last_draw_at, minimum_debt;
    uint32_t draw_us, skip_us, since_draw, max_gap_us;
    uint32_t interval_q16, credit_q16;
    unsigned minimum, buffer_limited;
    unsigned feedback, every;
} gb_pacer_t;

static inline void gb_pacer_reset(gb_pacer_t *p) {
    *p=(gb_pacer_t){.every=2,.max_gap_us=200000,
        .interval_q16=2u*65536,.minimum=1};
}

static inline uint32_t gb_pacer_average(uint32_t previous,uint64_t elapsed) {
    if(elapsed>1000000)elapsed=1000000;
    if(!previous)return (uint32_t)elapsed;
    /* Recover promptly from the two initial, fully repainted buffers. */
    unsigned weight=elapsed<previous?4:8;
    return (uint32_t)(((uint64_t)previous*(weight-1)+elapsed)/weight);
}

static inline void gb_pacer_observe(gb_pacer_t *p,int drawn,uint64_t elapsed) {
    if(drawn)p->draw_us=gb_pacer_average(p->draw_us,elapsed);
    else p->skip_us=gb_pacer_average(p->skip_us,elapsed);
}

static inline void gb_pacer_tick(gb_pacer_t *p,uint64_t now,
                                  uint64_t debt,unsigned hz,unsigned requested) {
    if(!p->checked_at){p->checked_at=now;p->previous_debt=debt;p->last_draw_at=now;p->minimum_debt=debt;}
    else {
      if(debt<p->minimum_debt)p->minimum_debt=debt;
      if(now-p->checked_at>=1000000) {
        /* Costs outside the Guest callback are not in observe(). Account for
         * them through measured debt growth and insufficient catch-up. */
        /* Stable historical debt still needs real catch-up work. Hold a
         * reduced render rate until caught up; never erase CPU/APU debt. */
        /* A batch can have a large entry debt even though the queue was
         * caught up earlier in this window. Only persistent lag warrants
         * feedback; reacting to that transient peak costs real LCD frames. */
        if(p->minimum_debt>2000000 && (debt>p->previous_debt+2000000 ||
            (p->minimum_debt>3000000 && debt+1000000>=p->previous_debt)) &&
            p->feedback<15)++p->feedback;
        else if(p->feedback && p->minimum_debt<2000000)--p->feedback;
        p->checked_at=now;p->previous_debt=debt;p->minimum_debt=debt;
      }
    }
    uint32_t interval=2u*65536;
    uint32_t period=1000000/hz,budget=period-period/10;
    if(p->draw_us && p->skip_us) {
        if(p->draw_us<=budget)interval=65536;
        else if(p->skip_us>=budget)interval=16u*65536;
        else {
            uint32_t extra=p->draw_us-p->skip_us,room=budget-p->skip_us;
            /* Retain fractional intervals: 2.01 frames must not become a
             * permanent 3-frame interval (30 -> 20 fps on NTSC). */
            uint64_t estimate=((uint64_t)extra*65536+room-1)/room;
            interval=estimate>16u*65536?16u*65536:(uint32_t)estimate;
        }
    }
    /* Near two frames, fractional jitter can request adjacent draws while
     * the Surface release event is still queued behind this callback. After
     * observing real backpressure, prefer a stable two-frame cadence in this
     * narrow band. Faster scenes automatically leave the band. */
    if(interval<2u*65536-65536/8)p->buffer_limited=0;
    else if(p->buffer_limited && interval<2u*65536)interval=2u*65536;
    interval+=p->feedback*65536;
    if(interval>16u*65536)interval=16u*65536;
    if(!requested)requested=1;
    if(requested>16)requested=16;
    if(interval<requested*65536)interval=requested*65536;
    p->interval_q16=interval;
    p->every=(interval+65535)/65536;
    p->minimum=requested;
    p->max_gap_us=200000*requested;
}
static inline int gb_pacer_due(gb_pacer_t *p,uint64_t now) {
    if(p->since_draw<UINT32_MAX)++p->since_draw;
    p->credit_q16+=65536;
    /* A blocked Surface retains one opportunity, not a burst of overdue
     * renders once a buffer becomes available. Keep the fractional remainder. */
    uint32_t cap=p->interval_q16+65535;
    if(p->credit_q16>cap)p->credit_q16=cap;
    /* When even a skipped frame is too expensive, keep a bounded opportunity
     * for visual feedback. This is a maximum gap, not a fixed low-FPS mode. */
    return (p->since_draw>=p->minimum && p->credit_q16>=p->interval_q16) ||
        now-p->last_draw_at>=p->max_gap_us;
}
static inline void gb_pacer_drew(gb_pacer_t *p,uint64_t now) {
    p->since_draw=0;p->last_draw_at=now;
    if(p->credit_q16>=p->interval_q16)p->credit_q16-=p->interval_q16;
    else p->credit_q16=0;
}
