# Math, Vectors & Number Formats

VU0-assisted matrix and vector transforms, fixed-point world-position decomposition and grid-cell helpers, bit counting / bit indexing, and packed-BCD digit formatting.

Entries marked *(unresolved)* carry a placeholder `UNKNOWN_` symbol; their behaviour below comes from decompilation notes only.

| Address | Name | What it does | Callers | Callees |
| --- | --- | --- | --- | --- |
| `0x00207B20` | `CountSetBits` | Returns the population count of a 32-bit value. | `0x0023C150`, `0x0023CB68`, `0x00246250`, `0x002455C0` | — |
| `0x00207EC8` | `TransformRectangle` | Builds rectangle bounds and applies a matrix/vector transform. | `0x0021F340`, `0x00259A08`, `0x0025D7C8`, `0x0026ABD0` | `0x00272988`, `0x00272A28` |
| `0x00207F38` | `WrapCoordinate` | Wraps a floating-point coordinate across configured lower and upper bounds. | `0x0022A408` | — |
| `0x00207F88` | `BuildTransformMatrix` | Constructs a transform matrix from basis vectors using VU matrix operations. | `0x00223B30`, `0x00251B60`, `0x00254430`, `0x002609D0` | `0x002727A0`, `0x002727E8`, `0x002729D0`, `0x00272A48` |
| `0x00208008` | `BuildYawTransform` | Builds a matrix from a direction vector and copies a translation vector. | `0x0021B3D0`, `0x0021BA70`, `0x0021DC18`, `0x00251B60` | `0x002727A0`, `0x002727E8`, `0x002729D0`, `0x00272A48` |
| `0x00208080` | `ApplyMatrixRotation` | Builds a rotation matrix and multiplies it into the supplied matrix. | `0x0021B3D0`, `0x0021DC18` | `0x00272C38`, `0x00272758`, `0x00272A48` |
| `0x002081D0` | `FindSetBitRank` | Returns the zero-based rank of a set bit, or -1 if the requested bit is clear. | `0x002455C0` | — |
| `0x00208228` | `FindNthSetBit` | Returns the bit index of the requested set bit, or -1 when absent. | `0x0022CA28` | — |
| `0x002085B0` | `ReflectVectorVU0` | VU0 vector math: reflects input vector about normal (param3) with scale param2; uses FUN_00272* helpers [inferred]. | — | `0x002727C0`, `0x002727E8`, `0x002728D8`, `0x00272958`, `0x00272988`, `0x002732B8`, `0x00274B10`, `0x002729D0` |
| `0x0021C6A0` | `DecomposeWorldPosition` | Normalizes packed fixed-point world pos into in-cell range, writes carry/base to out ptrs, returns grid sector index [inferred] | `0x002105B8`, `0x0026CA48` | — |
| `0x0021C728` | `ToRelativeVec4` | Builds float vec4(x,y,z,1) from fixed-point position minus reference origin, wraps at 27 bits, scales by world-unit constant DAT_00332c1c | `0x00222B70`, `0x002228A8` | — |
| `0x0021C7F0` | `FixedPosToFloatVec4` | Converts packed fixed-point position struct (x, y, z+carry bit) to float vec4 with w=1.0 using scale DAT_00332c20 | `0x0021E690`, `0x0021EC58` | — |
| `0x0021C880` | `GetGridCellIndex` | Returns 2-bit grid cell selector (0-3) from sign bits of fixed-point x and z fields of a position struct | `0x0021E690`, `0x00220938` | — |
| `0x0021C8D0` | `ComputeGridCellIndex` | Same grid cell index as 0x21c6a0 computed directly from raw fixed-point x and z values [inferred] | `0x002201C0`, `0x0026D700` | — |
| `0x0021C928` | `StoreIndexedPosition` | Writes scaled int3, float3 and a 16-bit tag into slot (stride 392B) of global race-object position table indexed by param_4 [inferred] | `0x00250F00`, `0x00254430`, `0x00254D20`, `0x002595F0`, `0x0025A120`, `0x0025EB70`, `0x0025FF00`, `0x00260638`, `0x00260B48`, `0x00260DC0`, `0x00262678`, `0x002631A0`, `0x00263710`, `0x002638C0`, `0x00263C38`, `0x00263E68`, `0x002642B8` | — |
| `0x0021D2D8` | `TransformVecByF15Matrix` | VU0: transforms f15 fixed-point vector by 4x4 f15 matrix rows 0-2 (no translation), writes fixed-point result [inferred] | `0x00219660`, `0x0021BA70`, `0x0021A310` | — |
| `0x0021D310` | `VecSubtractAndLength` | Subtracts two f15 fixed-point vectors and stores 3D Euclidean length of the difference in result vector's w component [inferred] | `0x00217830`, `0x002179D8` | — |
| `0x0021D358` | `GetSpeedFromF15Vector` | Converts f15 fixed-point 3D vector to float and returns its length scaled by 216.0 (car speed in display units) [inferred] | `0x0021BA70`, `0x002249B8`, `0x00224CD0`, `0x002406F8`, `0x00241F68`, `0x00249458`, `0x002494E8`, `0x00253230`, `0x00253B80`, `0x00259A08`, `0x002631A0`, `0x00263E68` | — |
| `0x0021D388` | `UNKNOWN_0021D388` *(unresolved)* | VU0 stub: loads 4 quads from matrix ptr and re-stores via vsqi at doubled VI0; semantics unclear, likely VU register sync [inferred] | `0x0022AEB0` | — |
| `0x0021D3B8` | `UNKNOWN_0021D3B8` *(unresolved)* | VU0 stub: loads 3 matrix rows and stores via vsqi at address ctc2(4); returns 4; semantics unclear [inferred] | `0x00226298` | — |
| `0x0021D3E0` | `UNKNOWN_0021D3E0` *(unresolved)* | VU0 stub: loads matrix rows then vlqi/qmtc2/ctc2 sequence with no memory output; unclear [inferred] | `0x0022AEB0` | — |
| `0x0021D450` | `UNKNOWN_0021D450` *(unresolved)* | 12-byte stub: single lqc2 quad load to $0, result unused; likely prefetch/sync artifact [inferred] | `0x0022AEB0` | — |
| `0x0024F3B8` | `CountSetBitsInByteArray` | Computes population count (total set bits) across an array of param_2 bytes [inferred] | `0x002161E0`, `0x00267F48`, `0x002681A8`, `0x00269BD0`, `0x00269C80` | — |
| `0x0024F4A0` | `IntToPackedBCD` | Converts an integer to packed BCD format with each decimal digit placed into a 4-bit nibble [inferred] | `0x00230520`, `0x00231208`, `0x002318B0`, `0x00231C90`, `0x00231F28`, `0x002494E8`, `0x002660F8` | — |
| `0x0024F4E0` | `CountDecimalDigits` | Returns the number of decimal digits required to represent positive integer param_1 [inferred] | `0x00231208`, `0x002318B0`, `0x00231C90`, `0x00231F28`, `0x00261638`, `0x002660F8`, `0x00268428`, `0x00268890` | — |
| `0x0024FC30` | `LoadAndTransformVec4` | Loads 4 shorts into a stack vector, transforms via FUN_00272a28 and FUN_00272988 with global matrix. | — | `0x00272988`, `0x00272A28` |
| `0x0024FC98` | `ScaleShortByMatrix` | Loads one short into a vector, transforms via FUN_00272a28 and FUN_00272988, returns first component. | — | `0x00272988`, `0x00272A28` |
| `0x00252540` | `NormalizeQuatAndRotate` | VU0 quaternion helper: normalizes param (default identity), computes projection and rotation [inferred]. | — | `0x002727C0`, `0x002727E8`, `0x00272958`, `0x002732B8` |
| `0x00254290` | `RotateObjectByWrappedYaw` | Normalizes an angle float into a [min,max] range, builds a vu0 rotation matrix, and applies it to an object transform and its child, normalizing both. [inferred] | `0x00254430` | `0x002727E8`, `0x00272CE0`, `0x0027E840`, `0x00272728`, `0x00272A48` |

