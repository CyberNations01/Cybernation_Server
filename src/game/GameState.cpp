#include "game/GameState.hpp"
#include <algorithm>
#include <random>
#include <map>

namespace {
bool compareWithOp(int lhs, comparator op, int rhs) {
    switch (op) {
        case comparator::GT: return lhs > rhs;
        case comparator::GE: return lhs >= rhs;
        case comparator::EQ: return lhs == rhs;
        case comparator::LE: return lhs <= rhs;
        case comparator::LT: return lhs < rhs;
        case comparator::NE: return lhs != rhs;
        default: return false;
    }
}

bool isPosMatch(int tilePos, const std::optional<std::string>& pos) {
    if (!pos.has_value() || pos->empty()) return true;
    if (*pos == "inner") return tilePos == 0;
    if (*pos == "middle") return tilePos >= 1 && tilePos <= 6;
    if (*pos == "outer") return tilePos >= 7 && tilePos <= 10;
    return false;
}
}

GameState::GameState() {
    for (int i = 0; i < NUM_PLAYERS; ++i) {
        players[i] = Player(i);
    }
    players[0].setFirstPlayer(true);

    DataLoader loader;
    disruptionManager = CardManager<DisruptionCard>(loader.loadDisrupt("./data/disruption.json"));
    board = loader.loadTile("./data/layout.json");
    tileManager = CardManager<Tile>(board);
    goalManager = loader.loadDeck<Goal>("./data/goal.json");
    if (!goalManager.isDeckEmpty()) {
        currentGoal = goalManager.draw();
    }

    CardManager<Stack> allStackManager = loader.loadDeck<Stack>("./data/stack.json");
    std::vector<std::vector<Stack>> stackVector(4, std::vector<Stack>());
    std::map<int, Stack> stackById;
    for (const auto& s : allStackManager.getDeck()) {
        stackById[s.getId()] = s;
        switch (s.getType()) {
            case StackType::WILD: stackVector[0].push_back(s); break;
            case StackType::WASTE: stackVector[1].push_back(s); break;
            case StackType::DEV_A: stackVector[2].push_back(s); break;
            case StackType::DEV_B: stackVector[3].push_back(s); break;
            default: break;
        }
    }
    wildStackManager = CardManager<Stack>(stackVector[0]);
    wasteStackManager = CardManager<Stack>(stackVector[1]);
    devAStackManager = CardManager<Stack>(stackVector[2]);
    devBStackManager = CardManager<Stack>(stackVector[3]);

    disruptionManager.shuffle();

    int baseId[]    = {2, 9, 11, 1, 3, 4, 17, 12, 16, 22, 14};
    int overlayId[] = {26, -1, 42, -1, -1, -1, -1, -1, 35, -1, 41};
    peopleToken = {4, 4};

    for (int i = 0; i < NUM_TILE; ++i) {
        auto baseIt = stackById.find(baseId[i]);
        if (baseIt != stackById.end()) board[i].setBase(baseIt->second);
        if (overlayId[i] >= 0) {
            auto overlayIt = stackById.find(overlayId[i]);
            if (overlayIt != stackById.end()) board[i].setOverlay(overlayIt->second);
        }
    }

    rebuildTokenBag();
    fillFeedbackTrackFromBag();
    adaptTrack.clear();
    adaptCursor = 0;
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

TokenEffect GameState::mapStackTypeToFeedbackToken(StackType type) const{
    switch(type){
        case StackType::WILD:
            return TokenEffect::TURN_WILD;
        case StackType::WASTE:
            return TokenEffect::LOSE_COHESION;
        case StackType::DEV_A: // Works
            return TokenEffect::TURN_WASTE;
        case StackType::DEV_B: // Agora
            return TokenEffect::SOLVE_DISRUPTION;
        default:
            return TokenEffect::UNKNOWN;
    }
}

void GameState::rebuildTokenBag() {
    tokenBag.clear();
    // Rebuild bag from current board state by drawing matching tokens from reserve.
    for (const auto& tile : board) {
        TokenEffect token = mapStackTypeToFeedbackToken(tile.getEffectiveType());
        if (token == TokenEffect::UNKNOWN){
            continue;
        }
        if (pool.draw(token)){
            tokenBag.push_back(token);
        }
        // If reserve is empty for that token, we silently skip for now.
        // Later you can change this to logging or error handling.
    }

    // Shuffle bag
    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(tokenBag.begin(), tokenBag.end(), gen);
}

void GameState::fillFeedbackTrackFromBag(){
    for(int i = 0; i < FEEDBACK_TRACK_SIZE; ++i){
        if (!tokenBag.empty()){
            feedbackTrack[i] = tokenBag.back();
            tokenBag.pop_back();
        }else{
            feedbackTrack[i] = TokenEffect::UNKNOWN;
        }
    }
}

void GameState::syncTokenBagFromManager() {
    tokenBag = tokenManager.getBag();
}

void GameState::setTokenBag(const std::vector<TokenEffect>& nextBag) {
    tokenManager.setBag(nextBag);
    syncTokenBagFromManager();
}

bool GameState::isActiveGoalMet() const {
    const auto& conditions = currentGoal.getConditions();
    if (conditions.empty()) return false;

    for (const auto& cond : conditions) {
        int lhs = 0;
        if (cond.type == "Wild") {
            for (const auto& tile : board) {
                if (tile.getEffectiveType() == StackType::WILD &&
                    isPosMatch(tile.getPosition(), cond.position)) {
                    ++lhs;
                }
            }
        } else if (cond.type == "Waste") {
            for (const auto& tile : board) {
                if (tile.getEffectiveType() == StackType::WASTE &&
                    isPosMatch(tile.getPosition(), cond.position)) {
                    ++lhs;
                }
            }
        } else if (cond.type == "DevA") {
            for (const auto& tile : board) {
                if (tile.getEffectiveType() == StackType::DEV_A &&
                    isPosMatch(tile.getPosition(), cond.position)) {
                    ++lhs;
                }
            }
        } else if (cond.type == "DevB") {
            for (const auto& tile : board) {
                if (tile.getEffectiveType() == StackType::DEV_B &&
                    isPosMatch(tile.getPosition(), cond.position)) {
                    ++lhs;
                }
            }
        } else if (cond.type == "Co" || cond.type == "Cohesion") {
            lhs = params.getCohesion();
        } else if (cond.type == "Cy" || cond.type == "Cybernation") {
            lhs = params.getCybernationLevel();
        } else if (cond.type == "HR" || cond.type == "HumanRelation") {
            lhs = params.getHumanRelation();
        } else if (cond.type == "Env" || cond.type == "Environment") {
            lhs = params.getEnvironment();
        } else if (cond.type == "Tech" || cond.type == "Technology") {
            lhs = params.getTechnology();
        } else {
            return false;
        }

        if (!compareWithOp(lhs, cond.op, cond.num)) return false;
    }
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
    j["activeGoal"] = {
        {"id", currentGoal.getId()},
        {"name", currentGoal.getName()},
        {"met", isActiveGoalMet()}
    };

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
        {"complete", (!adaptTrack.empty() && adaptCursor >= static_cast<int>(adaptTrack.size()))}
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
