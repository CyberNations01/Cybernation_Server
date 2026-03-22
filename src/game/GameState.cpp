#include "game/GameState.hpp"

GameState::GameState() {
    // Initialize players
    for (int i = 0; i < NUM_PLAYERS; ++i) {
        players[i] = Player(i);
    }
    players[0].setFirstPlayer(true);

    // Load static resource 
    DataLoader loader;

    this->disruptionManager   = loader.loadDeck<DisruptionCard>("./data/disruption.json");
    this->board               = loader.loadTile("./data/layout.json");
    this->tileManager         = CardManager<Tile>(board);
    this->goalManager         = loader.loadDeck<Goal>("./data/goal.json");
    
    CardManager<Stack> stackManager = loader.loadDeck<Stack>("./data/stack.json");
    std::vector<std::vector<Stack>> stackVector(4, std::vector<Stack>());

    // ! Initialize each Stack type CardManager
    for (auto &e: stackManager.getDeck()) {
        switch (e.getType()) {
            case StackType::WILD:
                stackVector[0].push_back(e);
                break;
            case StackType::WASTE:
                stackVector[1].push_back(e);
                break;
            case StackType::DEV_A:
                break;
            case StackType::DEV_B:
                stackVector[3].push_back(e);
                break;
            default: break;
        }
    }

    wildStackManager  = CardManager<Stack>(stackVector[0]);
    wasteStackManager = CardManager<Stack>(stackVector[1]);
    devAStackManager  = CardManager<Stack>(stackVector[2]);
    devBStackManager  = CardManager<Stack>(stackVector[3]);
    
    this->disruptionManager.shuffle();

    // Test for setup

    std::map<int, Stack> stackById;
    while (!stackManager.isDeckEmpty()) {
        Stack s = stackManager.draw();
        stackById[s.getId()] = s;
    }

    int baseId[]    = {2,   9,  11,   1,   3,   4,  17,  12,  16,  22,  14};
    int overlayId[] = {26,  -1,  42,  -1,  -1,  -1,  -1,  -1,  35,  -1,  41};
    this->peopleToken = {4, 4};

    for (int i = 0; i < NUM_TILE; i++) {
        board[i].setBase(stackById[baseId[i]]);
        if (overlayId[i] >= 0) {
            board[i].setOverlay(stackById[overlayId[i]]);
        }
    }
    
    // Build initial token bag from board
    rebuildTokenBag();
}

Tile* GameState::getTile(int position) {
    for (auto& t : board) {
        if (t.getPosition() == position) return &t;
    }
    return nullptr;
}

Player* GameState::getPlayer(int id) {
    if (id < 0 || id >= NUM_PLAYERS) return nullptr;
    return &players[id];
}

void GameState::setPeopleToken(const std::pair<int, int>& pos)
{
    int tile = pos.first, side = pos.second;
    if (tile < 0 || side < 0 || tile >= NUM_TILE || side >= Tile::TILE_SIDES)
        return;
    
    Tile t = board[tile];
    // ! People Token must stand on the edge 
    if (t.getNeighbourTile(side) == -1) {
        peopleToken = pos;
    }
}
int GameState::findFirstPlayer() const
{
    for (int i = 0; i < NUM_PLAYERS; ++i) {
        if (players[i].isFirstPlayer()) return i;
    }
    return 0;
}

void GameState::rebuildTokenBag() {
    tokenBag.clear();
    for (const auto& tile : board) {
        tokenBag.push_back(static_cast<TokenEffect>(static_cast<int>(tile.getEffectiveType())));
    }
}

nlohmann::json GameState::toJson() const {
    nlohmann::json j;

    // Game progress
    j["round"] = {
        {"current", currentRound},
        {"max", maxRounds}
    };
    j["phase"]           = static_cast<int>(currentPhase);
    j["currentPlayerId"] = currentPlayerId;
    j["firstPlayerId"]   = firstPlayerId;
    j["gameOver"]        = gameOver;

    // Parameters
    j["params"] = {
        {"cohesion",         params.getCohesion()},
        {"cybernationLevel", params.getCybernationLevel()},
        {"humanRelation",    params.getHumanRelation()},
        {"environment",      params.getEnvironment()},
        {"technology",       params.getTechnology()}
    };

    // Board
    j["board"] = nlohmann::json::array();
    for (const auto& t : board) {
        j["board"].push_back({
            {"position", t.getPosition()},
            {"type",     t.getEffectiveType()}
        });
    }

    // Pool
    j["pool"] = {
        {"turnWild",       pool.getTurnWild()},
        {"loseCohesion",   pool.getLoseCohesion()},
        {"turnWaste",      pool.getTurnWaste()},
        {"solveDisruption", pool.getSolveDisrupt()},
        {"totalRemaining", pool.getPoolSize()}
    };

    // Players
    j["players"] = nlohmann::json::array();
    for (int i = 0; i < NUM_PLAYERS; ++i) {
        j["players"].push_back({
            {"id",           players[i].getId()},
            {"isFirstPlayer", players[i].isFirstPlayer()},
            {"handSize",     static_cast<int>(players[i].getHand().size())}
        });
    }

    return j;
}

std::string GameState::snapshot() const {
    return toJson().dump(2);
}
