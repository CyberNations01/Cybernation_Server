#include "game/GameState.hpp"
#include <algorithm>
#include <fstream>
#include <random>

namespace {
TokenEffect tokenFromStackType(StackType type) {
    switch (type) {
        case StackType::WILD:  return TokenEffect::TURN_WILD;
        case StackType::WASTE: return TokenEffect::LOSE_COHESION;
        case StackType::DEV_A: return TokenEffect::TURN_WASTE;       // Works feedback
        case StackType::DEV_B: return TokenEffect::SOLVE_DISRUPTION; // Agora feedback
        case StackType::UNKNOWN:
        default: return TokenEffect::UNKNOWN;
    }
}

StackType stackTypeFromGoalKey(const std::string& key) {
    if (key == "Wild") return StackType::WILD;
    if (key == "Waste") return StackType::WASTE;
    if (key == "DevA") return StackType::DEV_A;
    if (key == "DevB") return StackType::DEV_B;
    return StackType::UNKNOWN;
}

bool compareWithOp(int lhs, const std::string& op, int rhs) {
    if (op == "GT") return lhs > rhs;
    if (op == "GE") return lhs >= rhs;
    if (op == "EQ") return lhs == rhs;
    if (op == "LE") return lhs <= rhs;
    if (op == "LT") return lhs < rhs;
    if (op == "NE") return lhs != rhs;
    return false;
}
}

GameState::GameState() {
    // Initialize players
    for (int i = 0; i < NUM_PLAYERS; ++i) {
        players[i] = Player(i);
    }
    players[0].setFirstPlayer(true);

    // Load static resource 
    DataLoader loader;

    this->disruptionCatalog   = loader.loadDisrupt("./data/disruption.json");
    this->disruptionManager   = CardManager<DisruptionCard>(disruptionCatalog);
    this->stackManager        = loader.loadDeck<Stack>("./data/stack.json");
    this->board               = loader.loadTile("./data/layout.json");
    this->tileManager         = CardManager<Tile>(board);
    
    this->stackManager.shuffle();
    this->disruptionManager.shuffle();

    // Load and draw initial active goal card.
    {
        std::ifstream goalFile("./data/goal.json");
        if (goalFile.is_open()) {
            nlohmann::json goalJson;
            goalFile >> goalJson;
            if (goalJson.is_array()) {
                for (const auto& g : goalJson) goalDeck.push_back(g);
                if (!goalDeck.empty()) {
                    std::random_device rd;
                    std::mt19937 gen(rd());
                    std::shuffle(goalDeck.begin(), goalDeck.end(), gen);
                    activeGoal = goalDeck.front();
                }
            }
        }
    }


    // Test for setup

    std::map<int, Stack> stackById;
    while (!stackManager.isDeckEmpty()) {
        Stack s = stackManager.draw();
        stackById[s.getId()] = s;
        if (s.getType() == StackType::DEV_A) devATemplates.push_back(s);
        if (s.getType() == StackType::DEV_B) devBTemplates.push_back(s);
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
    resetAdaptState();
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
        TokenEffect t = tokenFromStackType(tile.getEffectiveType());
        if (t == TokenEffect::UNKNOWN) continue;
        // Reserve/pool is finite: if depleted, nothing is generated.
        if (pool.draw(t)) {
            tokenBag.push_back(t);
        }
    }
}

void GameState::resetAdaptState() {
    adaptTrack.clear();
    adaptCursor = 0;
    adaptPlacedOn.assign(NUM_TILE, -1);
    adaptCancelled.assign(NUM_TILE, false);
    adaptTileOccupied.assign(NUM_TILE, false);
}

bool GameState::initAdaptTrackIfNeeded() {
    if (!adaptTrack.empty()) return true;

    if (tokenBag.empty()) {
        rebuildTokenBag();
    }
    if (tokenBag.empty()) return false;

    adaptTrack = tokenBag;
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(adaptTrack.begin(), adaptTrack.end(), g);

    if (static_cast<int>(adaptTrack.size()) > NUM_TILE) {
        adaptTrack.resize(NUM_TILE);
    } else if (static_cast<int>(adaptTrack.size()) < NUM_TILE) {
        while (static_cast<int>(adaptTrack.size()) < NUM_TILE) {
            adaptTrack.push_back(TokenEffect::UNKNOWN);
        }
    }

    adaptCursor = 0;
    adaptPlacedOn.assign(NUM_TILE, -1);
    adaptCancelled.assign(NUM_TILE, false);
    adaptTileOccupied.assign(NUM_TILE, false);
    return true;
}

bool GameState::isAdaptComplete() const {
    return !adaptTrack.empty() && adaptCursor >= static_cast<int>(adaptTrack.size());
}

bool GameState::isAdaptTileAvailable(int tilePos) const {
    if (tilePos < 0 || tilePos >= NUM_TILE) return false;
    if (adaptTileOccupied.empty()) return true;
    return !adaptTileOccupied[tilePos];
}

Stack GameState::nextDevelopmentStack(StackType targetType) {
    std::vector<Stack>* templates = nullptr;
    int* index = nullptr;

    if (targetType == StackType::DEV_A) {
        templates = &devATemplates;
        index = &devATemplateIndex;
    } else if (targetType == StackType::DEV_B) {
        templates = &devBTemplates;
        index = &devBTemplateIndex;
    }

    if (!templates || templates->empty()) {
        Stack fallback;
        fallback.setType(targetType);
        return fallback;
    }

    Stack picked = (*templates)[(*index) % static_cast<int>(templates->size())];
    *index = ((*index) + 1) % static_cast<int>(templates->size());
    picked.setType(targetType);
    return picked;
}

int GameState::countStacksByType(StackType type) const {
    int count = 0;
    for (const auto& tile : board) {
        if (tile.getEffectiveType() == type) {
            ++count;
        }
    }
    return count;
}

const DisruptionCard* GameState::findDisruptionCardByName(const std::string& name) const {
    for (const auto& card : disruptionCatalog) {
        if (card.getName() == name) return &card;
    }
    return nullptr;
}

bool GameState::isActiveGoalMet() const {
    if (!activeGoal.is_object()) return false;
    if (!activeGoal.contains("victory_condition")) return false;
    const nlohmann::json& vc = activeGoal["victory_condition"];
    if (!vc.is_object()) return false;

    // 1) Stack-based conditions.
    if (vc.contains("stack") && vc["stack"].is_object()) {
        const auto& stackCond = vc["stack"];
        for (auto it = stackCond.begin(); it != stackCond.end(); ++it) {
            const std::string stackKey = it.key();
            const auto& cond = it.value();
            StackType targetType = stackTypeFromGoalKey(stackKey);
            if (targetType == StackType::UNKNOWN || !cond.is_object()) return false;

            const std::string op = cond.value("compare", cond.value("Compare", "EQ"));
            const int num = cond.value("num", 0);
            const std::string pos = cond.value("position", "");

            int count = 0;
            for (const auto& tile : board) {
                if (tile.getEffectiveType() != targetType) continue;
                const int p = tile.getPosition();
                bool posMatch = true;
                if (pos == "inner") posMatch = (p == 0);
                else if (pos == "middle") posMatch = (p >= 1 && p <= 6);
                else if (pos == "outer") posMatch = (p >= 7 && p <= 10);
                if (posMatch) ++count;
            }

            if (!compareWithOp(count, op, num)) return false;
        }
    }

    // 2) Resource conditions.
    auto checkParam = [&](const std::string& key, int currentVal) -> bool {
        if (!vc.contains(key) || !vc[key].is_object()) return true;
        const auto& cond = vc[key];
        const std::string op = cond.value("compare", cond.value("Compare", "EQ"));
        const int num = cond.value("num", 0);
        return compareWithOp(currentVal, op, num);
    };

    if (!checkParam("Co", params.getCohesion())) return false;
    if (!checkParam("Cy", params.getCybernationLevel())) return false;
    if (!checkParam("HR", params.getHumanRelation())) return false;
    if (!checkParam("Env", params.getEnvironment())) return false;
    if (!checkParam("Tech", params.getTechnology())) return false;

    return true;
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
    if (activeGoal.is_object()) {
        j["activeGoal"] = {
            {"id", activeGoal.value("id", -1)},
            {"name", activeGoal.value("name", "Unknown")},
            {"met", isActiveGoalMet()}
        };
    } else {
        j["activeGoal"] = {
            {"id", -1},
            {"name", "None"},
            {"met", false}
        };
    }

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
        {"develop",        pool.getDevelop()},
        {"transform",      pool.getTransform()},
        {"totalRemaining", pool.getPoolSize()}
    };

    j["adapt"] = {
        {"trackSize", static_cast<int>(adaptTrack.size())},
        {"cursor", adaptCursor},
        {"complete", isAdaptComplete()}
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
