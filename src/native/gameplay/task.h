#pragma once
// Authored task scheduler (pure C++, no PS2/guest code).
//
// Mirrors the co-routine "task" model documented in
// docs/game-mechanics/task-scheduler.md:
//   - a fixed pool of 48 slots (gs_tasks_pool),
//   - a parent / first-child / next-sibling tree per slot,
//   - spawn / kill / tick contracts (gs_tasks_spawn 0x204c48,
//     gs_tasks_kill 0x204de8, gs_tasks_walker 0x204f98, DestroyTaskTree
//     0x205018),
//   - the state-machine step pattern where a handler rewrites its own
//     function pointer (+0xc) to the next phase's handler.
//
// The Task fields mirror the guest slot layout (task* +0x0..+0x2c) as named
// data, not guest RAM offsets: nothing here reads or writes guest memory.

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>

namespace rt::native::gameplay {

struct Task;
class TaskPool;

inline constexpr std::uint16_t kTaskIndexInvalid = 0xffff;

// Opaque handle: pool index + slot generation, so stale handles are detected
// after a slot is freed (and possibly reused by a later spawn).
struct TaskId {
    std::uint16_t index = kTaskIndexInvalid;
    std::uint16_t generation = 0;

    [[nodiscard]] bool valid() const { return index != kTaskIndexInvalid; }
    friend bool operator==(TaskId, TaskId) = default;
};

inline constexpr TaskId kInvalidTask{};

// handler(self, pool): the frame-context parameter of the guest handler
// (frameCtx at 0x335900) is folded into the pool itself in this authored
// model. The cleanup callback (+0x2c) uses the same signature.
using TaskFn = std::function<void(Task&, TaskPool&)>;

// One pool slot. Field comments cite the guest offsets they mirror.
struct Task {
    bool live = false;           // +0x0  live byte (0 = free, 1 = live)
    std::uint16_t cursor = 0;    // +0x2  short opcode/cursor counter
    std::int32_t result = 0;     // +0x4  result passed back to the parent
    std::uint32_t tickCount = 0; // +0x8  per-frame counter (+1 per tick())
    TaskFn handler;              // +0xc  code *handler(self, frameCtx)
    TaskId firstChild;           // +0x10 first child slot
    TaskId nextSibling;          // +0x14 next sibling slot
    TaskId parent;               // +0x18 parent slot (invalid for roots)
    void* arg = nullptr;         // +0x1c initial argument (opaque)
    bool done = false;           // +0x24 "done" flag consumed by the parent
    std::uint8_t state = 0;      // +0x25 inner state phase (0..6 in the game)
    std::uint8_t subState = 0;   // +0x26 per-opcode scratch
    std::uint8_t step = 0;       // +0x27 step index (e.g. day-cycle 0..10)
    void* arg2 = nullptr;        // +0x28 second arg / opcode pointer
    TaskFn cleanup;              // +0x2c cleanup callback, run by kill

    // Implementation guards (not part of the guest layout): the tick pass this
    // task was spawned on (tasks spawned during a pass tick on the next pass),
    // and the slot generation mirrored into TaskId for stale-handle detection.
    std::uint64_t spawnTick = 0;
    std::uint16_t generation = 0;
};

class TaskPool {
public:
    static constexpr std::size_t kCapacity = 48;  // gs_tasks_pool: 48 slots

    // gs_tasks_spawn: allocate the first free slot, init it, and link it at
    // the end of the parent's child chain (the root chain when parent is
    // invalid). Returns kInvalidTask when the pool is exhausted (the game
    // fatal()s there) or when parent is a stale/dead handle.
    TaskId spawn(TaskFn handler, TaskId parent = kInvalidTask, void* arg = nullptr);

    // gs_tasks_kill: run the task's cleanup callback (+0x2c), tear down the
    // task's descendant subtree (DestroyTaskTree recursion), splice it out of
    // its sibling chain and free the slot. The task's siblings survive.
    // Returns false when id is not live. Safe to call from inside a handler
    // (including self-kill).
    bool kill(TaskId id);

    // gs_tasks_walker: one per-frame pass. Pre-order walk of the tree: every
    // live task's handler runs exactly once and its tick counter increments
    // exactly once per call; tasks spawned during the pass tick on the next
    // pass (per the spawn-then-poll state-machine pattern).
    void tick();

    [[nodiscard]] bool isLive(TaskId id) const;
    [[nodiscard]] std::size_t liveCount() const;
    [[nodiscard]] TaskId rootHead() const { return rootHead_; }

    Task* get(TaskId id);                // nullptr when stale/dead
    const Task* get(TaskId id) const;

private:
    void walkChain(TaskId head);         // snapshot the chain, tick each entry
    void tickOne(TaskId id);
    void destroySubtree(TaskId id);      // cleanup + free, top-down
    void unlink(TaskId id);              // splice out of parent/root chain
    void freeSlot(std::uint16_t index);

    std::array<Task, kCapacity> slots_{};
    TaskId rootHead_{};
    std::uint64_t epoch_ = 0;  // current tick pass
};

// Free-function form of TaskPool::tick (see the spawn/kill/tick contract).
void tick(TaskPool& pool);

}  // namespace rt::native::gameplay
