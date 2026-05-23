# CyberNation Server — Development Documentation

## Table of Contents

1. [Overview](#1-overview)
2. [Project Architecture](#2-project-architecture)
3. [Build & Run](#3-build--run)
4. [Layered Design](#4-layered-design)
5. [Data Structures (`data/`)](#5-data-structures-data)
6. [Core Module (`includes/core/`, `src/core/`)](#6-core-module)
7. [Game Module (`includes/game/`, `src/game/`)](#7-game-module)
8. [Phase Handlers (`includes/phase/`, `src/phase/`)](#8-phase-handlers)
9. [Networking Module (`includes/net/`, `src/net/`)](#9-networking-module)
10. [Key APIs & Important Interfaces](#10-key-apis--important-interfaces)
11. [Action JSON Request Formats](#11-action-json-request-formats)
12. [Testing & QA History](#12-testing--qa-history)
13. [Known Limitations & Future Work](#13-known-limitations--future-work)

---

![alt text](image.png)

---

## 1. Overview

**CyberNation** is a cooperative board-game backend server written in **C++17**. It manages multi-player game sessions on a hexagonal tile board, processes player actions through a structured round/phase/turn system, and returns full JSON snapshots of the game state after every action. The server supports both a single-player HTTP test mode and a multi-player room-based mode with connection/session binding.

### Key Features

| Feature | Description |
|---|---|
| **Hex Board** | 11-position hexagonal board with neighbor adjacency (layout.json) |
| **4 Stack Types** | Wild, Waste, DevA (Works), DevB (Agora) — each with 6-sided resource slots and internal paths |
| **5 Cyber-Parameters** | Cohesion (Co), Cybernation Level (Cy), Human Relation (HR), Environment (Env), Technology (Tech) |
| **3 Game Phases** | Envision → Traverse → Adopt (repeats for up to 5 rounds) |
| **5 Players** | Fixed 5-player game; first-player token shifts dynamically |
| **Disruption Cards** | 10 categories (CatA–CatK) with conditional effects, cancellation, and optional actions |
| **Feedback Token System** | 6 token types drawn from a finite reserve, placed on an 11-slot Adapt track |
| **Goal System** | 10 goals with victory conditions and reverse-goal pairs |
| **Walk Path Mechanic** | People token traverses connected hex sides, collecting resources |
| **HTTP JSON API** | RESTful endpoints for state query, action submission, and room management |
| **Room System** | Multi-connection room with session binding, broadcast snapshots, and outbox polling |

---

## 2. Project Architecture

```
CyberNation_Server/
├── data/                          # Static game data (JSON)
│   ├── disruption.json            # 50+ disruption cards
│   ├── goal.json                  # 10 goal cards
│   ├── layout.json                # 11-tile hexagonal board layout
│   ├── role.json                  # (empty — reserved)
│   └── stack.json                 # 45 stack tiles (Wild/Waste/DevA/DevB)
├── includes/                      # Header files
│   ├── core/                      # Core data types & models
│   │   ├── Types.hpp, Params.hpp, Player.hpp, Tile.hpp
│   │   ├── Stack.hpp, Goal.hpp, DisruptionCard.hpp
│   │   ├── FeedbackPool.hpp, FeedbackTokenManager.hpp
│   │   ├── CardManager.hpp, DataLoader.hpp
│   │   ├── Action.hpp, ActionResult.hpp, GamePhase.hpp
│   ├── game/                      # Game logic & state
│   │   ├── GameState.hpp, GameRoom.hpp
│   │   ├── GameUtility.hpp, RoundController.hpp, PhaseHandler.hpp
│   ├── phase/                     # Per-phase action handlers
│   │   ├── EnvisionPhaseHandler.hpp
│   │   ├── TraversePhaseHandler.hpp
│   │   └── AdoptPhaseHandler.hpp
│   └── net/                       # Networking
│       ├── Room.hpp, RoomManager.hpp
│       └── http/httplib.h         # cpp-httplib (header-only HTTP library)
├── src/                           # Implementation files (mirrors includes/)
├── test/                          # Unit/integration tests
├── test.html                      # Single-player test UI
├── room_test.html                 # Multi-player room test UI
├── Makefile                       # Build system
└── README.md                      # Action JSON format reference
```

### Dependency Graph

```
                    ┌──────────────┐
                    │  HTTP Server │  (http-server / room-http-server)
                    └──────┬───────┘
                           │
                    ┌──────▼───────┐
                    │   GameRoom   │  (session owner)
                    └──────┬───────┘
                           │
              ┌────────────┼────────────┐
              │            │            │
     ┌────────▼───┐ ┌─────▼──────┐ ┌───▼──────────┐
     │ GameState  │ │ RoundCtrl  │ │ PhaseHandlers │
     │ (data)     │ │ (flow)     │ │ (validation)  │
     └────────────┘ └─────┬──────┘ └───┬───────────┘
                          │            │
                    ┌─────▼────────────▼───┐
                    │    GameUtility       │
                    │ (shared operations)  │
                    └──────────────────────┘
```

---

## 3. Build & Run

### Prerequisites

- **g++** with C++17 support
- **pthread** (for HTTP server threading)
- No external dependencies (uses bundled `nlohmann/json.hpp` and `cpp-httplib`)

### Build Commands

```bash
# Single-player HTTP server (port 8080)
make server
./out/server

# Multi-player room server (port 8081)
make room-server
./out/room-server

# All test targets
make test-all

# Individual test targets
make adapt-sim           # Adapt phase simulation test
make disruption-categories  # Disruption category test
make traverse-test       # Traverse phase test

# Clean build artifacts
make clean
```

---

## 4. Layered Design

The server follows a strict three-layer architecture:

### Layer 1: RoundController (Flow Control)
- **Responsibility**: Who can act, when, and in what order
- Manages turn order, phase advancement, round counting
- Validates player identity (`currentPlayerId`)
- Enforces action sequencing (e.g., Traverse: draw → resolve → walk)
- Handles `pass` and `advance` meta-actions
- Detects game-over conditions (goal met or max rounds reached)

### Layer 2: PhaseHandler (Rule Validation)
- **Responsibility**: What actions are legal in the current phase
- Three concrete handlers: `EnvisionPhaseHandler`, `TraversePhaseHandler`, `AdoptPhaseHandler`
- Each validates action-specific parameters and resource costs
- Mutates `GameState` directly on success
- Returns `ActionResult` with status and message

### Layer 3: GameState (Data Container)
- **Responsibility**: Pure data, no logic
- Holds board, players, parameters, decks, token bags, active disruption
- Provides `toJson()` for full state serialization
- Provides `snapshot()` for JSON string output

### Shared: GameUtility
- **Responsibility**: Cross-cutting operations used by multiple handlers
- `walkPath()` — People token traversal and resource collection
- `drawDisruption()` — Draw from disruption deck
- `applyDisruptionEffect()` — Resolve disruption cards (all 10 categories)
- `changeTileStack()` — Swap a tile's base/overlay stack
- `tradeForDisruption()` — CatK trade resolution

---

## 5. Data Structures (`data/`)

### 5.1 `layout.json` — Board Layout

Defines the 11 hexagonal tile positions and their neighbor relationships.

```json
{
  "position": 0,        // Tile index (0–10)
  "tileId": 0,          // Starting tile type ID
  "rotation": 0,        // Initial rotation (0–5)
  "neighbours": [       // 6 sides: [neighbor_tile, neighbor_side] or [] for edge
    [1, 3], [2, 4], [3, 5], [4, 0], [5, 1], [6, 2]
  ]
}
```

- **11 positions**: 0 (inner/center), 1–6 (middle ring), 7–10 (outer ring)
- Empty `[]` means that side faces the map edge (valid for people token placement)
- Each side maps to a neighbor tile+side pair

### 5.2 `stack.json` — Stack Tile Library

45 stack tiles across 4 types, each with 6 sides containing resource labels and 3 internal path connections.

```json
{
  "id": 23,
  "type": "DevA",
  "sides": [
    ["Env", "Tech"],           // Side 0 resources
    ["HR", "HR"],              // Side 1
    ...                        // Sides 2–5
  ],
  "paths": [[3, 0], [4, 2], [1, 5]]  // Bidirectional connections
}
```

| Type | Count | Token Mapping |
|---|---|---|
| **Wild** (id 1–11) | 11 | → TURN_WILD feedback token |
| **Waste** (id 12–22) | 11 | → LOSE_COHESION feedback token |
| **DevA / Works** (id 23–33) | 11 | → TURN_WASTE feedback token |
| **DevB / Agora** (id 34–44) | 12 | → SOLVE_DISRUPTION feedback token |

- **Sides**: Array of 6 resource labels. Resources include `HR`, `Tech`, `Env` (positive) and `-Co` (negative/cohesion loss on Waste tiles).
- **Paths**: 3 bidirectional pairs connecting stack sides. Used by `walk_path` to traverse through tiles.
- Tiles are drawn from typed `CardManager` decks and placed onto board positions.

### 5.3 `disruption.json` — Disruption Cards

50+ disruption cards across 10 categories (CatA–CatK). Each card has:

```json
{
  "name": "HURRICANE_1",
  "category": "CatA",
  "description": "For each affected Stack, turn into waste unless pay 1 cybernation level",
  "conditionType": "none" | "stack" | "resource",
  "condition": {},
  "stackTarget": [0, 2, 5, 8, 9],
  "effect": [{"TurnWaste": 1}],
  "cost": [{"Cy": -1}],
  "cancel": true
}
```

#### Category Summary

| Category | Description | Cancellable | Key Params |
|---|---|---|---|
| **CatA** | Turn stacks to waste/wild (per-tile cancel) | Yes | `canceltiles` |
| **CatB** | Lose cohesion / stack-conditional effects | Yes | `cancel` or `canceltiles` |
| **CatC** | Conditional turn-to-waste + cohesion loss | Yes | `canceltiles` |
| **CatD** | Turn wild stacks to waste | Yes | `canceltiles` |
| **CatE** | Swap or draw goal card | Yes | `cancel` |
| **CatF** | Gain cohesion + distributed resources | No | `resourceDistribution` |
| **CatG** | Per-stack choice between two effects | No | `effectIndex` |
| **CatH** | Choice: move people / gain HR / gain Cy / draw tokens | No | `effectIndex`, `ppl` |
| **CatI** | Build DevA on undeveloped tiles | No | `targetTiles` |
| **CatJ** | Optional cost for bonus gain | No | `useOptional` |
| **CatK** | Trade resources | No | `trade` (`src`, `dst`, `amount`) |

- **Condition types**: `none` (always applies), `stack` (only affects tiles matching `stackType`), `resource` (compares two parameters, e.g. `Tech > HR`)
- **Effects** are ordered pairs of `DisruptionEffect` + amount
- **Costs** are paid to cancel the card or skip tiles

### 5.4 `goal.json` — Goal Cards

10 goals, each with victory conditions and a start-of-game board effect.

```json
{
  "id": 0,
  "name": "Restore and Rewild",
  "victory_condition": {
    "stack": { "Wild": { "compare": "EQ", "num": 11 } },
    "HR": { "compare": "GE", "num": 11 }
  },
  "start_effect": { "DevA": [7], "DevB": [0], "Waste": [2] },
  "reverseGoalId": 1
}
```

- **Victory conditions**: Multiple AND-combined conditions on stack counts (with optional `position`: `inner`/`middle`/`outer`) and parameter levels
- **Comparators**: `GT`, `GE`, `EQ`, `LE`, `LT`, `NE`
- **Reverse goal pairs**: e.g. Goal 0 ↔ Goal 1. Swapping flips to the reverse goal.
- **Start effects**: Initial board setup when the goal is active

### 5.5 `role.json`

Currently empty — reserved for role cards (not yet implemented).

---

## 6. Core Module

### 6.1 Types (`Types.hpp`)

Central enumeration definitions:

| Enum | Values |
|---|---|
| `StackType` | `WILD`, `WASTE`, `DEV_A`, `DEV_B`, `UNKNOWN` |
| `CyberParameter` | `COHESION`, `CYBERNATION_LEVEL`, `HUMAN_RELATION`, `ENVIRONMENT`, `TECHNOLOGY` |
| `TokenEffect` | `TURN_WILD`, `LOSE_COHESION`, `TURN_WASTE`, `SOLVE_DISRUPTION`, `DEVELOP_STACK`, `TRANSFORM_STACK` |
| `GamePhase` | `ENVISION`, `TRAVERSE`, `ADOPT` |
| `DisruptionEffect` | `TURN_WASTE`, `TURN_WILD`, `TURN_DEV_A`, `TURN_DEV_B`, `COHESION`, `HUMAN_RELATION`, `CYBERNATION`, `TECHNOLOGY`, `ENVIRONMENT`, `RESOURCES`, `TOKEN`, `TRADE`, `CAP_ENV`, `IGNORE_COHESION_EFFECT`, `SWAP_GOAL`, `DRAW_GOAL`, `MOVE_PEOPLE` |
| `ActionStatus` | `SUCCESS`, `INVALID_TARGET`, `INVALID_ACTION`, `INSUFFICIENT_RESOURCE`, `NOT_YOUR_TURN`, `PLAYER_ALREADY_PASSED`, `GAME_OVER`, `UNKNOWN_ERROR` |

Includes inline conversion functions: `parseCyberParameter()`, `strToStackType()`, `tokenEffectToStr()`, `strToDisruptionEffect()`, etc.

### 6.2 Params (`Params.hpp/.cpp`)

Tracks the 5 cyber-parameters with a hard cap of 25. Key rule: **Cohesion caps all other resources** — HR, Env, Tech cannot exceed Cohesion.

```cpp
class Params {
    int cohesion = 25;
    int cybernationLevel = 2;
    int humanRelation = 7;
    int environment = 7;
    int technology = 7;
    // getters, setters with validation, adjustParam(delta)
};
```

### 6.3 Player (`Player.hpp/.cpp`)

Minimal: holds `id` and `hasFirstPlayerToken` boolean.

### 6.4 Tile (`Tile.hpp/.cpp`)

Represents one hexagonal board position.

```cpp
class Tile {
    int position;           // 0–10
    int rotation;           // 0–5 (affects side-to-stack-side mapping)
    Stack base;             // Always present
    std::optional<Stack> overlay;  // DevA/DevB development layer
    std::vector<std::pair<int,int>> neighbours;  // 6 sides
};
```

Key methods:
- `getEffectiveType()` — returns overlay type if present, else base type
- `getSideResources(int side)` — collects resources from both base and overlay on a given board side
- `boardSideToStackSide(int)` / `stackSideToBoardSide(int)` — handles rotation offset
- `getNeighbourTile(int side)` — returns `-1` for map edges

### 6.5 Stack (`Stack.hpp/.cpp`)

```cpp
class Stack {
    int id;
    StackType type;
    std::vector<std::vector<std::string>> sides;  // 6 sides of resource labels
    std::map<int, int> paths;  // bidirectional side-to-side connections
};
```

- `getConnectedSide(int entrySide)` — returns the exit side given an entry side, or `-1` if no path exists

### 6.6 DisruptionCard (`DisruptionCard.hpp/.cpp`)

Models a single disruption card with category, condition, stack targets, ordered effects, costs, optional costs/gains, and cancellable flag. The `hasTileChangeEffect()` helper checks if any effect modifies tile types.

### 6.7 Goal (`Goal.hpp/.cpp`)

Holds victory conditions (`vector<victory_condition>`) and stack start effects (`map<StackType, vector<int>>`).

### 6.8 FeedbackPool (`FeedbackPool.hpp/.cpp`)

Finite reserve of feedback tokens (default 10 each, 60 total). Operations:
- `draw(TokenEffect)` — decrements count, returns false if empty
- `putBack(TokenEffect)` — returns token to reserve

### 6.9 FeedbackTokenManager (`FeedbackTokenManager.hpp/.cpp`)

Manages the token bag (drawn from reserve) and the feedback track (drawn from bag).

- `rebuildBagFromBoard(board, pool)` — scans all tiles, draws corresponding tokens from pool
- `drawTrackFromBag(trackSize)` — shuffles bag, draws up to `trackSize` tokens into the track
- Token mapping: Wild→TURN_WILD, Waste→LOSE_COHESION, DevA→TURN_WASTE, DevB→SOLVE_DISRUPTION

### 6.10 CardManager (`CardManager.hpp`)

Generic template for deck management:
```cpp
template <typename T>
class CardManager {
    std::vector<T> deck;
    std::vector<T> discard;
    // draw(), shuffle(), reshuffle(), returnToDiscard()
};
```

### 6.11 DataLoader (`DataLoader.hpp/.cpp`)

JSON parsing with template specialization for `DisruptionCard`, `Stack`, `Tile`, and `Goal`. Uses `nlohmann/json.hpp`.

### 6.12 Action & ActionResult (`Action.hpp`, `ActionResult.hpp`)

```cpp
struct Action {
    int playerId;
    std::string type;       // e.g. "shift_power", "resolve_disruption"
    std::unordered_map<std::string, std::string> params;
};

struct ActionResult {
    ActionStatus status;
    ActionMessage message;  // {type, payload} strings
    // Factory methods: success(), ignored(), invalid(), alreadyPassed()
};
```

---

## 7. Game Module

### 7.1 GameState (`GameState.hpp/.cpp`)

The central data container. Key members:

| Field | Type | Description |
|---|---|---|
| `board` | `vector<Tile>` (11) | Hex board tiles |
| `players` | `Player[5]` | Player states |
| `params` | `Params` | Cyber-parameters |
| `pool` | `FeedbackPool` | Finite token reserve |
| `tokenManager` | `FeedbackTokenManager` | Bag + track management |
| `tokenBag` | `vector<TokenEffect>` | Current token bag |
| `disruptionManager` | `CardManager<DisruptionCard>` | Disruption deck |
| `goalManager` | `CardManager<Goal>` | Goal deck |
| `*StackManager` | 4× `CardManager<Stack>` | Typed stack decks |
| `peopleToken` | `pair<int,int>` | `{tile, side}` position |
| `currentGoal` | `Goal` | Active goal card |
| `activeDisruption` | `optional<DisruptionCard>` | Currently drawn disruption |
| `adaptTrack` | `vector<TokenEffect>` | 11-slot Adapt track |
| `adaptCursor` | `int` | Current Adapt slot index |
| `ignoreCohesionLossThisRound` | `bool` | From CatH effect |

Key methods:
- `rebuildTokenBag()` — scans board, adds tokens from pool to bag, shuffles, syncs with tokenManager
- `isActiveGoalMet()` — evaluates all victory conditions against current state
- `randomizeBoard()` — randomizes base stacks on all tiles
- `setPeopleToken(pos)` — validates edge placement constraint
- `toJson()` — full JSON serialization including board, params, pool, bag, adapt track, active disruption, players, goal

### 7.2 RoundController (`RoundController.hpp/.cpp`)

Controls game flow:

```
Round 1..5:
  Envision Phase (max 5 turns, turn cost: Tech -1 at turn 3–4, Tech -2 at turn 5)
    → Players act in first-player order; pass to skip
  Traverse Phase (enforced sequence: draw_disruption → resolve_disruption → walk_path)
    → First player only
  Adopt Phase (resolve 11 feedback track slots in cursor order)
    → Players rotate; Agora triggers forced disruption resolution
```

Controlled flow mode is enforced by the RoundController. In controlled mode, players cannot skip required actions or manually advance phases. For example:

- Traverse strictly follows:
  `draw_disruption -> resolve_disruption -> walk_path`
- After a successful `walk_path`, the controller automatically advances to `ADOPT`
- In ADOPT, `draw_disruption` is not a standalone action. Agora-triggered disruption draws happen automatically through `resolve_feedback`
- The same player must resolve the disruption before the next player continues the feedback sequence

**Envision turn costs**: Turns 3 and 4 cost 1 Technology; turn 5 costs 2 Technology. Players must pass if they can't afford the cost.

**Traverse flow enforcement**: The controller tracks `traverse_record.stage` (0→1→2) and rejects out-of-order actions.

**Adopt Agora handling**: When a `SOLVE_DISRUPTION` feedback token is resolved with `allow`, the same player must immediately resolve the drawn disruption before advancing.

### 7.3 GameRoom (`GameRoom.hpp/.cpp`)

Top-level session wrapper. Owns `GameState` and `RoundController`. Generates a unique `sessionId` on construction. Delegates `receiveAction()` to the controller. `getSnapshot()` combines game state, controller state, and session ID into one JSON.

### 7.4 GameUtility (`GameUtility.hpp/.cpp`)

Shared operations used by Traverse and Adopt handlers:

- **`walkPath(state)`**: Starts from people token position, follows stack path connections tile-to-tile, collecting resources from both base and overlay on each traversed side. Stops when a side has no connected path or reaches a map edge.
- **`drawDisruption(state)`**: Draws from disruption deck into `activeDisruption`.
- **`applyDisruptionEffect(state, action)`**: Main disruption resolution logic handling all 10 categories with parameter parsing, condition checking, cancellation, and effect application.
- **`changeTileStack(state, tilePos, targetType)`**: Swaps a tile's base (Wild/Waste) or overlay (DevA/DevB) by drawing from the appropriate typed stack deck.
- **`tradeForDisruption(state, action)`**: Handles CatK resource trading.

---

## 8. Phase Handlers

### 8.1 EnvisionPhaseHandler

| Action | Cost | Effect |
|---|---|---|
| `shift_power` | 1 HR | Transfer first-player token to target player |
| `come_together` | 1 Env | Gain 1 Cohesion |
| `prepare` | 2 HR | Gain 1 Cybernation Level |
| `set_course` (move_people) | 2 Tech | Move people token to a map edge (side with no neighbor) |
| `set_course` (rotate) | 2 Tech | Rotate a tile's stack (clockwise/counterclockwise, steps or degree) |
| `connect` | 2 of one relationship | Gain 1 of another relationship (HR/Env/Tech only) |
| `steer` | — | Add one feedback token from reserve to token bag |

### 8.2 TraversePhaseHandler

Delegates to `GameUtility`:
- `draw_disruption` → `GameUtility::drawDisruption()`
- `resolve_disruption` → `GameUtility::applyDisruptionEffect()`
- `walk_path` → `GameUtility::walkPath()`

### 8.3 AdoptPhaseHandler

Manages the 11-slot Adapt feedback track with cursor-based resolution.

- **`preparePhase(state)`**: Rebuilds token bag from board and fills the 11-slot track.
- **`resolve_feedback`**: Processes one track slot at `adaptCursor`. Requires `target_tile` and `decision` (`allow`/`cancel`). Cancel costs 1 Cy. Allow applies the token effect.
- **Token effects**:
  - `TURN_WILD` → changes tile to Wild
  - `TURN_WASTE` → changes tile to Waste
  - `LOSE_COHESION` → -2 Cohesion (blocked if `ignoreCohesionLossThisRound`)
  - `SOLVE_DISRUPTION` → draws a disruption card (Agora)
  - `DEVELOP_STACK` → places DevA/DevB overlay on undeveloped tile
  - `TRANSFORM_STACK` → toggles DevA↔DevB

- **Track slot → ring mapping**:
  - Slot 0 → inner tile (position 0)
  - Slots 1–6 → middle ring (positions 1–6)
  - Slots 7–10 → outer ring (positions 7–10)

- **Phase cleanup** (`finalizeAdaptPhaseCleanup`): After all 11 slots are resolved, tokens placed on inner (0) + outer ring (7–10) return to the bag; others return to the reserve pool. This implements the rulebook step-2 cleanup.

---

## 9. Networking Module

### 9.1 HTTP Server (`http-server.cpp`)

Single-player test server on **port 8080**.

| Endpoint | Method | Description |
|---|---|---|
| `/state` | GET | Full game state snapshot |
| `/action` | POST | Process action through GameRoom → RoundController |
| `/reset` | POST | Recreate GameRoom (fresh state) |
| `/test/action` | POST | Bypass RoundController; invoke PhaseHandler directly (for manual testing) |

CORS headers enabled for all origins.

### 9.2 Room HTTP Server (`room-http-server.cpp`)

Multi-player room server on **port 8081**.

| Endpoint | Method | Description |
|---|---|---|
| `/state` | GET | Room state snapshot |
| `/join` | POST | Join the room; server assigns `connId` and `playerId` (P0–P4) |
| `/start` | POST | Start the game (requires `sessionId`) |
| `/continue` | POST | Continue from current state after reset |
| `/action` | POST | Submit an action (server binds `playerId` from connection, clients cannot spoof) |
| `/messages` | GET | Poll outbox messages (broadcast updates from other players) |
| `/reset` | POST | Reset entire room |

**Connection binding**: Each client gets a `sessionId` on `/join`. The server maps `sessionId → connId → playerId`. On `/action`, the server overrides `playerId` from the connection map, preventing identity spoofing.

**Broadcast**: On successful actions, the full game snapshot is broadcast to all connected clients via their outbox, polled through `/messages`.

### 9.3 Room (`Room.hpp/.cpp`)

Manages a single game room:
- `joinPlayer(conn_id)` — assigns next available player ID (0–4)
- `onAction(conn_id, action)` — binds identity, processes action, broadcasts on success
- `startGame()` / `continueGame()` — transitions to PLAYING state
- Room states: `WAITING → PLAYING → FINISHED`

### 9.4 RoomManager (`RoomManager.hpp/.cpp`)

Manages multiple rooms with 6-character alphanumeric room codes. Currently used for programmatic room creation but the `room-http-server` uses a single-room model for simplicity.

---

## 10. Key APIs & Important Interfaces

### 10.1 `RoundController::processAction(Action, GameState) → ActionResult`

The main entry point for all player actions. Flow:
1. Validate `playerId == currentPlayerId`
2. Handle meta-actions (`pass`, `advance`)
3. Validate phase-specific constraints (Traverse ordering, Adopt flow)
4. Delegate to the appropriate `PhaseHandler::handle()`
5. On success, advance turn/phase state

### 10.2 `PhaseHandler::handle(Action, GameState) → ActionResult`

Virtual interface implemented by each phase handler. Validates game rules and mutates state.

### 10.3 `GameState::toJson() → json`

Returns complete game state as `nlohmann::json`:
- `board[]` — tiles with position, type, rotation, paths (board-side), sideResources
- `params` — 5 cyber-parameter values
- `pool` — feedback token reserve counts
- `tokenBagCount`, `tokenBagBreakdown` — current bag state
- `peopleToken` — `[tile, boardSide]` and `peopleTokenStackSide`
- `adapt.track` — 11-slot track with cursor and completion status
- `activeGoal` — with conditions, met status, start effect
- `activeDisruption` — full card details if drawn
- `players[]` — id, isFirstPlayer

### 10.4 `GameUtility` Static Methods

| Method | Purpose |
|---|---|
| `walkPath(state)` | Traverse path from people token, collect resources |
| `drawDisruption(state)` | Draw disruption card |
| `applyDisruptionEffect(state, action)` | Resolve disruption (all categories) |
| `changeTileStack(state, pos, type)` | Change tile base/overlay |
| `tradeForDisruption(state, action)` | CatK trade |

### 10.5 `DataLoader` Static Methods

| Method | Input | Output |
|---|---|---|
| `loadDisrupt(filename)` | `disruption.json` | `vector<DisruptionCard>` |
| `loadStack(filename)` | `stack.json` | `vector<Stack>` |
| `loadTile(filename)` | `layout.json` | `vector<Tile>` |
| `loadDeck<Goal>(filename)` | `goal.json` | `CardManager<Goal>` |

---

## 11. Action JSON Request Formats

### Envision Phase

```json
// shift_power
{ "phase": "ENVISION", "playerId": 0, "type": "shift_power",
  "params": { "targetPlayerId": 1 } }

// come_together
{ "phase": "ENVISION", "playerId": 0, "type": "come_together" }

// prepare
{ "phase": "ENVISION", "playerId": 0, "type": "prepare" }

// set_course (move_people — edge sides only)
{ "phase": "ENVISION", "playerId": 0, "type": "set_course",
  "params": { "mode": "move_people", "tile": 3, "side": 1 } }

// set_course (rotate — steps preferred, falls back to degree)
{ "phase": "ENVISION", "playerId": 0, "type": "set_course",
  "params": { "mode": "rotate", "tile": 3, "degree": 2 } }

// connect
{ "phase": "ENVISION", "playerId": 0, "type": "connect",
  "params": { "cost": "HR", "gain": "Tech" } }

// steer
{ "phase": "ENVISION", "playerId": 0, "type": "steer",
  "params": { "tokenType": "SOLVE_DISRUPTION" } }
```

### Traverse Phase

```json
// draw_disruption
{ "phase": "TRAVERSE", "playerId": 0, "type": "draw_disruption" }

// resolve_disruption (example with all possible params)
{ "phase": "TRAVERSE", "playerId": 0, "type": "resolve_disruption",
  "params": {
    "cancel": "1",
    "canceltiles": "1,3,5",
    "effectIndex": "0,1,0",
    "targetTiles": "2,5",
    "useOptional": "1",
    "ppl": "3,4",
    "resourceDistribution": { "HR": "2", "Tech": "2", "Env": "1" },
    "trade": { "src": "HR", "dst": "Tech", "amount": "1" }
  }
}

// walk_path
{ "phase": "TRAVERSE", "playerId": 0, "type": "walk_path" }
```

### Adopt Phase

```json
// resolve_feedback
{ "phase": "ADOPT", "playerId": 0, "type": "resolve_feedback",
  "params": { "target_tile": "0", "decision": "allow" } }

// resolve_disruption (triggered by Agora feedback)
{ "phase": "ADOPT", "playerId": 0, "type": "resolve_disruption",
  "params": { "cancel": "1" } }
```

---

## 12. Testing & QA History

> The following summarizes testing activities performed throughout development. Tester names have been omitted.

### Phase Integration & HTTP Testing

The Adapt Phase actions were fully integrated, with disruption functionality consolidated through shared GameUtility methods. The dual Feedback Track was merged, and the Envision `fillFeedbackTrack` process was automated — token bag population, drawing, and track filling now happen automatically at the start of each Envision round rather than as a first-player action. All Adapt Phase actions were tested via HTTP and confirmed correct. JSON request format documentation for Adapt was added to the README.

### Envision Phase Bug Fixes

During HTTP testing of the Envision phase, two issues were found and fixed in `set_course`:
- **move_people mode**: The people token can only be placed on outer hex edges (sides with no neighbor in `layout.json`). An incorrect test case was fixed in the documentation.
- **rotate mode**: A branch logic error was corrected. Rotation now prioritizes the `steps` parameter; if missing, it falls back to `degree`.

### Traverse Phase Testing & UI Fixes

While the backend was correct, the test UI had incorrect side link mappings that caused display errors during Traverse testing. The Traverse test UI path display was also optimized. Small changes to `GameRoom` and `GameState` were made:
- `GET /state` and `POST /test/action` now include `sessionId` for frontend session differentiation
- Server restart clears expired path highlights
- `GameState::toJson` now includes `peopleToken` for visualizing people token positions

### Adapt Phase Deep Testing

Traverse and Adapt phases were fully tested (Disruption cards received a lighter pass). The `feedback_resolve` testing was the most comprehensive, covering all six feedback token types. Several bugs were found and fixed:

**Visualization fixes:**
- Added 11-slot Adapt track with cursor indicator and token bag breakdown panel
- Fixed incorrect side number mappings, path connections, and tile colors (initial hardcoded values adjusted; colors now change based on tile type)
- Minor layout optimizations applied

**Logic fixes (disruption resolution, feedback resolution, Envision-Adapt interaction):**
- Removed redundant `cancel_disruption` action from Adapt Phase; `resolve_disruption` now handles cancellation
- Fixed `steer` to append tokens to existing `tokenBag` via `setTokenBag`, preventing bag from being wiped
- `resolve_feedback` now only resets the token bag when it is empty (non-empty bags are preserved)
- `TRANSFORM_STACK` token: uses `getEffectiveType()` to toggle DevA↔DevB, compatible with tiles where development is on the base layer rather than overlay
- `GameState::toJson` additions: `adapt.track`, `tokenBagBreakdown`; `rebuildTokenBag` now syncs `tokenManager`

### RoundController Testing

The RoundController was tested end-to-end, excluding Role cards and game-end mechanics (Goal triggering). All other flows — phase sequencing, actions, and interactions — were tested in controlled scenarios. Bugs found and fixed:

**Envision:**
- Token bag logic on phase entry was incorrect (token retention, consumption, drawing)
- After a successful non-pass action, the handler only returned `handler->handle(...)` without advancing to the next player — now fixed
- `set_course` rotate did not propagate rotation to subsequent state and path logic

**Traverse:**
- First player token was incorrectly returning to P0 on phase entry
- Missing hard constraint: `draw_disruption → resolve_disruption → walk_path` order was not enforced — now enforced
- People token physical position was moving incorrectly when it should stay at board edge coordinates; `set_course` rotate now updates the side coordinate properly

**Adapt:**
- Removed `draw_disruption` from Adapt (not valid in controlled flow)
- `resolve_feedback` was stuck with the first player executing all feedback resolutions — now rotates properly among players
- When encountering `SOLVE_DISRUPTION` (Agora) feedback, the same player was not forced to resolve the disruption before the next player's turn, causing cascading errors (unable to draw cards, etc.) — fixed with `pendingDisruptionResolution` tracking


### 12.1 Controlled Flow Rules

The server uses a controlled flow system managed by `RoundController`. In controlled mode, players cannot manually skip required actions or freely advance phases. The controller enforces action ordering, current-player validation, and phase progression automatically.

#### Traverse Controlled Flow

Traverse strictly follows this sequence:

```text
draw_disruption -> resolve_disruption -> walk_path
```

After a successful `walk_path`, the controller automatically advances the game to the `ADOPT` phase.

`advance` is not used in controlled `TRAVERSE`.

#### ADOPT Controlled Flow

In controlled `ADOPT` flow:

1. `draw_disruption` is not a standalone player action.
2. Agora (`SOLVE_DISRUPTION`) feedback tokens automatically trigger disruption draw during `resolve_feedback`.
3. The same player must complete `resolve_disruption` before the next player continues the feedback sequence.
4. After all feedback resolutions are completed, the controller automatically advances the phase.

`advance` is not used in controlled `ADOPT`.

---

### 12.2 Action JSON Request Formats

#### 12.2.1 Envision Phase

##### Shift Power

```json
{
    "phase": "ENVISION",
    "playerId": 0,
    "type": "shift_power",
    "params": {
        "targetPlayerId": 1
    }
}
```

##### Come Together

```json
{
    "phase": "ENVISION",
    "playerId": 0,
    "type": "come_together"
}
```

##### Prepare

```json
{
    "phase": "ENVISION",
    "playerId": 0,
    "type": "prepare"
}
```

##### Set Course: Move People

The people token may only be placed on a map edge, meaning a hex side with no neighbor tile in `data/layout.json`.

Example: tile `3` has edge sides `1` and `3`. Side `4` faces another tile and will fail.

```json
{
    "phase": "ENVISION",
    "playerId": 0,
    "type": "set_course",
    "params": {
        "mode": "move_people",
        "tile": 3,
        "side": 1
    }
}
```

##### Set Course: Rotate

```json
{
    "phase": "ENVISION",
    "playerId": 0,
    "type": "set_course",
    "params": {
        "mode": "rotate",
        "tile": 3,
        "degree": 2
    }
}
```

##### Connect

```json
{
    "phase": "ENVISION",
    "playerId": 0,
    "type": "connect",
    "params": {
        "cost": "HR",
        "gain": "Tech"
    }
}
```

##### Steer

```json
{
    "phase": "ENVISION",
    "playerId": 0,
    "type": "steer",
    "params": {
        "tokenType": "SOLVE_DISRUPTION"
    }
}
```

##### Feedback Track

The feedback track is not a player action.

At the start of each Envision round, before player actions, the server adds feedback tokens from the current board into the existing token bag. This is limited by the finite pool. The server then shuffles the bag and draws up to 11 `adaptTrack` slots from it.

Drawn tokens leave the bag. Whether they return later is decided by ADOPT cleanup rules.

The client reads track state from `gameState` or snapshot. There is no `fill_track` request.

---

#### 12.2.2 Traverse Phase

Controlled flow enforces this order:

```text
draw_disruption -> resolve_disruption -> walk_path
```

After a successful `walk_path`, the controller advances to `ADOPT` automatically.

`advance` is not used in controlled `TRAVERSE`.

##### Draw Disruption Card

```json
{
    "phase": "TRAVERSE",
    "playerId": 0,
    "type": "draw_disruption"
}
```

##### Resolve Disruption Card

```json
{
    "phase": "TRAVERSE",
    "playerId": 0,
    "type": "resolve_disruption",
    "params": {
        "cancel": "1",
        "canceltiles": "1,3,5",
        "effectIndex": "0,1,0",
        "targetTiles": "2,5",
        "useOptional": "1",
        "ppl": "3,4",
        "resourceDistribution": {
            "HR": "2",
            "Tech": "2",
            "Env": "1"
        },
        "trade": {
            "src": "HR",
            "dst": "Tech",
            "amount": "1"
        }
    }
}
```

##### Walk People Token

```json
{
    "phase": "TRAVERSE",
    "playerId": 0,
    "type": "walk_path"
}
```

##### Fields for Each Disruption Card Category

```text
CatA:             { "canceltiles": "1,3" }
CatB (none/res):  { "cancel": "1" }
CatB (stack):     { "canceltiles": "1,3" }
CatE:             { "cancel": "1" }
CatF:             { "HR": "2", "Tech": "2", "Env": "1" }
CatG:             { "effectIndex": "0,1,0" }
CatH:             { "effectIndex": "0", "ppl": "3,4" }
CatI:             { "targetTiles": "2,5" }
CatJ:             { "useOptional": "1" }
CatK:             { "src": "HR", "dst": "Tech", "amount": "1" }
```

---

#### 12.2.3 Adapt / ADOPT Phase

##### Resolve Feedback

```json
{
    "phase": "ADOPT",
    "playerId": 0,
    "type": "resolve_feedback",
    "params": {
        "target_tile": "0",
        "decision": "allow"
    }
}
```

##### Resolve Disruption

Use the same `resolve_disruption` format as the Traverse phase. Optional fields such as `disruption_name`, `times`, and `decision` may also be used on the same action type if needed.

```json
{
    "phase": "ADOPT",
    "playerId": 0,
    "type": "resolve_disruption",
    "params": {
        "cancel": "1",
        "canceltiles": "1,3,5",
        "effectIndex": "0,1,0",
        "targetTiles": "2,5",
        "useOptional": "1",
        "ppl": "3,4",
        "resourceDistribution": {
            "HR": "2",
            "Tech": "2",
            "Env": "1"
        },
        "trade": {
            "src": "HR",
            "dst": "Tech",
            "amount": "1"
        }
    }
}
```

In ADOPT, `draw_disruption` is not a valid standalone action in controlled flow.

Disruption draw happens automatically when `resolve_feedback` allows the `SOLVE_DISRUPTION` Agora token. The same player must then call `resolve_disruption` before the next player's `resolve_feedback`.

When the feedback sequence completes, the controller advances phase automatically.

Use `resolve_disruption` with `"cancel": "1"` in `params` when the same behaviour as the old Adopt-only alias is needed.

---

### 12.3 Manual Testing Entry Points

This repository currently provides two manual testing entry points. They test different layers of the system.

---

#### 12.3.1 Rules Debug Server: `server` and `test.html`

This entry point is designed for rules debugging and phase testing.

Start the server:

```bash
make server
./out/server
```

Then open:

```text
test.html
```

Use this entry point for:

1. Debugging a single `GameRoom`.
2. Testing the `GameRoom -> RoundController -> PhaseHandler` rules flow.
3. Quickly testing Envision, Traverse, ADOPT actions, board state, feedback track, disruption cards, and snapshots.
4. Switching between direct handler mode and round controlled mode.

| Mode | Endpoint | Description |
|---|---|---|
| Direct handler mode | `/test/action` | Bypasses `RoundController` and invokes `PhaseHandler` directly. |
| Round controlled mode | `/action` | Goes through `GameRoom.receiveAction()` and `RoundController`. |

##### Notes

1. This is a debugging entry point, not a multiplayer connection layer.
2. `playerId` is still entered by the client, so it can simulate five players but does not represent five real network connections.
3. Direct handler mode should only be used for isolated phase-handler debugging.
4. Round controlled mode should be used to validate turn order, current-player enforcement, first-player changes, phase transitions, Traverse ordering, and ADOPT ordering.

---

#### 12.3.2 Room Connection Test Server: `room-server` and `room_test.html`

This entry point is designed for room connection and multiplayer synchronization testing.

Start the room test server:

```bash
make room-server
./out/room-server
```

Then open multiple browser windows:

```text
room_test.html
```

Recommended setup: open five windows, one for each client.

##### Test Flow

1. Click `Join room` in each window.
2. Confirm that the windows receive different player IDs, from `P0` through `P4`.
3. Click `Start room` from any joined window.
4. Send actions from the current player's window.
5. In the other windows, click `Poll messages` or `Refresh state` to observe synchronization.

##### This Entry Point Can Test

1. `Room` join flow.
2. `sessionId / connId -> playerId` binding.
3. `Room::onAction()` ignoring client-supplied `playerId` and using the player ID bound to the connection.
4. Non-current players being unable to advance the game state.
5. Current-player actions updating the shared `GameRoom`.
6. Room broadcast via message polling, namely `snapshot_broadcast`.
7. Five-player `RoundController` behavior inside a single `Room`.

##### Known Limitations

1. `room-server` is an HTTP and polling test server, not a WebSocket or SSE real-time server.
2. `Poll messages` manually pulls broadcast messages. The server does not push updates automatically.
3. There is no automatic disconnect detection when a browser tab is closed.
4. Reconnect recovery, host permissions, automatic start, and leave policies are not implemented yet.
5. `sessionId` is a temporary testing identity generated by `room-server`. It is not the same as `playerId`.

##### Room Test Pass Criteria

1. Five windows can join as `P0` through `P4`.
2. After start, `roomState` becomes `PLAYING`.
3. A non-current player cannot advance the turn.
4. A current player can perform a valid action, and the `currentPlayer` or phase updates correctly.
5. Other windows can receive the updated state through `Poll messages` or `Refresh state`.
6. Traverse follows `draw_disruption -> resolve_disruption -> walk_path`.
7. ADOPT rotates players through `resolve_feedback`. When Agora or `SOLVE_DISRUPTION` appears, the same player must resolve the disruption before the next player continues.

---

### 12.4 Testing & QA History

The following summarizes major testing activities completed during development.

#### Phase Integration and HTTP Testing

The Adapt Phase actions were integrated with disruption functionality through shared `GameUtility` methods. The feedback track process was also automated. Token bag population, token drawing, and track filling now happen automatically at the start of each Envision round instead of being handled as a first-player action.

Adapt Phase actions were tested through HTTP requests. JSON request format documentation was also added.

#### Envision Phase Bug Fixes

During HTTP testing of the Envision phase, issues were found and fixed in `set_course`.

1. In `move_people` mode, the people token can only be placed on outer hex edges, meaning sides with no neighbor in `layout.json`.
2. In `rotate` mode, a branch logic error was corrected. Rotation now prioritizes the `steps` parameter. If `steps` is missing, it falls back to `degree`.

#### Traverse Phase Testing and UI Fixes

The backend logic was mostly correct, but the test UI had incorrect side link mappings that caused display errors during Traverse testing. The Traverse test UI path display was optimized.

Small changes were also made to `GameRoom` and `GameState`.

1. `GET /state` and `POST /test/action` now include `sessionId` for frontend session differentiation.
2. Server restart clears expired path highlights.
3. `GameState::toJson` now includes `peopleToken` for visualizing people token positions.

#### Adapt Phase Deep Testing

Traverse and Adapt phases were tested in detail. Feedback resolution testing covered all six feedback token types.

Main fixes included:

1. Added 11-slot Adapt track with cursor indicator and token bag breakdown panel.
2. Fixed incorrect side number mappings, path connections, and tile colors.
3. Removed redundant `cancel_disruption` action from Adapt Phase.
4. Fixed `steer` to append tokens to the existing `tokenBag` instead of wiping the bag.
5. Fixed `resolve_feedback` so the token bag is only reset when empty.
6. Fixed `TRANSFORM_STACK` to use `getEffectiveType()` when toggling DevA and DevB.
7. Added `adapt.track` and `tokenBagBreakdown` to `GameState::toJson`.
8. Updated `rebuildTokenBag` to sync with `tokenManager`.

#### RoundController Testing

The `RoundController` was tested end-to-end, excluding role card assignment and full goal-triggered game-end mechanics.

The following flows were tested:

1. Phase sequencing.
2. Action handling.
3. Turn order.
4. Traverse ordering.
5. ADOPT feedback resolution.
6. Disruption continuation during ADOPT.

Main fixes included:

1. Fixed Envision token bag logic on phase entry.
2. Fixed action success flow so the controller advances to the next player after successful non-pass actions.
3. Fixed `set_course` rotation propagation.
4. Fixed first-player token transition into Traverse.
5. Enforced the `draw_disruption -> resolve_disruption -> walk_path` order.
6. Fixed people token physical position updates.
7. Removed invalid `draw_disruption` action from controlled ADOPT.
8. Fixed ADOPT player rotation.
9. Fixed Agora disruption continuation using pending disruption resolution tracking.

#### Room Connection Layer Testing

A multi-connection room test was performed using `room-http-server` and `room_test.html`.

Five browser windows joined as `P0` through `P4`. The server bound sessions to player IDs, room snapshots were broadcast to all connections, and action constraints were verified.

The following behaviours were tested:

1. Five-player room joining.
2. Session binding.
3. Room state synchronization.
4. Current-player action restrictions.
5. Snapshot broadcasting through polling.
6. Five-player `RoundController` behaviour inside a single room.

---

## 13. Known Limitations & Future Work

1. **Role Cards**: The role card system is partially prepared, but it is not integrated into the controlled game flow yet. The current RoundController does not include a role selection or role assignment stage at game start, so players cannot obtain or use role abilities during gameplay.
2. **Game End**: The goal-triggered victory mechanism is partially implemented (`isActiveGoalMet()` exists) but not fully tested end-to-end.
3. **Player Disconnect**: Graceful disconnect handling (auto-pass, first-player reassignment) is marked as TODO in `Room.cpp`.
4. **Room Cleanup**: Finished or empty rooms are not automatically removed from `RoomManager`.
5. **CatH Token Effect**: The `TOKEN` disruption effect (draw tokens into bag) has a TODO marker and is not fully implemented.
6. **State Rollback**: No undo/rollback mechanism exists for reverting to a previous game state.
7. **Complete Input Entry**: A unified input interface for sending full request sequences would simplify testing (noted as a small, easy change).
