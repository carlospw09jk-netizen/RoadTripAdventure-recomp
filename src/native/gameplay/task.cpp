// Authored task pool implementation; see task.h and
// docs/game-mechanics/task-scheduler.md. Pure C++, no guest code.

#include "task.h"

#include <utility>

namespace rt::native::gameplay {

// ---------------------------------------------------------------- lookup

Task* TaskPool::get(TaskId id) {
    if (!id.valid() || id.index >= kCapacity) return nullptr;
    Task& t = slots_[id.index];
    if (!t.live || t.generation != id.generation) return nullptr;
    return &t;
}

const Task* TaskPool::get(TaskId id) const {
    return const_cast<TaskPool*>(this)->get(id);
}

bool TaskPool::isLive(TaskId id) const { return get(id) != nullptr; }

std::size_t TaskPool::liveCount() const {
    std::size_t n = 0;
    for (const Task& t : slots_)
        if (t.live) ++n;
    return n;
}

// ---------------------------------------------------------------- spawn

TaskId TaskPool::spawn(TaskFn handler, TaskId parent, void* arg) {
    // Linear scan for the first free slot, like gs_tasks_spawn.
    std::uint16_t index = kTaskIndexInvalid;
    for (std::uint16_t i = 0; i < kCapacity; ++i) {
        if (!slots_[i].live) {
            index = i;
            break;
        }
    }
    if (index == kTaskIndexInvalid) return kInvalidTask;  // pool exhausted

    if (parent.valid() && !get(parent)) return kInvalidTask;  // dead parent

    Task& t = slots_[index];
    const std::uint16_t generation = t.generation;
    t = Task{};                      // spawn clears the rest of the slot
    t.generation = generation;
    t.live = true;
    t.handler = std::move(handler);
    t.parent = parent;
    t.arg = arg;
    t.spawnTick = epoch_;

    const TaskId id{index, generation};

    // Link at the END of the parent's child chain (or of the root chain).
    TaskId head = rootHead_;
    if (parent.valid()) head = get(parent)->firstChild;
    if (!head.valid()) {
        if (parent.valid())
            get(parent)->firstChild = id;
        else
            rootHead_ = id;
    } else {
        TaskId c = head;
        while (true) {
            Task* ct = get(c);
            if (!ct) break;  // defensive: chain invariant should hold
            if (!ct->nextSibling.valid()) {
                ct->nextSibling = id;
                break;
            }
            c = ct->nextSibling;
        }
    }
    return id;
}

// ---------------------------------------------------------------- kill

bool TaskPool::kill(TaskId id) {
    if (!get(id)) return false;
    unlink(id);          // splice out first so re-entrant walks stay valid
    destroySubtree(id);  // cleanup callback + descendants, then free
    return true;
}

// DestroyTaskTree: run the cleanup callback (+0x2c), then recursively destroy
// the child chain (+0x10 / +0x14 within the subtree), then free the slot.
// The killed root's siblings are not part of its subtree and survive.
void TaskPool::destroySubtree(TaskId id) {
    Task* t = get(id);
    if (!t) return;

    if (t->cleanup) {
        // Move the callback out before invoking: a cleanup that kills its own
        // task (or reuses the slot) must not destroy the running callable.
        TaskFn cb = std::move(t->cleanup);
        t->cleanup = {};
        cb(*t, *this);
    }

    t = get(id);  // cleanup may have freed this slot already
    if (!t) return;

    // Snapshot the child chain: cleanups may have mutated the links.
    std::array<TaskId, kCapacity> kids{};
    std::size_t nk = 0;
    for (TaskId c = t->firstChild; c.valid() && nk < kCapacity;) {
        const Task* ct = get(c);
        if (!ct) break;
        kids[nk++] = c;
        c = ct->nextSibling;
    }
    t->firstChild = kInvalidTask;

    for (std::size_t i = 0; i < nk; ++i) destroySubtree(kids[i]);
    freeSlot(id.index);
}

// Splice the task out of its parent's child chain (or out of the root chain).
void TaskPool::unlink(TaskId id) {
    Task* t = get(id);
    if (!t) return;

    TaskId head = rootHead_;
    if (t->parent.valid()) {
        Task* p = get(t->parent);
        if (!p) return;
        head = p->firstChild;
    }

    TaskId prev = kInvalidTask;
    for (TaskId c = head; c.valid();) {
        Task* ct = get(c);
        if (!ct) break;
        if (c == id) {
            if (prev.valid()) {
                get(prev)->nextSibling = t->nextSibling;
            } else if (t->parent.valid()) {
                get(t->parent)->firstChild = t->nextSibling;
            } else {
                rootHead_ = t->nextSibling;
            }
            t->nextSibling = kInvalidTask;
            return;
        }
        prev = c;
        c = ct->nextSibling;
    }
}

void TaskPool::freeSlot(std::uint16_t index) {
    Task& t = slots_[index];
    if (!t.live) return;
    const std::uint16_t generation = static_cast<std::uint16_t>(t.generation + 1);
    t = Task{};  // clear the slot (the game zeroes *task and +0x28)
    t.generation = generation;
}

// ---------------------------------------------------------------- tick

void TaskPool::tick() {
    ++epoch_;  // tasks spawned during this pass carry the old epoch and skip
    walkChain(rootHead_);
}

void TaskPool::walkChain(TaskId head) {
    // Snapshot the sibling chain before ticking: handlers may spawn into it,
    // kill from it, or reorder it while the pass runs.
    std::array<TaskId, kCapacity> chain{};
    std::size_t n = 0;
    for (TaskId c = head; c.valid() && n < kCapacity;) {
        const Task* ct = get(c);
        if (!ct) break;
        chain[n++] = c;
        c = ct->nextSibling;
    }
    for (std::size_t i = 0; i < n; ++i) tickOne(chain[i]);
}

void TaskPool::tickOne(TaskId id) {
    Task* t = get(id);
    if (!t) return;
    if (t->spawnTick == epoch_) return;  // spawned this pass: first tick next frame

    // Snapshot the child chain BEFORE the handler runs, so a child spawned by
    // this handler first ticks on the next pass.
    std::array<TaskId, kCapacity> kids{};
    std::size_t nk = 0;
    for (TaskId c = t->firstChild; c.valid() && nk < kCapacity;) {
        const Task* ct = get(c);
        if (!ct) break;
        kids[nk++] = c;
        c = ct->nextSibling;
    }

    t->done = false;  // the walker clears the "done" flag every tick
    TaskFn fn = t->handler;  // copy: the handler may kill its own task
    if (fn) fn(*t, *this);

    t = get(id);  // the handler may have killed this task (or reused the slot)
    if (!t) return;
    ++t->tickCount;  // +0x8: exactly once per tick() for every live task

    for (std::size_t i = 0; i < nk; ++i) tickOne(kids[i]);
}

// ---------------------------------------------------------------- misc

void tick(TaskPool& pool) { pool.tick(); }

}  // namespace rt::native::gameplay
