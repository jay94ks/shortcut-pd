#ifndef __TIMERS_TIMER_H__
#define __TIMERS_TIMER_H__

#include <stdint.h>

// --> forward decls.
struct STimer;
enum {
    ETIMER_NONE = 0,
    ETIMER_ONESHOT,
    ETIMER_PERIODIC,
};

typedef void(* TimerCb)(const STimer* timer);
typedef void(* TimerCleanupCb)(const STimer* timer);

#define TIMER_TRIG_PENDING  0
#define TIMER_TIRG_ONESHOT  1
#define TIMER_TRIG_CLEANUP  2

/**
 * Timer structure.
 */
struct STimer {
    STimer* link;       // --> siblings.
    uint8_t type;       // --> type of the timer.
    uint8_t trig;       // --> timer triggered or not.
    uint32_t base;      // --> base tick.
    uint32_t time;      // --> target period.
    TimerCb cb;         // --> callback.
    TimerCleanupCb ccb; // --> cleanup callback.

    // --> user data.
    void* ptr;
    union {
        uint64_t u64;
        uint32_t u32;
        uint16_t u16;
        uint8_t u8;

        int64_t s64;
        int32_t s32;
        int16_t s16;
        int8_t s8;

        double f64;
        float f32;
    } data;
};

/**
 * Timer.
 */
class Timer {

};

#endif
