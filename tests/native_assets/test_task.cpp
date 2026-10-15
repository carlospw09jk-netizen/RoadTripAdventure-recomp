// Pins the authored task scheduler contracts (docs/game-mechanics/task-scheduler.md).
// No game data.
#include "../../src/native/gameplay/task.h"

#include <cstdio>
#include <cstdlib>

using namespace rt::native::gameplay;

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

// Per-tag handler run counters.
static int g_runs[64];

// A handler that just counts its own runs (tag captured by value).
static TaskFn counter(int tag) {
    return [tag](Task&, TaskPool&) { ++g_runs[tag]; };
}

static void resetRuns() {
    for (int& r : g_runs) r = 0;
}

// spawn N tasks; every live task ticks exactly once per tick().
static void testSpawnTicksOncePerTick() {
    resetRuns();
    TaskPool pool;
    TaskId ids[5];
    for (int i = 0; i < 5; ++i) {
        const int tag = i;
        ids[i] = pool.spawn([tag](Task& self, TaskPool&) {
            ++g_runs[tag];
            // The counter increments BEFORE the walker's +0x8 bump.
            CHECK(self.tickCount == static_cast<std::uint32_t>(g_runs[tag] - 1));
        });
        CHECK(ids[i].valid());
    }
    CHECK(pool.liveCount() == 5);

    pool.tick();
    for (int i = 0; i < 5; ++i) {
        CHECK(g_runs[i] == 1);
        CHECK(pool.get(ids[i])->tickCount == 1u);
    }
    pool.tick();
    pool.tick();
    for (int i = 0; i < 5; ++i) {
        CHECK(g_runs[i] == 3);  // exactly one run per tick(), no doubles
        CHECK(pool.get(ids[i])->tickCount == 3u);
    }
    tick(pool);  // free-function form of the tick contract
    for (int i = 0; i < 5; ++i) {
        CHECK(g_runs[i] == 4);
        CHECK(pool.get(ids[i])->tickCount == 4u);
    }
}

// The pool holds exactly 48 slots; spawn #49 fails cleanly.
static void testPoolCapacity() {
    resetRuns();
    TaskPool pool;
    TaskId ids[48];
    for (int i = 0; i < 48; ++i) {
        ids[i] = pool.spawn(counter(i));
        CHECK(ids[i].valid());
    }
    CHECK(pool.liveCount() == 48);
    CHECK(!pool.spawn(counter(48)).valid());  // gs_tasks_spawn overflow
    CHECK(pool.liveCount() == 48);

    pool.tick();
    int total = 0;
    for (int i = 0; i < 48; ++i) total += g_runs[i];
    CHECK(total == 48);  // every slot ticked exactly once

    CHECK(pool.kill(ids[47]));
    TaskId reused = pool.spawn(counter(48));
    CHECK(reused.valid());
    CHECK(reused.index == 47);            // first free slot is rescanned from 0
    CHECK(!pool.isLive(ids[47]));         // stale handle does not resolve
    CHECK(pool.get(ids[47]) == nullptr);
}

// kill(id) removes the task (no further ticks) and its descendants; siblings live.
static void testKillRemovesTaskAndChildren() {
    resetRuns();
    TaskPool pool;
    //  R
    //  +- A -+- C -+- D
    //  |     +- (C's child D)
    //  +- B -+- E
    TaskId r = pool.spawn(counter(0));
    TaskId a = pool.spawn(counter(1), r);
    TaskId b = pool.spawn(counter(2), r);
    TaskId c = pool.spawn(counter(3), a);
    TaskId d = pool.spawn(counter(4), c);
    TaskId e = pool.spawn(counter(5), b);
    CHECK(pool.liveCount() == 6);

    pool.tick();
    for (int i = 0; i < 6; ++i) CHECK(g_runs[i] == 1);

    CHECK(pool.kill(a));  // kills a, c, d
    CHECK(pool.liveCount() == 3);
    CHECK(!pool.isLive(a));
    CHECK(!pool.isLive(c));
    CHECK(!pool.isLive(d));
    CHECK(pool.isLive(r));
    CHECK(pool.isLive(b));
    CHECK(pool.isLive(e));

    pool.tick();
    CHECK(g_runs[0] == 2);  // R keeps ticking
    CHECK(g_runs[1] == 1);  // A gone: no further ticks
    CHECK(g_runs[2] == 2);  // B (sibling of A) survives
    CHECK(g_runs[3] == 1);  // C gone
    CHECK(g_runs[4] == 1);  // D gone
    CHECK(g_runs[5] == 2);  // E survives

    CHECK(!pool.kill(a));  // double-kill is a no-op returning false
    CHECK(pool.kill(e));
    CHECK(pool.kill(b));
    CHECK(pool.kill(r));
    CHECK(pool.liveCount() == 0);
}

// Parent / first-child / next-sibling links stay valid across spawn and kill.
static void testTreeLinksStayValid() {
    resetRuns();
    TaskPool pool;
    TaskId r = pool.spawn(counter(0));      // root chain: r
    TaskId a = pool.spawn(counter(1), r);   // r -> a
    TaskId b = pool.spawn(counter(2), r);   // r -> a -> b
    TaskId c = pool.spawn(counter(3), a);   // a -> c
    CHECK(r.index == 0 && a.index == 1 && b.index == 2 && c.index == 3);

    CHECK(pool.rootHead() == r);
    CHECK(pool.get(r)->nextSibling.valid() == false);
    CHECK(pool.get(r)->firstChild == a);
    CHECK(pool.get(a)->nextSibling == b);
    CHECK(pool.get(b)->nextSibling.valid() == false);
    CHECK(pool.get(a)->parent == r);
    CHECK(pool.get(b)->parent == r);
    CHECK(pool.get(a)->firstChild == c);
    CHECK(pool.get(c)->parent == a);
    CHECK(pool.get(c)->firstChild.valid() == false);

    CHECK(pool.kill(a));  // drops a and c; r's chain becomes r -> b
    CHECK(pool.rootHead() == r);
    CHECK(pool.get(r)->firstChild == b);
    CHECK(pool.get(b)->nextSibling.valid() == false);
    CHECK(pool.get(b)->parent == r);
    CHECK(pool.liveCount() == 2);

    // A stale parent handle rejects the spawn instead of corrupting links.
    CHECK(!pool.spawn(counter(4), a).valid());
    CHECK(pool.liveCount() == 2);

    TaskId d = pool.spawn(counter(4), r);  // appended at the end: r -> b -> d
    CHECK(pool.get(b)->nextSibling == d);
    CHECK(pool.get(d)->nextSibling.valid() == false);
    CHECK(pool.get(d)->parent == r);
}

// A task can spawn a child that ticks the NEXT frame (spawn-then-poll pattern).
static void testChildSpawnsTickNextFrame() {
    resetRuns();
    TaskPool pool;
    TaskId child, grand;
    int parentRuns = 0, childRuns = 0, grandRuns = 0;

    TaskId parent = pool.spawn([&](Task& self, TaskPool& p) {
        ++parentRuns;
        if (self.tickCount == 0) {  // first tick: spawn the child
            child = p.spawn([&](Task& s, TaskPool& q) {
                ++childRuns;
                if (s.tickCount == 0) {  // child spawns a grandchild on its first tick
                    grand = q.spawn([&](Task&, TaskPool&) { ++grandRuns; }, child);
                }
            }, parent);
        }
    });

    pool.tick();  // frame 1: only the parent runs; the child is born mid-frame
    CHECK(parentRuns == 1);
    CHECK(childRuns == 0);
    CHECK(pool.isLive(child));
    CHECK(pool.get(child)->tickCount == 0u);  // no tick on the spawn frame
    CHECK(!grand.valid());

    pool.tick();  // frame 2: child's first tick; grandchild is born mid-frame
    CHECK(parentRuns == 2);
    CHECK(childRuns == 1);
    CHECK(grandRuns == 0);
    CHECK(grand.valid());
    CHECK(pool.get(grand)->tickCount == 0u);

    pool.tick();  // frame 3: grandchild's first tick
    CHECK(parentRuns == 3);
    CHECK(childRuns == 2);
    CHECK(grandRuns == 1);
    CHECK(pool.get(grand)->tickCount == 1u);

    pool.tick();
    CHECK(parentRuns == 4 && childRuns == 3 && grandRuns == 2);
    CHECK(pool.get(parent)->tickCount == 4u);
    CHECK(pool.get(child)->tickCount == 3u);
    CHECK(pool.get(grand)->tickCount == 2u);
}

// Tick order is a stable pre-order: roots in spawn order, parent before child,
// children in spawn order, identical on every pass.
static void testTickOrderStable() {
    resetRuns();
    TaskPool pool;
    // R1 -+- A -+- A1
    //     +- A2
    // R2
    int order[8];
    int orderN = 0;
    auto tagged = [&](int tag) {
        return [&, tag](Task&, TaskPool&) {
            if (orderN < 8) order[orderN] = tag;
            ++orderN;
        };
    };
    TaskId r1 = pool.spawn(tagged(0));
    TaskId r2 = pool.spawn(tagged(1));
    TaskId a = pool.spawn(tagged(2), r1);
    TaskId a1 = pool.spawn(tagged(3), a);
    TaskId a2 = pool.spawn(tagged(4), r1);
    (void)r2;
    (void)a1;
    (void)a2;

    const int expected[5] = {0, 2, 3, 4, 1};  // R1, A, A1, A2, R2
    for (int frame = 0; frame < 3; ++frame) {
        orderN = 0;
        pool.tick();
        CHECK(orderN == 5);
        for (int i = 0; i < 5; ++i) CHECK(order[i] == expected[i]);
    }
}

// The state-machine step pattern: a handler rewrites its own handler pointer
// to the next phase; self-kill runs the cleanup callback (+0x2c) once.
static void testStateMachineStepAndCleanup() {
    resetRuns();
    TaskPool pool;
    int cleanupRuns = 0;
    int phaseBRuns = 0;

    TaskId id = pool.spawn([&](Task& self, TaskPool&) {
        // phase A: first tick sets state=1 and hands the slot to phase B.
        CHECK(self.tickCount == 0u);
        self.state = 1;
        self.handler = [&](Task& s, TaskPool&) {
            ++phaseBRuns;
            if (s.tickCount == 1u) {
                s.state = 2;
                s.handler = [&](Task& t, TaskPool& r) {
                    // phase C: done after one run -> kill self + report.
                    CHECK(t.tickCount == 2u);
                    t.state = 3;
                    t.result = -1;
                    t.done = true;
                    CHECK(r.kill(id));
                };
            }
        };
    });
    pool.get(id)->cleanup = [&](Task&, TaskPool&) { ++cleanupRuns; };

    pool.tick();  // phase A
    CHECK(pool.get(id)->state == 1);
    CHECK(pool.get(id)->tickCount == 1u);
    pool.tick();  // phase B run 1
    CHECK(pool.get(id)->state == 2);
    CHECK(phaseBRuns == 1);
    CHECK(pool.get(id)->tickCount == 2u);
    pool.tick();  // phase C self-kills during the run
    CHECK(cleanupRuns == 1);
    CHECK(!pool.isLive(id));
    CHECK(pool.liveCount() == 0);

    pool.tick();
    pool.tick();
    CHECK(cleanupRuns == 1);   // cleanup runs exactly once
    CHECK(phaseBRuns == 1);    // dead handlers never run again
}

int main() {
    testSpawnTicksOncePerTick();
    testPoolCapacity();
    testKillRemovesTaskAndChildren();
    testTreeLinksStayValid();
    testChildSpawnsTickNextFrame();
    testTickOrderStable();
    testStateMachineStepAndCleanup();
    if (g_fail) { std::fprintf(stderr, "%d failure(s)\n", g_fail); return 1; }
    std::puts("task: all checks passed");
    return 0;
}
