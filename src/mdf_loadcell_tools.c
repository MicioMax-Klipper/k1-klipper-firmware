#include "basecmd.h"
#include "command.h"
#include "sched.h"
#include "board/misc.h"
#include "load_cell_probe.h"
#include <stdint.h>
#include <stdlib.h>

enum {
    MDF_REASON_NONE = 0,
    MDF_REASON_DF = 1,
    MDF_REASON_SAFE = 2,
};

#define MDF_DF_RING_SIZE 100
#define MDF_DF_LOG_MIN_DEFAULT 500

struct mdf_df_record {
    int32_t raw;
    int32_t df;
    int32_t d0;
    int32_t sum_df;
    uint8_t hits;
};

struct mdf_state {
    struct timer timer;
    struct load_cell_probe *lce;

    uint32_t sample_ticks;
    int32_t df_threshold;
    int32_t safe_threshold;
    uint8_t required_hits;
    int32_t df_noise_floor;
    int8_t polarity;
    int32_t df_log_min;

    int32_t raw0;
    int32_t prev_raw;
    int32_t last_raw;
    int32_t last_df;
    int32_t last_d0;
    int32_t df_sum;

    uint8_t hit_count;

    struct mdf_df_record df_ring[MDF_DF_RING_SIZE];
    uint8_t df_ring_pos;
    uint8_t df_ring_count;

    uint8_t active;
    uint8_t triggered;
    uint8_t trigger_reason;
};

static struct mdf_state mdf;

static void
mdf_ring_clear(void)
{
    mdf.df_ring_pos = 0;
    mdf.df_ring_count = 0;
    mdf.df_sum = 0;
}

static void
mdf_ring_log(int32_t raw, int32_t df, int32_t d0)
{
    if (abs(df) < mdf.df_log_min)
        return;

    struct mdf_df_record *r = &mdf.df_ring[mdf.df_ring_pos];

    r->raw = raw;
    r->df = df;
    r->d0 = d0;
    r->sum_df = mdf.df_sum;
    r->hits = mdf.hit_count;

    mdf.df_ring_pos++;
    if (mdf.df_ring_pos >= MDF_DF_RING_SIZE)
        mdf.df_ring_pos = 0;

    if (mdf.df_ring_count < MDF_DF_RING_SIZE)
        mdf.df_ring_count++;
}

static void
mdf_trigger(uint8_t reason, int32_t raw, int32_t df, int32_t d0)
{
    mdf.triggered = 1;
    mdf.trigger_reason = reason;
    mdf.active = 0;

    sendf("mdf_trigger reason=%c raw=%i df=%i d0=%i",
          mdf.trigger_reason, raw, df, d0);
}

static uint_fast8_t
mdf_timer_event(struct timer *t)
{
    if (!mdf.active || mdf.triggered)
        return SF_DONE;

    int32_t raw = load_cell_probe_get_last_raw_sample(mdf.lce);
    int32_t df = raw - mdf.prev_raw;
    int32_t d0 = raw - mdf.raw0;
    int32_t event_df = df * mdf.polarity;

    mdf.df_sum += df;

    mdf.last_raw = raw;
    mdf.last_df = df;
    mdf.last_d0 = d0;

    if (event_df < mdf.df_noise_floor) {
        // Ignore small normalized changes, zero, and opposite-direction changes.
    } else if (event_df >= mdf.df_threshold) {
        if (mdf.hit_count < 255)
            mdf.hit_count++;
    } else {
        mdf.hit_count = 0;
    }

    mdf_ring_log(raw, df, d0);

    if (mdf.hit_count >= mdf.required_hits) {
        mdf_trigger(MDF_REASON_DF, raw, df, d0);
        return SF_DONE;
    }

    if (abs(d0) >= mdf.safe_threshold) {
        mdf_trigger(MDF_REASON_SAFE, raw, df, d0);
        return SF_DONE;
    }

    mdf.prev_raw = raw;
    mdf.timer.waketime += mdf.sample_ticks;
    return SF_RESCHEDULE;
}

void
command_mdf_ping(uint32_t *args)
{
    sendf("mdf_pong value=%u", 117);
}
DECL_COMMAND(command_mdf_ping, "mdf_ping");

void
command_mdf_force_query(uint32_t *args)
{
    uint8_t oid = args[0];
    struct load_cell_probe *lce = load_cell_probe_oid_lookup(oid);
    int32_t raw = load_cell_probe_get_last_raw_sample(lce);

    sendf("mdf_force_state oid=%c raw=%i", oid, raw);
}
DECL_COMMAND(command_mdf_force_query, "mdf_force_query oid=%c");

void
command_mdf_config(uint32_t *args)
{
    uint8_t oid = args[0];

    mdf.lce = load_cell_probe_oid_lookup(oid);
    mdf.sample_ticks = args[1];
    mdf.df_threshold = args[2];
    mdf.safe_threshold = args[3];
    mdf.required_hits = args[4];
    mdf.df_noise_floor = args[5];
    mdf.polarity = args[6] ? -1 : 1;
    mdf.df_log_min = args[7];

    if (!mdf.required_hits)
        mdf.required_hits = 1;

    if (mdf.df_noise_floor < 0)
        mdf.df_noise_floor = 0;

    if (mdf.df_log_min < 0)
        mdf.df_log_min = MDF_DF_LOG_MIN_DEFAULT;

    mdf.active = 0;
    mdf.triggered = 0;
    mdf.trigger_reason = MDF_REASON_NONE;
    mdf.hit_count = 0;
    mdf.timer.func = mdf_timer_event;

    mdf_ring_clear();
}
DECL_COMMAND(command_mdf_config,
    "mdf_config oid=%c sample_ticks=%u df_threshold=%i safe_threshold=%i required_hits=%c df_noise_floor=%i invert=%c df_log_min=%i");

void
command_mdf_start(uint32_t *args)
{
    if (!mdf.lce)
        shutdown("MDF not configured");

    int32_t raw = load_cell_probe_get_last_raw_sample(mdf.lce);

    sched_del_timer(&mdf.timer);

    mdf.raw0 = raw;
    mdf.prev_raw = raw;
    mdf.last_raw = raw;
    mdf.last_df = 0;
    mdf.last_d0 = 0;
    mdf.df_sum = 0;
    mdf.hit_count = 0;
    mdf.triggered = 0;
    mdf.trigger_reason = MDF_REASON_NONE;
    mdf.active = 1;

    mdf.timer.waketime = timer_read_time() + mdf.sample_ticks;
    sched_add_timer(&mdf.timer);
}
DECL_COMMAND(command_mdf_start, "mdf_start");

void
command_mdf_stop(uint32_t *args)
{
    sched_del_timer(&mdf.timer);
    mdf.active = 0;
}
DECL_COMMAND(command_mdf_stop, "mdf_stop");

void
command_mdf_status_query(uint32_t *args)
{
    sendf("mdf_status active=%c triggered=%c reason=%c raw0=%i raw=%i df=%i d0=%i",
          mdf.active, mdf.triggered, mdf.trigger_reason,
          mdf.raw0, mdf.last_raw, mdf.last_df, mdf.last_d0);
}
DECL_COMMAND(command_mdf_status_query, "mdf_status_query");

void
command_mdf_clear(uint32_t *args)
{
    mdf_ring_clear();
    mdf.hit_count = 0;
}
DECL_COMMAND(command_mdf_clear, "mdf_clear");

void
command_mdf_dump(uint32_t *args)
{
    sendf("mdf_dump_begin count=%c pos=%c sum=%i",
          mdf.df_ring_count, mdf.df_ring_pos, mdf.df_sum);

    uint8_t start = 0;
    if (mdf.df_ring_count >= MDF_DF_RING_SIZE)
        start = mdf.df_ring_pos;

    for (uint8_t i = 0; i < mdf.df_ring_count; i++) {
        uint8_t idx = start + i;
        if (idx >= MDF_DF_RING_SIZE)
            idx -= MDF_DF_RING_SIZE;

        struct mdf_df_record *r = &mdf.df_ring[idx];

        sendf("mdf_dump_item i=%c raw=%i df=%i d0=%i sum=%i hits=%c",
              i, r->raw, r->df, r->d0, r->sum_df, r->hits);
    }

    sendf("mdf_dump_end count=%c", mdf.df_ring_count);
}
DECL_COMMAND(command_mdf_dump, "mdf_dump");
