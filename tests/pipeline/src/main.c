#include <zephyr/ztest.h>
#include "sensorhub.h"

static void enqueue_seq(uint32_t first, uint32_t count)
{
    for (uint32_t seq = first; seq < first + count; seq++) {
        sensorhub_enqueue(&(struct sample_msg){ .seq = seq });
    }
}

static void drain(void)
{
    struct sample_msg m;

    while (sensorhub_dequeue(&m, K_NO_WAIT) == 0) {
    }
}

static void pipeline_before(void *fixture)
{
    ARG_UNUSED(fixture);
    drain();
    sensorhub_reset_stats();
    sensorhub_set_rate(SENSORHUB_RATE_DEFAULT_HZ);
}

ZTEST(queue, test_fifo_order_and_stats)
{
    struct queue_stats st;
    struct sample_msg m;

    for (uint32_t seq = 0; seq < 3; seq++) {
        zassert_equal(sensorhub_enqueue(&(struct sample_msg){ .seq = seq }), 0,
                      "dropped a sample while the queue had room");
    }

    sensorhub_get_qstats(&st);
    zassert_equal(st.depth, 3);
    zassert_equal(st.capacity, SENSORHUB_QUEUE_CAPACITY);
    zassert_equal(st.high_watermark, 3);
    zassert_equal(st.drops, 0);

    for (uint32_t seq = 0; seq < 3; seq++) {
        zassert_ok(sensorhub_dequeue(&m, K_NO_WAIT));
        zassert_equal(m.seq, seq, "samples came out of order");
    }
    zassert_equal(sensorhub_dequeue(&m, K_NO_WAIT), -ENOMSG);

    sensorhub_get_qstats(&st);
    zassert_equal(st.depth, 0);
    zassert_equal(st.high_watermark, 3, "high-watermark must survive draining");
}

ZTEST(queue, test_full_queue_drops_oldest)
{
    const uint32_t extra = 5;
    const uint32_t total = SENSORHUB_QUEUE_CAPACITY + extra;
    struct queue_stats st;
    struct sample_msg m;

    for (uint32_t seq = 0; seq < total; seq++) {
        int expected = seq < SENSORHUB_QUEUE_CAPACITY ? 0 : 1;

        zassert_equal(sensorhub_enqueue(&(struct sample_msg){ .seq = seq }), expected,
                      "wrong drop count for seq %u", seq);
    }

    sensorhub_get_qstats(&st);
    zassert_equal(st.depth, SENSORHUB_QUEUE_CAPACITY);
    zassert_equal(st.high_watermark, SENSORHUB_QUEUE_CAPACITY);
    zassert_equal(st.drops, extra);

    /* The oldest samples were discarded; the newest remain, still in order */
    for (uint32_t seq = extra; seq < total; seq++) {
        zassert_ok(sensorhub_dequeue(&m, K_NO_WAIT));
        zassert_equal(m.seq, seq, "expected seq %u, got %u", seq, m.seq);
    }
    zassert_equal(sensorhub_dequeue(&m, K_NO_WAIT), -ENOMSG);
}

ZTEST(queue, test_reset_stats)
{
    struct queue_stats st;

    enqueue_seq(0, SENSORHUB_QUEUE_CAPACITY + 3);

    /* Drops clear; the high-watermark restarts from what is queued right now */
    sensorhub_reset_stats();
    sensorhub_get_qstats(&st);
    zassert_equal(st.drops, 0);
    zassert_equal(st.depth, SENSORHUB_QUEUE_CAPACITY);
    zassert_equal(st.high_watermark, SENSORHUB_QUEUE_CAPACITY);

    drain();
    sensorhub_reset_stats();
    sensorhub_get_qstats(&st);
    zassert_equal(st.high_watermark, 0);

    enqueue_seq(0, 2);
    sensorhub_get_qstats(&st);
    zassert_equal(st.high_watermark, 2);
}

ZTEST(rate, test_rate_limits)
{
    zassert_ok(sensorhub_set_rate(SENSORHUB_RATE_MIN_HZ));
    zassert_equal(sensorhub_get_rate(), SENSORHUB_RATE_MIN_HZ);
    zassert_ok(sensorhub_set_rate(SENSORHUB_RATE_MAX_HZ));
    zassert_equal(sensorhub_get_rate(), SENSORHUB_RATE_MAX_HZ);

    zassert_equal(sensorhub_set_rate(SENSORHUB_RATE_MIN_HZ - 1), -EINVAL);
    zassert_equal(sensorhub_set_rate(SENSORHUB_RATE_MAX_HZ + 1), -EINVAL);
    zassert_equal(sensorhub_set_rate(0), -EINVAL);
    zassert_equal(sensorhub_set_rate(-100), -EINVAL);
    zassert_equal(sensorhub_get_rate(), SENSORHUB_RATE_MAX_HZ,
                  "a rejected rate must not change the setting");
}

/*
 * A fast producer thread and a slower consumer thread share the queue while the
 * test thread keeps reading the statistics. Every sample has to be accounted
 * for exactly once: consumed + dropped + still queued == produced.
 */
#define PRODUCED  2000
#define BURST     40

static K_THREAD_STACK_DEFINE(producer_stack, 1024);
static K_THREAD_STACK_DEFINE(consumer_stack, 1024);
static struct k_thread producer_thread, consumer_thread;
static atomic_t producer_done;
static atomic_t consumed;
static atomic_t out_of_order;

static void producer(void *a, void *b, void *c)
{
    ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);

    for (uint32_t seq = 0; seq < PRODUCED; seq++) {
        sensorhub_enqueue(&(struct sample_msg){ .seq = seq });
        /* Produce in bursts larger than the queue so it overflows */
        if (seq % BURST == BURST - 1) {
            k_sleep(K_USEC(500));
        }
    }
    atomic_set(&producer_done, 1);
}

static void consumer(void *a, void *b, void *c)
{
    ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);
    struct sample_msg m;
    int64_t last_seq = -1;

    while (true) {
        if (sensorhub_dequeue(&m, K_USEC(200)) == 0) {
            /* Dropping only ever removes samples, it never reorders them */
            if ((int64_t)m.seq <= last_seq) {
                atomic_set(&out_of_order, 1);
            }
            last_seq = m.seq;
            atomic_inc(&consumed);
            k_sleep(K_USEC(50));
        } else if (atomic_get(&producer_done)) {
            break;
        }
    }
}

ZTEST(concurrency, test_every_sample_accounted_for)
{
    struct queue_stats st;
    uint32_t last_drops = 0;

    k_thread_create(&producer_thread, producer_stack, K_THREAD_STACK_SIZEOF(producer_stack),
                    producer, NULL, NULL, NULL, K_PRIO_PREEMPT(1), 0, K_NO_WAIT);
    k_thread_create(&consumer_thread, consumer_stack, K_THREAD_STACK_SIZEOF(consumer_stack),
                    consumer, NULL, NULL, NULL, K_PRIO_PREEMPT(2), 0, K_NO_WAIT);

    while (!atomic_get(&producer_done)) {
        sensorhub_get_qstats(&st);
        zassert_true(st.depth <= st.capacity, "depth %u > capacity", st.depth);
        zassert_true(st.high_watermark <= st.capacity, "hwm %u > capacity", st.high_watermark);
        zassert_true(st.drops >= last_drops, "drops went backwards");
        last_drops = st.drops;
        k_sleep(K_USEC(100));
    }

    zassert_ok(k_thread_join(&producer_thread, K_SECONDS(5)));
    zassert_ok(k_thread_join(&consumer_thread, K_SECONDS(5)));

    sensorhub_get_qstats(&st);
    zassert_true(st.drops > 0, "the test never overflowed the queue");
    zassert_equal(st.high_watermark, SENSORHUB_QUEUE_CAPACITY);
    zassert_equal((uint32_t)atomic_get(&consumed) + st.drops + st.depth, PRODUCED,
                  "consumed %ld + dropped %u + queued %u != produced %u",
                  (long)atomic_get(&consumed), st.drops, st.depth, PRODUCED);
    zassert_false(atomic_get(&out_of_order), "consumer saw samples out of order");
}

ZTEST_SUITE(queue, NULL, NULL, pipeline_before, NULL, NULL);
ZTEST_SUITE(rate, NULL, NULL, pipeline_before, NULL, NULL);
ZTEST_SUITE(concurrency, NULL, NULL, pipeline_before, NULL, NULL);
