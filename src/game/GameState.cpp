#include "game/GameState.hpp"
#include <algorithm>
#include <random>
#include <map>

namespace {
    bool isPosMatch(int tilePos, const std::optional<std::string>& pos)
    {
        if (!pos.has_value() || pos->empty()) return true;
        if (*pos == "inner") return tilePos == 0;
        if (*pos == "middle") return tilePos >= 1 && tilePos <= 6;
        if (*pos == "outer") return tilePos >= 7 && tilePos <= 10;
        return false;
    }
}

void GameState::randomizeBoard()
{
    std::vector<CardManager<Stack>*> managers = {
        &wildStackManager, &wasteStackManager, 
        &devAStackManager, &devBStackManager
    };

    for (auto* m : managers)
        m->shuffle();

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, 3);

    for (int i = 0; i < NUM_TILE; ++i) {
        int type = dist(gen);
        board[i].setBase(managers[type]->draw());
    }
}

GameState::GameState() {
    for (int i = 0; i < NUM_PLAYERS; ++i)
        players[i] = Player(i);
    players[0].setFirstPlayer(true);

    DataLoader loader;
    disruptionManager = CardManager<DisruptionCard>(loader.loadDisrupt("./data/disruption.json"));
    board = loader.loadTile("./data/layout.json");
    goalManager = loader.loadDeck<Goal>("./data/goal.json");
    if (!goalManager.isDeckEmpty())
        currentGoal = goalManager.draw();

    CardManager<Stack> allStackManager = loader.loadDeck<Stack>("./data/stack.json");
    std::map<StackType, std::vector<Stack>> stacksByType;
    for (const auto& s : allStackManager.getDeck())
        stacksByType[s.getType()].push_back(s);

    wildStackManager  = CardManager<Stack>(stacksByType[StackType::WILD]);
    wasteStackManager = CardManager<Stack>(stacksByType[StackType::WASTE]);
    devAStackManager  = CardManager<Stack>(stacksByType[StackType::DEV_A]);
    devBStackManager  = CardManager<Stack>(stacksByType[StackType::DEV_B]);

    disruptionManager.shuffle();

    peopleToken = {4, 4};
    randomizeBoard();
    rebuildTokenBag();
    adaptTrack.clear();
    adaptCursor = 0;
}

Tile* GameState::getTile(int position)
{
    if (position < 0 || position >= NUM_TILE)
        return nullptr;
    return &board[position];
}

Player* GameState::getPlayer(int id)
{
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

void GameState::rebuildTokenBag()
{
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

void GameState::syncTokenBagFromManager()
{
    tokenBag = tokenManager.getBag();
}

void GameState::setTokenBag(const std::vector<TokenEffect>& nextBag)
{
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
    j["ignoreCohesionLossThisRound"] = ignoreCohesionLossThisRound;
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
            {"type",     stackTypeToStr(t.getEffectiveType())}
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

    if (activeDisruption.has_value()) {
        j["activeDisruption"] = {
            {"name",        activeDisruption->getName()},
            {"category",    activeDisruption->getCategory()},
            {"cancellable", activeDisruption->isCancellable()}
        };
    } else {
        j["activeDisruption"] = nullptr;
    }

    return j;
}

std::string GameState::snapshot() const {
    return toJson().dump(2);
}
