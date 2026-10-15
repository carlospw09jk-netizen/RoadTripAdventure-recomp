# Collision & Terrain Queries

World mesh intersection tests (point, segment, quad), car-vs-car and car-vs-object collision detection with impact response, and arena wall bouncing.

| Address | Name | What it does | Callers | Callees |
| --- | --- | --- | --- | --- |
| `0x00206F28` | `IntersectPointWithMesh` | Tests a point against mesh triangles and records the nearest qualifying intersection. | `0x00207280`, `0x002084D0` | — |
| `0x00207280` | `FindWorldMeshIntersection` | Adjusts a world position across map sectors and tests candidate meshes for intersection. | `0x002080D8` | `0x00206F28`, `0x0023DE30`, `0x002729D0` |
| `0x002080D8` | `SelectMeshIntersectionRoutine` | Selects the mesh intersection function based on a mode flag. | `0x0020F950`, `0x00210158`, `0x002406F8`, `0x0025DBB8`, `0x0025E278`, `0x00263E68` | — |
| `0x002084D0` | `TestMeshIntersection` | Tests candidate triangles and updates the closest intersection result. | `0x002080D8` | `0x00206F28` |
| `0x002175D0` | `TestCarCollisionBox` | Transforms 4 collision check points of one car into another car's frame and tests against bounding volume extents. | `0x002179D8` | `0x00272958`, `0x00272728` |
| `0x00217830` | `HandleCarCollisionResponse` | Triggers collision rumble, collision sound effect, and impact recoil or interaction triggers between two cars. | `0x002179D8` | `0x0021C998`, `0x0021D310`, `0x00258E50`, `0x0020ACB8` |
| `0x002179D8` | `UpdateCarCarCollisionsTask` | Iterates over all car pairs, performs broadphase and narrowphase collision checks, and applies impact physics and reactions. | `0x0020F950`, `0x00210430` | `0x002175D0`, `0x00217830`, `0x0021D310` |
| `0x00219660` | `ApplyCollisionImpulse` | Maps collision code 0-15 to velocity/yaw impulses in car frame and repositions; code 15 = crash: prints, sets flag, rumble [inferred] | `0x0021BA70` | `0x0021D2D8`, `0x00258E50`, `0x0020ACB8`, `0x0027EA30` |
| `0x00219CB0` | `TestQuadLateralMask` | Per quad offsets verts by plane origin and rotates to car frame; returns bitmask 1/2/4/8 of lateral band when height/z tests pass [inferred] | `0x00219ED8` | `0x00272728` |
| `0x00219ED8` | `QueryCarSurfaces` | Rotates by car yaw, bitmask-tests the car's surface quad list via 0x219cb0; special type loops 26 extra parts [inferred] | `0x0021BA70` | `0x00219CB0`, `0x0023DE30`, `0x00272C38`, `0x00272A48` |
| `0x0021D0C0` | `TestSegmentInBoxWindow` | Clips a segment between two 3D pts to a +/-110 X window, tests clipped Z span against +/-110 of param_2; returns visibility [inferred] | `0x0021FA38` | — |
| `0x00240BE0` | `IsPointInsideQuad` | Returns whether the position at offset 0x90 lies inside a clockwise four-point 2D polygon. [inferred] | `0x00240CD8` | — |
| `0x00253B80` | `HandleCarObjectCollisions` | Checks collision between object and all player/NPC cars, applying bounce impulse and triggering rumble [inferred] | `0x00254430` | `0x0021D358`, `0x002727E8`, `0x00272940`, `0x00272958`, `0x00272970`, `0x00272988`, `0x00272A38`, `0x00274B10`, `0x0020ACB8` |
| `0x00253E90` | `ApplyArenaWallCollisions` | Tests object position against rectangular arena boundary walls, reflecting velocity with bounce damping [inferred] | `0x00254430` | — |

