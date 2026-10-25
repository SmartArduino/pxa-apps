/* GPL-2.0-or-later. Rendering budget; never changes the emulated clock. */
#pragma once
#include <stdint.h>
#ifndef EMU_PACER_VISIBLE_BUDGET_US
#define EMU_PACER_VISIBLE_BUDGET_US 90000u
#endif

typedef struct {
    uint64_t checked_at, previous_debt, last_draw_at, minimum_debt;
    uint32_t draw_us, skip_us, since_draw, max_gap_us, draws_since_skip;
    uint32_t interval_q16, credit_q16;
    unsigned minimum, buffer_limited;
    unsigned feedback, every;
} emu_pacer_t;

static inline void emu_pacer_reset(emu_pacer_t *p) {
    *p=(emu_pacer_t){.every=2,.max_gap_us=200000,
        .interval_q16=2u*65536,.minimum=1};
}

static inline uint32_t emu_pacer_average(uint32_t previous,uint64_t elapsed) {
    if(elapsed>1000000)elapsed=1000000;
    if(!previous)return (uint32_t)elapsed;
    /* Recover promptly from the two initial, fully repainted buffers. */
    unsigned weight=elapsed<previous?4:8;
    return (uint32_t)(((uint64_t)previous*(weight-1)+elapsed)/weight);
}

static inline void emu_pacer_observe(emu_pacer_t *p,int drawn,uint64_t elapsed) {
    if(drawn) {
        p->draw_us=emu_pacer_average(p->draw_us,elapsed);
        if(p->draws_since_skip<UINT32_MAX)++p->draws_since_skip;
    } else {
        /* A load/ROM miss can make the initial skipped frame unusually slow.
         * A fresh probe after many draws describes the current scene better
         * than an average dominated by that old sample. */
        p->skip_us=emu_pacer_average(p->draws_since_skip>=30?0:p->skip_us,elapsed);
        p->draws_since_skip=0;
    }
}

static inline int emu_pacer_expensive_draw(const emu_pacer_t *p) {
    /* Once a render alone takes 100 ms, even continuous rendering cannot
     * exceed 10 FPS. Reserve a simulation-only frame when it is substantially
     * cheaper; CPU/APU time continues to advance normally. */
    return p->feedback>=3 && p->draw_us>=100000 &&
        p->skip_us && p->skip_us<p->draw_us/2;
}

static inline void emu_pacer_tick(emu_pacer_t *p,uint64_t now,
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
    /* Bound video starvation even when CPU/APU alone exceeds the budget.
     * Persistent lag gets a 90 ms render budget, leaving more CPU time while
     * targeting >10 LCD completions/sec with dispatch overhead. Before lag
     * is confirmed, retain the 70 ms budget. This only changes video cadence;
     * all CPU/APU frames and accumulated scheduler debt remain intact. */
    if(p->draw_us && p->skip_us) {
        uint32_t visible_budget=p->feedback>=3?EMU_PACER_VISIBLE_BUDGET_US:70000;
        uint32_t visible_interval=65536;
        if(p->draw_us<visible_budget) {
            uint64_t extra=(uint64_t)(visible_budget-p->draw_us)*65536/p->skip_us;
            visible_interval+=extra>15u*65536?15u*65536:(uint32_t)extra;
        }
        else if(emu_pacer_expensive_draw(p))visible_interval=2u*65536;
        if(interval>visible_interval)interval=visible_interval;
    }
    if(!requested)requested=1;
    if(requested>16)requested=16;
    if(interval<requested*65536)interval=requested*65536;
    p->interval_q16=interval;
    p->every=(interval+65535)/65536;
    p->minimum=requested;
    p->max_gap_us=(p->feedback>=3?120000:100000)*requested;
}
static inline int emu_pacer_due(emu_pacer_t *p,uint64_t now) {
    if(p->since_draw<UINT32_MAX)++p->since_draw;
    p->credit_q16+=65536;
    /* A blocked Surface retains one opportunity, not a burst of overdue
     * renders once a buffer becomes available. Keep the fractional remainder. */
    uint32_t cap=p->interval_q16+65535;
    if(p->credit_q16>cap)p->credit_q16=cap;
    /* The display deadline may already have expired inside a slow render.
     * Do not immediately draw again forever: measure one skipped frame after
     * an expensive initial render, and preserve it during sustained overload. */
    if(p->since_draw==1 && ((!p->skip_us && p->draw_us>=70000) ||
        emu_pacer_expensive_draw(p)))return 0;
    /* Recheck simulation-only cost during sustained overload. Otherwise a
     * costly startup sample can select continuous drawing forever and never
     * discover that skipping became useful. Caught-up games are unaffected. */
    if(p->since_draw==1 && p->feedback>=3 && p->draws_since_skip>=30)return 0;
    /* When even a skipped frame is too expensive, keep a bounded opportunity
     * for visual feedback. This is a maximum gap, not a fixed low-FPS mode. */
    return p->since_draw>=p->minimum && (p->credit_q16>=p->interval_q16 ||
        now-p->last_draw_at+p->skip_us>=p->max_gap_us);
}
static inline void emu_pacer_drew(emu_pacer_t *p,uint64_t now) {
    p->since_draw=0;p->last_draw_at=now;
    if(p->credit_q16>=p->interval_q16)p->credit_q16-=p->interval_q16;
    else p->credit_q16=0;
}
