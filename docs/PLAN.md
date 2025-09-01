# Stack
- **Language**: C11

# Project Layout
```
imm/
  CMakeLists.txt
  CMakePresets.json
  external/ (submodules)
    raylib
  docs/
    PLAN.md
  assets/
    models/ (OBJ for static, IQM for animated)
    textures/
    audio/
    levels/ (JSON/CSG data)
    scripts/
  src/
    main.c
    platform/ (fs, hotreload, time)
    core/ (memory, log, math, rng)
    ecs/ (ecs.h, ecs.c, components.h)
    render/ (renderer.c, materials.c)
    physics/ (collision.c, queries.c)
    ai/ (blackboard.c, behavior.c, path.c)
    game/
      game.c (bootstrap)
      systems/ (interaction.c, inventory.c, door.c)
      scripting/ (lua_bindings.c)
    tools/ (level_viewer.c, profiler_ui.c)
  tests/
```

---

# Milestones
**M0 — Bootstrap**
- Build raylib window, basic frame loop, hot-reload config JSON
- Flycam + simple level from boxes

**M1 — ECS & Entities**
- Implement tiny ECS (or integrate Flecs)
- Components: Transform, Model, RigidBody (kinematic), ActorTag, Interactable, Door, Item, Inventory
- Systems: Transform graph, Render, Interaction (raycast use), Door open/close

**M2 — First-Person Controller**
- Capsule controller (kinematic), gravity, step offset, slopes
- Head bob, crouch, lean (toggle), interaction ray

**M3 — AI Basics**
- Perception: FOV, raycast LOS, hearing (sound events)
- States: Idle/Investigate/Alert/Chase via behavior tree
- Pathfinding: grid A* with dynamic blockers

**M4 — Simulation & Items**
- Carry/throw lightweight props; triggers & volumes
- Door locks + keys; terminals; vents; power switches
- Sound propagation from footsteps, doors, throws

**M5 — Scripting & Save**
- Lua bindings for: spawn, set/get component fields, events
- Save/Load: snapshot ECS + deterministic RNG seed

**M6 — First Mission**
- Build a small mission with 2–3 routes, 3 AI, 5 interactables
- Win/lose conditions; minimal diegetic UI (wrist display/terminal)

**Stretch**
- Navmesh generation, anim graph, advanced physics, mod IO

---

# Data Formats
- **Levels**: JSON (list of brushes/props/links) → bake collision/grid
- **Models**: OBJ for static, IQM for rigged (raylib-friendly)
- **Materials**: JSON referencing texture sets
- **Scripts**: Lua files per entity/interaction

---

# Backlog (later?)
