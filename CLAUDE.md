# FoxEngine — Claude Working Document

## Project Goal

Build a complete DirectX 11 engine from scratch on Windows, progressing milestone by milestone until the engine reaches meaningful feature parity with modern mid-tier engines (think Godot 4 / early Unity feature set). This covers rendering, physics, audio, input, animation, scene management, and eventually a standalone GUI editor.

Each milestone is implemented, verified in the test scene, committed, then we move on.

---

## Technology Stack

| System | Choice | Notes |
|---|---|---|
| Graphics API | DirectX 11 | d3d11, dxgi, d3dcompiler |
| Platform | Win32 API | No SDL, no GLFW |
| Language | C++17 | MSVC (VS 2022 toolchain) |
| Build | CMake 3.20+ | Generates VS solution |
| Shaders | HLSL | D3DCompileFromFile at runtime; fxc offline later |
| Math | DirectXMath | Ships with Windows SDK, header-only |
| Physics | Custom first, then integrate if needed | Start with broadphase + AABB/sphere/OBB narrowphase |
| Audio | XAudio2 | Ships with Windows SDK; no dep needed |
| Input | Win32 raw input | Keyboard, mouse; XInput for gamepad |
| Animation | Custom skeletal | Bone hierarchy + blend tree |
| Scripting | Lua (sol2 binding) | Deferred until engine is stable |
| Editor GUI | ImGui first → custom later | In-process debug UI now; standalone editor later |

Third-party libraries are added only when they solve a well-scoped, narrow problem. Pulled in via CMake FetchContent or placed under `ThirdParty/`. Never copy-paste library source inline.

---

## Repository Layout

```
FoxEngine/
├── CMakeLists.txt              # root — ties all targets together
├── CLAUDE.md                   # this file
│
├── Engine/                     # Core engine — compiled as a static library (FoxEngine.lib)
│   ├── CMakeLists.txt
│   ├── Shaders/                # Engine-internal HLSL
│   ├── include/Engine/         # Public headers (SE:: namespace)
│   │   ├── Core/               # Platform, logging, memory, time
│   │   ├── Renderer/           # Graphics pipeline, shaders, mesh, material
│   │   ├── Physics/            # Collision, rigidbody, dynamics
│   │   ├── Audio/              # Sound engine (XAudio2 wrapper)
│   │   ├── Input/              # Keyboard, mouse, gamepad abstraction
│   │   ├── Animation/          # Skeleton, clips, state machine, blending
│   │   ├── Scene/              # Entity, component, scene graph, camera
│   │   └── Assets/             # Asset manager, resource cache
│   └── src/                    # Implementation files (mirrors include layout)
│
├── Game/                       # Test executable — always the live integration scene
│   ├── CMakeLists.txt
│   └── src/
│
├── Editor/                     # Standalone editor (added when engine is stable enough)
│   ├── CMakeLists.txt
│   └── src/
│
├── Assets/                     # User-facing runtime assets (textures, models, audio, etc.)
│   ├── Textures/
│   ├── Models/
│   ├── Audio/
│   └── Animations/
│
├── Tools/                      # Offline build-time tools (asset packers, converters)
│
└── ThirdParty/                 # External deps (FetchContent or vendored)
```

**Strict boundary rules:**
- Engine code never includes Game or Editor headers.
- Game/Editor include only `<Engine/...>` public headers.
- Editor is a separate CMake executable target that links FoxEngine.lib.
- Each Engine subsystem (Renderer, Physics, Audio…) is a logical module inside the lib; they may call Core but avoid coupling to each other except through well-defined interfaces.

---

## Coding Conventions

- **Namespace**: All engine code in `namespace SE {}`. Game is global or `namespace Game {}`. Editor is `namespace Ed {}`.
- **Public headers**: `Engine/include/Engine/<Subsystem>/Name.h`
- **Private impl**: `Engine/src/<Subsystem>/Name.cpp`
- **Error handling**: HRESULT checked with `SE_HR(expr)` — logs file/line and `__debugbreak()` in debug. No exceptions anywhere.
- **COM pointers**: `Microsoft::WRL::ComPtr<T>` always. Raw `Release()` is forbidden.
- **Naming**:
  - Types, public functions: `PascalCase`
  - Local variables, private functions: `camelCase`
  - Member variables: `m_` prefix
  - Globals (avoid): `g_` prefix
  - Constants / enum values: `k_` prefix or `SCREAMING_SNAKE` for C-style macros
- **No engine-level singletons** — subsystems are owned by a top-level `Engine` class, passed by reference where needed.
- **Comments**: Only when the *why* is non-obvious. No narration, no boilerplate.
- **HLSL**: One `.hlsl` file per shader program. `VS_Main` / `PS_Main` / `CS_Main` entry points.
- **Memory**: Use standard allocators for now. A custom allocator subsystem is a late-stage milestone.

---

## Full Milestone Roadmap

Milestones are numbered sequentially. The prefix letter groups them by system but the number is the canonical ID and commit label.

### Phase 1 — Foundation & Platform

| # | Milestone |
|---|---|
| M01 | ~~Repo + CMake scaffold, CLAUDE.md~~ ✓ |
| M02 | ~~Win32 window + message loop; clean shutdown~~ ✓ |
| M03 | ~~Logging system (`SE_LOG`, severity levels, file+console sink)~~ ✓ |
| M04 | ~~Timer / clock (`SE::Clock` — frame delta, elapsed, fixed timestep)~~ ✓ |

### Phase 2 — Renderer Bootstrap

| # | Milestone |
|---|---|
| M05 | ~~D3D11 device + swap chain + render target view; clear to a color~~ ✓ |
| M06 | ~~First triangle — hardcoded vertices, passthrough HLSL~~ ✓ |
| M07 | ~~Vertex/index buffer abstraction; indexed quad~~ ✓ |
| M08 | ~~Constant buffers; per-object MVP transform~~ ✓ |
| M09 | ~~Depth/stencil buffer; Z-test enabled~~ ✓ |
| M10 | ~~Texture loading (WIC, Microsoft-native); textured quad~~ ✓ |
| M11 | ~~Sampler states; UV wrap/clamp, min/mag/mip filter modes~~ ✓ |
| M12 | ~~Mesh class + Assimp FBX import; mossy stone wall in test scene~~ ✓ |

### Phase 3 — Debug UI & Input

| # | Milestone |
|---|---|
| M13 | ~~ImGui integration — DX11+Win32 backend, per-frame overlay~~ ✓ |
| M14 | ~~Raw Win32 input — keyboard (WM_INPUT) + mouse delta/wheel~~ ✓ |
| M15 | ~~Input action map — logical action bindings over raw key codes~~ ✓ |
| M16 | ~~XInput gamepad support (axes, buttons, rumble)~~ ✓ |

### Phase 4 — Lighting & Materials

| # | Milestone |
|---|---|
| M17 | ~~Blinn-Phong per-pixel lighting; directional light~~ ✓ |
| M18 | ~~Point lights (up to 8, cbuffer array)~~ ✓ |
| M19 | ~~Specular maps~~ ✓ |
| M20 | ~~Normal maps (tangent-space, TBN construction in VS)~~ ✓ |
| M21 | ~~Material struct (albedo, roughness, metallic — PBR-ready layout)~~ ✓ |

### Phase 5 — Scene & Camera

| # | Milestone |
|---|---|
| M22 | ~~Entity/component system (no ECS framework; hand-rolled, simple)~~ ✓ |
| M23 | ~~Scene graph; parent-child transforms~~ ✓ |
| M24 | ~~Camera component; arcball + FPS controller~~ ✓ |
| M25 | ~~Asset manager (path-keyed cache, ref-counted handles)~~ ✓ |

### Phase 6 — Audio

| # | Milestone |
|---|---|
| M25 | XAudio2 init; play a WAV file |
| M26 | Sound asset class; load + cache PCM/ADPCM |
| M27 | 3D positional audio (X3DAudio emitter/listener) |
| M28 | Audio mixer; volume, pitch, looping, fade in/out |

### Phase 7 — Physics

| # | Milestone |
|---|---|
| M29 | ~~Math primitives (AABB, Sphere, Ray, Plane) + narrowphase + ray tests~~ ✓ |
| M30 | Broadphase (uniform grid or dynamic AABB tree) — deferred until post-M32 |
| M31 | ~~Narrowphase (AABB-AABB, Sphere-Sphere, AABB-Sphere) — bundled with M29~~ ✓ |
| M32 | ~~Rigidbody component; linear dynamics (integrate velocity/position)~~ ✓ |
| M33 | ~~Collision response; impulse resolution, restitution, friction~~ ✓ |
| M33b | ~~Debug collider visualization — `ForwardPipeline::DrawWireSphere` / `DrawWireAABB`~~ ✓ |
| M34 | ~~Raycasting against scene colliders~~ ✓ |
| M35 | ~~OBB narrowphase + SAT~~ ✓ |
| M36 | ~~Simple character controller (capsule + step-up + slope limit)~~ ✓ |

### Phase 8 — Advanced Rendering Pipeline

| # | Milestone |
|---|---|
| M37 | ~~Mip mapping (auto-generate mipmaps at load; MIP_LINEAR sampler)~~ ✓ |
| M38 | ~~MSAA 4× (swap chain + MSAA render target; resolve to back buffer)~~ ✓ |
| M39 | ~~Skybox (equirectangular HDR panorama; unit cube, depth write off, LESS_EQUAL)~~ ✓ |
| M40 | ~~Render state cache (blend, rasterizer, depth-stencil state objects)~~ ✓ |
| M41 | ~~Shader permutation system (preprocessor define variants)~~ ✓ |
| M42 | ~~Render queue; front-to-back opaque, back-to-front transparent~~ ✓ |
| M43 | ~~Frustum culling (AABB vs 6 planes)~~ ✓ |
| M44 | ~~Shadow map (directional; CSM-ready depth pass)~~ ✓ |
| M45 | ~~PCF soft shadows~~ ✓ |
| M46 | Render-to-texture; fullscreen quad pass infrastructure |
| M47 | Deferred shading (G-buffer: albedo, normal, depth, material) |
| M48 | Point light shadow maps (cube depth map, 1–2 closest lights) |
| M49 | SSAO |
| M50 | HDR + tone mapping (Reinhard + ACES) |
| M51 | Bloom (dual Kawase blur) |
| M52 | IBL — diffuse irradiance + specular (split-sum) |

### Phase 9 — Animation

| # | Milestone |
|---|---|
| M53 | Skeleton data (bone hierarchy, bind pose, inverse bind) |
| M54 | Animation clip (keyframes: position, rotation, scale per bone) |
| M55 | Clip sampling + linear interpolation (lerp pos, slerp rot) |
| M56 | Skinned mesh renderer (GPU skinning via VS + bone cbuffer) |
| M57 | Animation state machine (states, transitions, conditions) |
| M58 | Blend tree (1D blend, 2D directional blend) |
| M59 | Root motion extraction |
| M60 | Animation events (callbacks at keyframe timestamps) |
| M61 | Import from glTF 2.0 (cgltf; replaces OBJ for complex assets) |

### Phase 10 — Engine Tooling & Profiling

| # | Milestone |
|---|---|
| M62 | ImGui integration; debug overlay, stat counters |
| M63 | GPU timestamp queries; per-pass frame timing |
| M64 | CPU profiler (hierarchical timers, `SE_PROFILE_SCOPE`) |
| M65 | In-engine console (ImGui; log viewer, command dispatch) |
| M66 | Hot-reload for HLSL shaders (watch file, recompile on change) |

### Phase 11 — Scripting

| # | Milestone |
|---|---|
| M67 | Lua runtime (sol2 binding) embedded in engine |
| M68 | Expose Entity/Component API to Lua |
| M69 | Script component; per-entity Lua update callbacks |
| M70 | Lua-accessible input, audio, physics APIs |

### Phase 12 — Editor (when engine is stable post-Phase 10)

| # | Milestone |
|---|---|
| M71 | Editor shell — separate executable, links FoxEngine.lib, hosts an engine instance |
| M72 | Viewport panel (render engine scene into an ImGui image) |
| M73 | Entity hierarchy panel + component inspector |
| M74 | Asset browser (file tree, drag-to-scene) |
| M75 | Transform gizmos (ImGuizmo; translate, rotate, scale) |
| M76 | Scene serialization/deserialization (JSON via nlohmann/json) |
| M77 | Play/pause/stop in-editor (clone scene state, run physics+scripts) |
| M78 | Undo/redo stack (command pattern) |
| M79 | Custom renderer for editor primitives (grid, wireframe, selection highlight) |

---

## Working Rules for Claude

1. **One milestone at a time.** Do not begin the next until the current one compiles, runs in the test scene, and is committed.
2. **Test scene is canonical.** `Game/` is always the live integration target. Never claim something works without wiring it into the scene.
3. **No premature abstraction.** If a pattern appears fewer than three times, keep it concrete. Refactor only when duplication actually hurts.
4. **Commit message format**: `M##: <short description>` — e.g., `M05: D3D11 device + swap chain`.
5. **Milestone numbering is fixed.** When new milestones are inserted, renumber from the insertion point forward and update this file.
6. **Dependencies are additive.** Add to `ThirdParty/` or CMake FetchContent. Never copy-paste library source inline.
7. **Platform assumption**: Windows 10+, x64 only. No cross-platform abstractions.
8. **Debug-first**: CMake default is Debug. RelWithDebInfo for profiling. Release only for final validation.
9. **Update this file** — check off milestones as they complete; add notes about key design decisions made during implementation so future sessions have context.

---

## Design Notes

### Renderer architecture (post-refactor)
- `Renderer` = D3D11 device / swap chain / surface management only.
- `ForwardPipeline` = shading strategy: owns VS/PS (`Shaders/Basic.hlsl`), input layout, TransformCB (b0), MaterialCB (b3), sampler, and all debug geometry buffers. Shadow pre-pass will feed *into* ForwardPipeline, not sit alongside it. `DeferredPipeline` is the future genuine sibling (M47).
- `LightEnvironment` = directional + point light state only (`Init`, `BindPS`). No ImGui. Game code reads public fields directly for debug controls.
- `CameraController` = single class with nested `OrbitState`/`FPSState`; `UpdateOrbit`/`UpdateFPS` are private methods. Tab switches modes.

### Debug collider visualization
- `ForwardPipeline::DrawWireSphere`, `DrawWireAABB`, `DrawWireDisc` all use `LINELIST` topology and restore `TRIANGLELIST` after each call.
- Wire draws bypass lighting via `Unlit` flag in `MaterialCB` (b3): `float Unlit` replaces one pad float; PS early-exits with `float4(AlbedoTint, 1)` when `Unlit > 0.5`.
- `DrawWireDisc` reuses ring 0 of `m_wireSphereIB` (the XZ circle, first 64 indices) — no extra GPU buffer.
- Do NOT add an `AABB` overload to `DrawWireAABB` — Renderer must not include Physics headers. Pass `aabb.min, aabb.max` at call site.
- Scene draws: green wire sphere = ball collider, yellow wire box = 60×60 floor OBB, cyan wire boxes = static OBBs.
- Toggle via `m_showColliders` bool in TestScene; checkbox in the Physics panel.

### Raycasting (M34)
- `PhysicsWorld::Raycast(ray, hit)` iterates registered spheres, planes, and static OBBs; returns the closest hit.
- `RaycastHit`: `t`, `point`, `normal`, `transform` (non-null = sphere), `kind` enum (Sphere/Plane/OBB).
- `ForwardPipeline::DrawLine` uses a `D3D11_USAGE_DYNAMIC` 2-vertex buffer updated via `Map/Unmap` each call. Vertices are in world space; model CB is identity.
- Test scene casts from the camera centre crosshair each frame. Hit shown as white wire sphere + yellow normal line when raycast toggle is on.

### OBB narrowphase (M35)
- `OBB` struct: center, halfExtents, axes[3] (world-space local axes, must stay orthonormal). `GetWorldMatrix()` builds rows from `axes[i] * halfExtents[i]` + translation — maps the unit cube (-1..1) to the OBB.
- `Intersects(OBB, OBB)` in `Intersect.h`: SAT over 15 axes (3+3 face normals, 9 edge cross products).
- `Intersects(Ray, OBB, t&)`: slab test in OBB local space. `Intersects(Ray, OBB, t&, normal&)` variant tracks which axis slab was the entry and returns the outward face normal. Sign rule: `f > 0` → ray enters from `-axes[i]` face → normal sign = −1.
- `Intersects(OBB, Sphere)`: closest-point test; clamp sphere centre projection onto each axis.
- `PhysicsWorld::ResolveSphereVsOBB`: closest-point penetration, normal impulse + friction, same pattern as ResolveSphereVsPlane.
- `ForwardPipeline::DrawWireBox(ctx, XMMATRIX world, color)`: reuses `m_wireAABBVB/IB` (unit cube) with provided OBB world matrix. Call site passes `obb.GetWorldMatrix()`.

### Render state cache (M40)
- `RenderStateCache` in `Engine/Renderer/RenderStateCache.h/.cpp`. Owned as a value member of `Renderer`; exposed via `Renderer::GetStateCache()`.
- Three `std::vector<std::pair<Desc, ComPtr<State>>>` — one each for rasterizer, depth-stencil, blend. Linear scan + `memcmp` for lookup (total state count is tiny, ~5).
- Callers must zero-init (`= {}`) the desc struct before filling fields; `memcmp` over uninitialized padding bytes would produce false misses.
- Returns raw non-owning `ID3D11XxxState*`; cache owns the lifetime via ComPtr.
- `Renderer::Initialize` calls `m_stateCache.Init(device)`, stores `m_sceneDepthState` (raw ptr) for use in `BeginFrame`. `Renderer::Shutdown` calls `m_stateCache.Clear()` before releasing the device.
- `SkyboxRenderer::Init` now takes `RenderStateCache&`; its two state objects (`LESS_EQUAL`+no-write DS, `CULL_NONE` RS) come from the cache instead of direct `CreateXxx` calls. Members are raw ptrs instead of ComPtrs.

### Mip mapping (M37)
- `Texture2D::CreateSRV` creates with `MipLevels=0` (full chain), `D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE`, `D3D11_RESOURCE_MISC_GENERATE_MIPS`, `D3D11_USAGE_DEFAULT`.
- Uploads mip 0 via `ctx->UpdateSubresource`, then calls `ctx->GenerateMips(srv)`.
- `SrvDesc.Texture2D.MipLevels = (UINT)-1` exposes all generated levels to the shader.
- `AssetManager::Init` now takes `(device, context)` — context stored as `m_context`, threaded through all texture load paths.
- Sampler was already `Anisotropic` with `MaxLOD = FLT_MAX` — no sampler changes needed.
- 1×1 default textures (white, normal) go through the same path; `GenerateMips` on a 1×1 is a no-op.

### Character controller (M36)
- `CharacterController` struct: `position` = feet (bottom of capsule). `radius`, `height`, `eyeHeight`, `stepHeight`, `slopeLimit`, `jumpSpeed`, `gravAccel`, `moveSpeed`, `gravityEnabled`.
- `GetEyePosition()` = `{pos.x, pos.y + eyeHeight, pos.z}`. `Jump()` sets `velY = jumpSpeed` if grounded.
- `PhysicsWorld::StepCharacter` runs two passes: (1) gravity + vertical move → resolve floors/ceilings via planes then OBBs (sets `isGrounded`); (2) horizontal move → resolve walls + step-up for OBBs.
- Collision uses a single sphere at `bottomCenter = {pos.x, pos.y + radius, pos.z}` — not a true two-sphere capsule.
- Step-up: OBB `maxY` = `center.y + Σ|axes[i].y|*halfExtents[i]`. If `maxY > feet && maxY <= feet + stepHeight` → snap feet up to maxY instead of horizontal push.
- Slope limit: `cosSlope = cos(slopeLimit°)`. `n.y > cosSlope` → floor (grounding); else → wall.
- `CameraController::UpdateFPS` is mouse-look only (RMB capture, yaw/pitch). WASD + jump + `StepCharacter` live in main.cpp. Camera eye = `cc.GetEyePosition()`, target = `eye + look(yaw, pitch)`.
- Global gravity toggle in TestScene: `m_gravityEnabled` syncs `m_ballRigidBody->useGravity` and `m_cc.gravityEnabled` together.
- Static OBBs drawn in cyan when `m_showColliders` is on.
