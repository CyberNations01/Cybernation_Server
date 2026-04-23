![alt text](image.png)



### Client request format

#### `Traverse Phase`

**1. Walk People Token**
```json
{
    "phase": "TRAVERSE",
    "playerId": 0,
    "type": "walk_path"
}
```

**2. Draw Disruption Card**
```json
{
    "phase": "TRAVERSE",
    "playerId": 0,
    "type": "draw_disruption"
}
```

**3. Resolve Disruption Card**
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



