# Native Asset Fixture — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Produce a typed, versioned `HostAssetContainer` (shared neutral schema), an authored offline converter (disc model+texture → container), and a shipped loader that consumes the container with **no** PS2Recomp/parallel-gs/EE/VU1/GS dependency, verified against independently-derived expected data for one pinned disc asset.

**Architecture:** A standalone authored asset pipeline: a shared neutral schema (`src/native/assets/container.h`, no guest types) is consumed by (a) an offline converter under `tools/native-assets/` (tool targets with their own `main()`, never a dependency of the runtime) and (b) runtime loaders under `src/native/assets/` (a library, no `main()`). Direct-GPU preview is the *next* task, gated on this one.

**Tech Stack:** C++20 (`CMAKE_CXX_STANDARD 20` in `CMakeLists.txt:11`; `std::span` available). SDL3/Vulkan/MoltenVK are permitted but unused by these pure-C++ tasks.

**Spec:** `docs/superpowers/specs/2026-10-14-fully-native-roadtrip-design.md` (direction APPROVED 2026-10-14; scope approval only).

## Global Constraints

- No PS2Recomp/parallel-gs/EE/VU1/GS execution or disguised vendoring in shipped targets; Vulkan/MoltenVK/SDL3 permitted (spec §0, §17).
- **One-way dependency boundary**: offline converter → shared schema (allowed); runtime loader → shared schema (allowed); **runtime → converter / GS / PS2 / rdram (forbidden)**. The shared schema carries no guest/PS2/GS types. (Fixes prior "converter must not include schema" error.)
- Converter (`tools/native-assets/`) is a **separate offline tool**, never a link dependency of any runtime target.
- Shipped loaders consume **only** `HostAssetContainer` records — no disc/PS2 byte inputs, no `Tex0`/swizzle/palette/GIF/GS types (spec §10). Palette identity is carried as a generic host `variantId`, never CSA/TEX0.
- Never commit game data; authored hand-made fixtures only; no proprietary bytes in tracked `src/`/`tests/` (CLAUDE.md line 3-5).
- Dirty `CMakeLists.txt` and dirty `third_party/PS2Recomp` submodule are **not** overwritten/deleted; native-asset targets use a **new standalone CMakeLists**, not the parent (spec §11).
- `nm | grep sub_00` is not a proof; use dependency-edge/forbidden-reference inspection (spec §13 G2).
- **No build/test/game/ADB/heavy-Ghidra/REA-import authorized now.** Every "Run" step is future acceptance, **BLOCKED** pending a separately authorized environment. Test *authoring* is planned; red/green execution is not claimed.
- One local implementer at a time; never concurrent with Android/macOS builds or heavy Ghidra; host `ctest`/`regress.py`/macOS game runs prohibited. **No commits until explicit user permission.**

## Review Focus

1. **Truncated/short container** — magic/version mismatch or a length exceeding the input span must return `Truncated`/`BadMagic`/`BadVersion`, never OOB. → Task 1 `test_truncated_rejected`.
2. **Relative-pointer/count overflow** — a section-relative pointer or count whose `base + value` overflows `uint64`/`uint32` must return `OffsetOverflow`/`CountOverflow` (REJECT, not clamp). → Task 2 `test_pointer_overflow_rejected`, `test_count_overflow_rejected`.
3. **Palette provenance** — a paletted texture with multiple CLUT banks produces one `ImageRecord` per palette instance, each carrying its `sourceVariantId` (generic host metadata, no CSA/TEX0). → Task 3 `test_variant_id_per_palette_bank`.
4. **Endianness** — u32 fields are read little-endian per disc byte order. → Task 2 `test_u32_little_endian`.
5. **Provenance integrity** — `hasValidProvenance` is false when `sourceFile` empty, `length == 0`, or `sha256` all-zero (absence sentinel — a design choice, **not** cryptographic proof); authenticated identity vs the real disc is established only in Task 0. → Task 1 `test_missing_provenance_rejected`.

## File Structure

- Create `src/native/assets/container.h` — shared neutral schema (`HostAssetContainer`, `Provenance`, records, `ContainerError`).
- Create `src/native/assets/container.cpp` — `serialize`/`deserialize` (minimal bounded LE wire format; bounds-checked).
- Create `src/native/assets/{native_model.h,native_image.h}` — `NativeModel`/`NativeImage`.
- Create `src/native/assets/{model_loader.{h,cpp},image_loader.{h,cpp}}` — consume records (library, no `main()`).
- Create `tools/native-assets/{model_converter.{h,cpp},image_converter.{h,cpp}}` — offline decode.
- Create `tools/native-assets/converter_main.cpp`, `tools/native-assets/inspect_main.cpp` — tool entrypoints (`main()`).
- Create `src/native/CMakeLists.txt` — standalone build (new file; does not touch parent).
- Test `tests/native_assets/{test_container,test_model_converter,test_image_converter,test_loader}.cpp`.

## Interfaces (exact; all `namespace rt::native::assets`)

```cpp
// container.h
struct Provenance {
    std::string sourceFile;              // empty => missing
    uint64_t offset = 0;
    uint64_t length = 0;                 // 0 => missing
    std::array<uint8_t, 32> sha256{};    // all-zero => absence sentinel (not crypto proof)
};
enum class PixelFormat : uint8_t { R8G8B8A8_UNORM = 0 };
struct ImageRecord {
    uint32_t width = 0, height = 0, mipCount = 0;
    PixelFormat format = PixelFormat::R8G8B8A8_UNORM;
    std::vector<uint8_t> pixels;         // size validated on deserialize/load
    std::string sourceVariantId;         // palette-instance identity (generic host metadata)
    Provenance provenance;
};
struct Vertex { float px,py,pz, nx,ny,nz, u,v; uint8_t r,g,b,a; };
struct MaterialRef { uint64_t firstIndex = 0; uint64_t indexCount = 0; uint32_t imageIndex = 0xFFFFFFFFu; }; // 0xFFFFFFFF => unresolved
struct MeshRecord { std::vector<Vertex> vertices; std::vector<uint32_t> indices; std::vector<MaterialRef> materials; Provenance provenance; };
struct HostAssetContainer { uint32_t version = 0; std::vector<ImageRecord> images; std::vector<MeshRecord> meshes; };
enum class ContainerError : uint8_t {
    Ok = 0, BadMagic, BadVersion, Truncated, OffsetOverflow, CountOverflow,
    IndexOutOfBounds, MaterialImageOutOfRange, NonFiniteFloat, BadImageSize, MissingProvenance,
};
struct ContainerResult { ContainerError err = ContainerError::Ok; HostAssetContainer value; };

// container.cpp
std::vector<uint8_t> serialize(const HostAssetContainer&);        // LE: u32 magic 'RTAC', u32 version, length-prefixed records
ContainerResult deserialize(std::span<const uint8_t> bytes);      // bounds-checked; no OOB, no wrap
bool hasValidProvenance(const Provenance&);

// tools/native-assets/model_converter.h
HostAssetContainer convertModel(const std::filesystem::path& discFile, const Provenance& src);
// tools/native-assets/image_converter.h
ImageRecord convertImage(std::span<const uint8_t> uploadChain, const Provenance& src, const std::string& variantId);

// src/native/assets/native_model.h / native_image.h
struct NativeModel { std::vector<Vertex> vertices; std::vector<uint32_t> indices; std::vector<uint32_t> materialImage; };
struct NativeImage { uint32_t width, height, mipCount; std::vector<uint8_t> pixels; std::string variantId; };
// src/native/assets/model_loader.h / image_loader.h
NativeModel loadModel(const MeshRecord&, std::span<const ImageRecord> images);  // validates indices + material image refs
NativeImage loadImage(const ImageRecord&);                                     // validates pixel size vs width/height/mips
```

## Tasks

### Task 0: Pin fixture + extract format evidence (cloud, read-only, no local model)

**Files:** evidence under `build/scratch/decomp-audit/m1-fixture/` (untracked).

**Interfaces:** Produces `fixture.tsv` (pinned path + SHA-256 + byte offsets), `expected.tsv`, `format-notes.md`.

- [ ] **Step 1:** Pin the **real** retained file `~/Library/Application Support/RoadTripRecomp/disc/CAR0/Q00.BIN` (verified present) as the mesh+texture fixture; record `sha256sum`, size. Note `disc/FLD/000.BIN` as an alternative. Static byte reads only — no extraction-tool binary execution.
- [ ] **Step 2:** Independently derive expected geometry (vertex/index counts, topology, UVs) and texture samples from raw bytes, or from named-provenance retained outputs (`tools/model_dump.cpp` OBJ/JSON, `tools/texture_extract.cpp` PNG+`sources.csv`). Never let the converter under test generate them.
- [ ] **Step 3:** Record `0x20` header / "6 model pointers" / section-offset layout as **hypotheses to verify against actual bytes**, and any unanswered format portion as `BLOCKED/UNKNOWN` — no invented offsets.
- [ ] **Gate:** `fixture.tsv` hashes pinned; `expected.tsv` is manual or named-provenance-independent. This is the acceptance data for Tasks 2–4. (Conversion-delivery location does **not** block this fixture — the converter is offline by construction.)

### Task 1: Shared neutral schema + serialization (model: 1a small cloud pool / 1b Qwen 27B)

**Files:** Create `src/native/assets/container.h`, `container.cpp`. Test `tests/native_assets/test_container.cpp`.

- [ ] **Step 1:** Write tests — `test_roundtrip` (hand-made container, no game bytes, serialize→deserialize equal); `test_bad_magic_rejected` (`BadMagic`), `test_bad_version_rejected` (`BadVersion`), `test_truncated_rejected` (`Truncated`), `test_pointer_overflow_rejected` (`OffsetOverflow`), `test_count_overflow_rejected` (`CountOverflow`), `test_nonfinite_float_rejected` (`NonFiniteFloat`), `test_bad_image_size_rejected` (`BadImageSize`), `test_missing_provenance_rejected` (`MissingProvenance` when `sourceFile` empty OR `length==0` OR `sha256` all-zero).
- [ ] **Step 2:** (BLOCKED — future authorized run) Expected FAIL.
- [ ] **Step 3:** Implement `container.h` structs (as in Interfaces) + `container.cpp` `serialize`/`deserialize`/`hasValidProvenance`. Minimal bounded LE layout: u32 magic `'RTAC'`, u32 version, then length-prefixed record blobs; every length checked against the input span; no wrap arithmetic.
- [ ] **Step 4:** (BLOCKED) Expected PASS.
- [ ] **Step 5:** Commit deferred — no commits until explicit user permission.

### Task 2: Model converter (mesh-only, independently testable) (model: Qwen 27B)

**Files:** Create `tools/native-assets/model_converter.{h,cpp}`. Test `tests/native_assets/test_model_converter.cpp`.

**Interfaces:** Consumes `HostAssetContainer`, `Provenance` (Task 1). Produces `convertModel(...)` returning a container with **only** `MeshRecord`s; each `MaterialRef.imageIndex = 0xFFFFFFFFu` (unresolved placeholder — image binding is Task 3's job).

- [ ] **Step 1:** Write tests using authored hand-made buffers — `test_truncated_header_rejected`, `test_section_offset_beyond_eof_rejected`, `test_pointer_overflow_rejected`, `test_count_overflow_rejected`, `test_u32_little_endian`. No disc bytes in tracked repo.
- [ ] **Step 2:** (BLOCKED) Expected FAIL.
- [ ] **Step 3:** Implement the CAR container parser per `docs/native-render.md:163` (6 relative model pointers, u32 model data size, then texture chain region) — but treat the exact header/pointer layout as **T0-verified**, not universal. Emit `MeshRecord`s for the model chains; image refs left unresolved.
- [ ] **Step 4:** (BLOCKED) Expected PASS; converter mesh output matches `expected.tsv` counts (Task 0).
- [ ] **Step 5:** Commit deferred.

### Task 3: Image converter (offline texture decode) (model: Qwen 27B)

**Files:** Create `tools/native-assets/image_converter.{h,cpp}`. Test `tests/native_assets/test_image_converter.cpp`.

**Interfaces:** Consumes container texture-chain bytes + `docs/native-textures.md` rules. Produces `convertImage(uploadChain, src, variantId) -> ImageRecord`; paletted chains emit one record per CLUT bank with a distinct `sourceVariantId`.

- [ ] **Step 1:** Write tests — `test_variant_id_per_palette_bank` (two banks → two records, distinct `variantId`), `test_pixels_match_expected` (vs `expected.tsv`), `test_mip_pixel_size_rejected` (`BadImageSize` on a mip whose pixel blob is the wrong size).
- [ ] **Step 2:** (BLOCKED) Expected FAIL.
- [ ] **Step 3:** Implement TEX0/CLUT/palette-bank/swizzle decode **offline** per `docs/native-textures.md` (reference `tools/texture_extract.cpp` for decode math only, not runtime reuse). No `ps2_gs_memory.h`.
- [ ] **Step 4:** (BLOCKED) Expected PASS.
- [ ] **Step 5:** Commit deferred.

### Task 4: Shipped loaders (library, no main) (model: small cloud pool)

**Files:** Create `src/native/assets/{native_model.h,native_image.h,model_loader.{h,cpp},image_loader.{h,cpp}}`. Test `tests/native_assets/test_loader.cpp`.

**Interfaces:** `loadModel(const MeshRecord&, std::span<const ImageRecord> images) -> NativeModel` (validates `indices` in-bounds and each `material.imageIndex < images.size()`, resolving to `materialImage`); `loadImage(const ImageRecord&) -> NativeImage` (validates pixel blob size vs `width`/`height`/`mipCount`). **No version check** (that is `deserialize`'s job); **no reserialization**.

- [ ] **Step 1:** Write tests — `test_index_out_of_bounds_rejected` (`IndexOutOfBounds`), `test_material_image_out_of_range_rejected` (`MaterialImageOutOfRange`), `test_image_pixel_size_validated` (`BadImageSize`), `test_roundtrip_shape` (a valid record → `NativeModel`/`NativeImage` with matching counts, no byte changes).
- [ ] **Step 2:** (BLOCKED) Expected FAIL.
- [ ] **Step 3:** Implement loaders as in Interfaces. No `#include` of `tools/native-assets/*`, `runtime/gs/*`, `ps2*`, `ps2x::`, `rdram`, `Tex0`.
- [ ] **Step 4:** (BLOCKED) Expected PASS.
- [ ] **Step 5:** Commit deferred.

### Task 5 (NOT implemented here): Direct-GPU preview — gated next

Depends on Tasks 1–4 passing + a separately authorized Vulkan/MoltenVK build/run. No steps until Task 4 evidence is complete (spec §16).

### Task 6: Standalone CMake + read-only assurance (model: small cloud pool — architecture, cloud-reviewed)

**Files:** Create `src/native/CMakeLists.txt` (standalone; does **not** modify the dirty parent `CMakeLists.txt`). Add tool entrypoints `tools/native-assets/converter_main.cpp` (`int main(int,char**)` → `convertModel`) and `inspect_main.cpp` (`int main(int,char**)` → load container, print summary).

**Interfaces:** Targets: `rt_native_assets` (static lib: container + loaders; no GS/PS2/disc); `rt-asset-convert` (exe: converter + main); `rt-asset-inspect` (exe: loader + main). All pure C++20; no Vulkan/SDL3 yet.

- [ ] **Step 1:** Record read-only preservation manifest (top-level + nested submodule `git status --porcelain`, diffs, checksums) before any build-file creation.
- [ ] **Step 2:** Add the standalone CMake targets with the one-way edges: `rt-asset-convert` → `rt_native_assets` (allowed, schema shared); `rt_native_assets` and `rt-asset-inspect` → no `tools/native-assets` converter, no GS/PS2.
- [ ] **Step 3 (read-only assurance, future run — not claimed):** `grep -nE 'runtime/gs|ps2_gs_memory|ps2x::|rdram|Tex0' src/native/assets/*` → empty; `grep -nE '#include "src/native/assets/(model|image)_loader' tools/native-assets/*` → empty (converter includes only the shared `container.h`); build-graph: `rt_native_assets` links no banned target. `nm`-style `sub_00` grep is **not** used as proof.

## Model allocation & safety (bind)

- **Local workhorse — Qwen 27B** (larger/complex tasks: Task 1b serialization, Tasks 2/3 converters — binary parser, GS-upload/palette/swizzle decode), directed by Sol/DeepSeek. One local inference worker maximum.
- **Small cloud pool — non-DeepSeek OpenCode Go**: `opencode-go/gpt-6-luna`, `opencode-go/mimo-v2.6-flash`, `opencode-go/glm-5.3-flash`, `opencode-go/qwen3.8-flash` — small coding tasks (schema types, synthetic tests, loaders, CMake glue) with **disjoint file ownership** so independent tasks run in parallel. Rotate among **task-suitable** pool members (price/quota evidence + quality tracking), never blind-random on critical work. **No Gemma e2b/e4b, no Argon-loops model.**
- **Reviewer — Haiku** (cloud), fresh reviewer per task. Cloud DeepSeek = coordinator; **parent arbitrates interface changes** (the shared brief is immutable for in-flight workers).
- **Safety**: one **local** inference worker max, never concurrent with host build/test/game/heavy Ghidra. **Cloud small parallel tool calls MUST NOT launch builds** (otherwise unsafe). Host `ctest`/`regress.py`/macOS game runs prohibited; **no host test/build permission currently**. Host build phase separately scheduled, local model unloaded, resource-control verified; exclusivity vs unfinished Thor dev-owner work confirmed before any local worker.
- **Quota**: model-catalog pricing is **not** quota remaining. `scripts/agent_usage_guard.py` is an infinite **Claude**-usage poller, **not** an OpenCode Go quota checker, and has no `--check` — do **not** run it or claim usage verified from it. Record quota as **unavailable** unless actual API evidence.
- No commits until explicit user permission; never generated/build/proprietary data.

## Self-review note

Scope = container + converter + loader + fixture only. Interfaces are exact and type-consistent (`ContainerError`/`ContainerResult`, `std::span<const ImageRecord>`, `Provenance`/`hasValidProvenance`, `variantId` palette identity); `deserialize(std::span<const uint8_t>) -> ContainerResult` fixes the invalid signature; version/magic/layout is an explicit LE minimal format with bounds checks; `NativeModel`/`NativeImage` shapes and index/material-reference validation are defined without requiring container internals. Fixture source is a real verified path; `0x20`/pointer counts are T0-gated hypotheses. All runs BLOCKED; no builds.
