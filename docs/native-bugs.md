# Bug notes on the native / hardware GS path

One entry per issue: what, cause, fix, test. Issues are never closed from here.

## #3 Texture pack stops applying after leaving and re-entering a building

- Cause (from the code, not yet seen on a Mac run): `TextureTools::submit` refused a texture when
  its job queue held more than 512 entries (a burst at a scene load, or the resubmit of every
  texture after an eviction), but `GSHwBackend` had already put the texture's key in
  `m_submitted`, so it was never offered again until the next resubmit. Its pack image never
  arrived and the building drew without the pack.
- Fix: `submit` returns whether it took the job; the backend erases the key from `m_submitted`
  when it did not, so the next draw of that texture offers it again. `RT_TEXTURE_JOBS_MAX` lowers
  the queue limit (tests).
- Test: `test_texture_pack_queue_overflow` (hardware GS, limit 3).

## #4 River Raceway 2P road decal (in progress)
Not yet root-caused: see the notes at the end of this file when finished.

## #20 Windows Intel HD630 crash at the Conspiracy screen
The issue body is unrelated text; the owner's comment says the GPU lacks the Vulkan features the
renderer needs. Nothing reproducible on macOS.
