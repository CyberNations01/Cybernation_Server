![alt text](image.png)



### Request Action JSON format

### `Envision Phase`

**1. Shift Power**
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

**2. Come Together**
```json
{
    "phase": "ENVISION",
    "playerId": 0,
    "type": "come_together"
}
```

**3. Prepare**
```json
{
    "phase": "ENVISION",
    "playerId": 0,
    "type": "prepare"
}
```

**4. Set Course**  
`move_people`: the people token may only be placed on a **map edge** — a hex **side** with no neighbor tile in `data/layout.json` (outer perimeter). Example: tile `3` has edge sides `1` and `3`; side `4` faces another tile and will fail.
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

**5. Connect**
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

**6. Steer**
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

**7. Feedback track (not an action)**  
At the start of each Envision round, before player actions, the server adds feedback tokens from the current board into the existing token bag (limited by the finite pool), shuffles the bag, and draws up to 11 `adaptTrack` slots from it. Drawn tokens leave the bag; whether they return later is decided by ADOPT cleanup rules. The client reads track state from `gameState` / snapshot; there is no `fill_track` request.

<br></br>

### `Traverse Phase`

Controlled flow enforces this order:
`draw_disruption` -> `resolve_disruption` -> `walk_path`.

After a successful `walk_path`, controller advances to `ADOPT` automatically.
`advance` is not used in controlled `TRAVERSE`.

**1. Draw Disruption Card**
```json
{
    "phase": "TRAVERSE",
    "playerId": 0,
    "type": "draw_disruption"
}
```

**2. Resolve Disruption Card**
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

**3. Walk People Token**
```json
{
    "phase": "TRAVERSE",
    "playerId": 0,
    "type": "walk_path"
}
```

**Fields for each disruption card Category**
```bash
CatA:              { "canceltiles": "1,3" }
CatB (none/res):   { "cancel": "1" }
CatB (stack):      { "canceltiles": "1,3" }
CatE:              { "cancel": "1" }
CatF:              { "HR": "2", "Tech": "2", "Env": "1" }
CatG:              { "effectIndex": "0,1,0" }
CatH:              { "effectIndex": "0", "ppl": "3,4" }
CatI:              { "targetTiles": "2,5" }
CatJ:              { "useOptional": "1" }
CatK:              { "src": "HR", "dst": "Tech", "amount": "1" }
```

<br></br>
### `Adapt / ADOPT Phase`

**1. Resolve feedback**
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

**2. Resolve disruption** (see Traverse Section 3 for `params` / Category fields; optional `disruption_name`, `times`, `decision` on same `type`)
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

In ADOPT, **`draw_disruption` is not a valid standalone action** in controlled flow.
Disruption draw happens automatically when `resolve_feedback` allows the `SOLVE_DISRUPTION` (Agora) token.
Then the same player must call **`resolve_disruption`** before the next player's `resolve_feedback`.
When feedback sequence completes, controller advances phase automatically.
`advance` is not used in controlled `ADOPT`.

Use **`resolve_disruption`** with **`"cancel": "1"`** in `params` when you need the same behaviour as the old Adopt-only alias (see Traverse Section 3).

<br></br>