# Car AI & Waypoints

Waypoint-path navigation for AI cars: target angle computation, progress/lateral error tracking, steering and throttle output, route table setup, and the AI data save file.

| Address | Name | What it does | Callers | Callees |
| --- | --- | --- | --- | --- |
| `0x0020E5B8` | `SaveAIDataFile` | Formats 'AI Save %s' path, opens file, writes non-zero-trimmed buffer in byte-sized sceWrite calls, closes. | `0x0026D020` | `0x0027EEE0`, `0x0027EA30`, `0x00289148`, `0x00288EC0`, `0x00289770` |
| `0x0024FCD8` | `CarAiInit` | Initializes car AI state, allocating 0x100-byte AI buffer and binding course waypoint path tables [inferred] | `0x00218458` | `0x0025FBC0`, `0x0027E840`, `0x0027EA30` |
| `0x0024FED8` | `ComputeWaypointTargetAngle` | Computes heading angles from vehicle position to waypoint segment endpoints via atan2 and packs results [inferred] | `0x002508E0` | `0x00274AF8` |
| `0x00250068` | `UpdateVehicleWaypointProgress` | Tracks vehicle position against course waypoint segments, updating current waypoint index and lateral error [inferred] | `0x0021A990` | `0x00203A50`, `0x00274938`, `0x00274A10`, `0x00274AF8`, `0x0027EEE0` |
| `0x00250738` | `DrawWaypointDebugGrid` | Renders on-screen debug text grid for nearby AI waypoint segments and node attributes [inferred] | `0x0026D020` | `0x00203A50`, `0x0027EEE0` |
| `0x002508E0` | `CalculateAiSteeringAndThrottle` | Calculates AI steering angle and throttle response based on target waypoint heading and current vehicle speed [inferred] | `0x0020E708` | `0x0024FED8` |
| `0x00250DD0` | `CopyAiRouteTables` | Copies 16 default AI route/path tables (0x100 bytes each) from DAT_002a9b80 into destination buffer [inferred] | `0x002277B0`, `0x0023DA58` | — |
| `0x00253230` | `SetAiSteeringTarget` | Computes and sets AI vehicle steering input flags based on angle delta to target and speed limit [inferred] | `0x00253448`, `0x00253680` | `0x0021D358` |
| `0x00253448` | `SteerAiToDefaultWaypoint` | Steers AI vehicle toward default waypoint target coordinates when navigating course sections [inferred] | `0x00253680` | `0x00253230`, `0x002727E8`, `0x00272940`, `0x00272958`, `0x00272970`, `0x00272988`, `0x00274AF8`, `0x00274B10` |
| `0x00253680` | `UpdateAiCourseSteering` | Directs AI vehicle navigation across course waypoints, choosing between target node and default path [inferred] | `0x00254CC8` | `0x00253230`, `0x00253448`, `0x00272958`, `0x00272970`, `0x00274AF8`, `0x00274B10` |

