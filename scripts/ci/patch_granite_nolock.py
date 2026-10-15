#!/usr/bin/env python3
"""CI-only patch: fix a self-deadlock in Granite's legacy (no-timeline) semaphore path.

On devices where timeline semaphores are not in use, emit_implicit_sync_to_queues()
acquires a fresh binary semaphore through request_legacy_semaphore() (inline in
device.hpp) -> Device::request_semaphore(), which takes the device lock via LOCK().
That code runs inside submit_queue, which the *_nolock submission paths call while
DRAIN_FRAME_LOCK already holds the same (non-recursive) std::mutex on this thread:
the lock is taken twice on one thread and the app deadlocks on its first frame
(black screen, 0% CPU, futex wait).

Fix: acquire from the semaphore manager directly (the device lock is already held
here), through the handle-based overload of request_semaphore which does not lock.

Applied to the checked-out Granite tree only; contains no game code or data.
"""

from pathlib import Path
import sys

OLD = "\t\t\tauto sem = request_legacy_semaphore();"
NEW = (
    "\t\t\t// This runs under the device lock (submit_queue from the *_nolock paths):\n"
    "\t\t\t// request_legacy_semaphore() would lock the same non-recursive std::mutex\n"
    "\t\t\t// again and self-deadlock on devices that do not use timeline semaphores.\n"
    "\t\t\t// Take the semaphore from the manager directly instead (CI patch).\n"
    "\t\t\tVkSemaphore legacy_vk_sem = managers.semaphore.request_cleared_semaphore();\n"
    "\t\t\tauto sem = request_semaphore(VK_SEMAPHORE_TYPE_BINARY_KHR, legacy_vk_sem, true);"
)
MARK = "request_semaphore(VK_SEMAPHORE_TYPE_BINARY_KHR, legacy_vk_sem, true);"

found = None
for root in (Path("third_party"), Path(".")):
    if not root.exists():
        continue
    hits = list(root.rglob("parallel-gs/Granite/vulkan/device.cpp"))
    if hits:
        found = hits[0]
        break

if found is None:
    sys.exit("patch_granite_nolock: parallel-gs/Granite/vulkan/device.cpp not found (bootstrap first?)")

text = found.read_text()
if MARK in text:
    print(f"already patched: {found}")
    sys.exit(0)

count = text.count(OLD)
if count != 1:
    sys.exit(f"patch_granite_nolock: expected exactly 1 match in {found}, found {count}")

found.write_text(text.replace(OLD, NEW))
print(f"patched: {found}")
