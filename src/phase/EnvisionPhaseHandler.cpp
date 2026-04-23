#include "phase/EnvisionPhaseHandler.hpp"

ActionResult EnvisionPhaseHandler::handle(const Action& action, GameState& state)
{
    // "pass" is valid in every phase — RoundController handles the pass 
    // mechanics, but we still return SUCCESS so it knows to proceed.

    if (action.type == "shift_power")
        return handleShiftPower(action, state);

    if (action.type == "come_together")
        return handleComeTogether(action, state);

    if (action.type == "prepare")
        return handlePrepare(action, state);

    if (action.type == "set_course")
        return handleSetCourse(action, state);

    if (action.type == "connect")
        return handleConnect(action, state);

    if (action.type == "steer")
        return handleSteer(action, state);

    return ActionResult::invalid("Unknown envision action: " + action.type);
}

bool EnvisionPhaseHandler::tryParseInt(const std::string& s, int& value) 
{
    try{
        size_t pos = 0;
        int parsed = std::stoi(s, &pos);
        if (pos != s.size()){
            return false;
        }
        value = parsed;
        return true;
    }catch(...){
        return false;
    }
}

/** 
 * Shit Power 
 * - Cost: 1 HR
 * - Gain: 1 Co
 */
ActionResult EnvisionPhaseHandler::handleShiftPower(const Action& action, GameState& state)
{
    auto it = action.params.find("targetPlayerId");
    if (it == action.params.end())
        return ActionResult::invalid({"Envision: ","Missing param: targetPlayerId"});

    int targetPlayerId = -1;
    if (!tryParseInt(it->second, targetPlayerId))
        return ActionResult::invalid({"Envision: ", "targetPlayerId must be an integer"});

    Player* target = state.getPlayer(targetPlayerId);
    Player* currFirst;

    if (!target)
        return {ActionStatus::INVALID_TARGET,{"Envision: ","Target player does not exist"}};

    /* Cost: 1 HR */ 
    int hr_cost = 1;
    if (state.params.getParamAmount(CyberParameter::HUMAN_RELATION) < hr_cost)
        return {ActionStatus::INSUFFICIENT_RESOURCE, {"Envision: ", "Not enough People Relationship"}};

    /* Update First Player */
    currFirst = state.getPlayer(state.findFirstPlayer());
    if (currFirst) {

        currFirst->setFirstPlayer(false);
        target->setFirstPlayer(true);

        /* Apply action cost */
        state.params.adjustParam(CyberParameter::HUMAN_RELATION, -hr_cost);
    } else
        return ActionResult::invalid({"Envision: ", "BUG_ON! Current first player is null"});

    return ActionResult::success({"Envision: ","First-player token shifted to player" + std::to_string(targetPlayerId)});
}

/* Come Together Cost: 1 Env */
ActionResult EnvisionPhaseHandler::handleComeTogether(const Action& action, GameState& state)
{
    int ct_cost = 1;
    int curr_env = state.params.getParamAmount(CyberParameter::ENVIRONMENT);
    if (curr_env < ct_cost)
        return ActionResult::invalid({"Envision: ", "Not enough environment relationship for come_together action"});

    // Cost 1 environment, gain 1 cohesion
    state.params.adjustParam(CyberParameter::ENVIRONMENT, -ct_cost);
    state.params.adjustParam(CyberParameter::COHESION, 1);

    return {ActionResult::success({"Envision: ", "Spent " + std::to_string(ct_cost) + 
                                  " environment relationship to increase cohesion by 1"})};
}

/** 
 * Prepare: 
 * - Cost: 2 Hr
 * - Gain: 1 Cy
 */
ActionResult EnvisionPhaseHandler::handlePrepare(const Action& action, GameState& state)
{
    int totalCost = 2;
    int currentPeople = state.params.getParamAmount(CyberParameter::HUMAN_RELATION);
    if (currentPeople < totalCost)
        return ActionResult::invalid("Not enough people relationship for Prepare");

    // Cost 2 HR relationships, gain 1 cybernation level
    state.params.adjustParam(CyberParameter::HUMAN_RELATION, -totalCost);
    state.params.adjustParam(CyberParameter::CYBERNATION_LEVEL, 1);

    return {ActionResult::success({"Envision: ","Spent " + std::to_string(totalCost) +
                                   " people relationships to increase cybernation level by 1"})};
}

/**
 * TODO: Set Cource: Move ppl token or Rotate Tiles
 */
ActionResult EnvisionPhaseHandler::handleSetCourse(const Action& action, GameState& state)
{
    auto modeIt = action.params.find("mode");
    if (modeIt == action.params.end())
        return ActionResult::invalid("Missing param:mode");
    
        const std::string& mode = modeIt-> second;

    
    if (mode != "move_people") {
        auto tileIt = action.params.find("tile");
        auto sideIt = action.params.find("side");

        if (tileIt == action.params.end())
            return ActionResult::invalid({"Envision: ", "Missing param: tile"});

        if (sideIt == action.params.end())
            return ActionResult::invalid({"Envision: ", "Missing param: side"});

        int targetTile = -1;
        int targetSide = -1;
        
        if (!tryParseInt(tileIt->second, targetTile))
            return ActionResult::invalid({"Envision: ", "tile must be an integer"});

        if (!tryParseInt(sideIt->second, targetSide))
            return ActionResult::invalid({"Envision: ", "tile must be an integer"});

        if (targetTile < 0 || targetTile >= static_cast<int>(state.board.size()))
            return {ActionStatus::INVALID_TARGET,{"Envision: ", "Target tile is our of range"}};

        if (targetSide < 0 || targetSide >= Tile::TILE_SIDES)
            return {ActionStatus::INVALID_TARGET, {"Envision: ", "Target side must be between 0 and 5"}};

        Tile* tile = state.getTile(targetTile);
        if (!tile)
            return {ActionStatus::INVALID_TARGET,{"Envision: ","Target tile does not exist"}};

        if (state.setPeopleToken({targetTile, targetSide})) {
            return ActionResult::success({
                "Envision: ",
                "People token moved to tile "+ std::to_string(targetTile) + 
                ", side " + std::to_string(targetSide)
            });
        }

        return ActionResult::invalid({"Envision: ", "Failed to set people token"}); 
    }

    /* TODO: Rotate Logic */
    else if (mode == "rotate") {
        auto tileIt = action.params.find("tile");
        if (tileIt == action.params.end()){
            return ActionResult::invalid("Missing param: targetTile");
        }
        int targetTile = -1;
        if (!tryParseInt(tileIt->second, targetTile)){
            return ActionResult::invalid("targetTile must be an integer");
        }
        if (targetTile < 0 || targetTile >= static_cast<int>(state.board.size())){
            return {ActionStatus::INVALID_TARGET, {"error", "Target tile is out of range"}};
        }

        Tile* tile = state.getTile(targetTile);
        if (tile == nullptr){
            return {ActionStatus::INVALID_TARGET, {"error", "Target tile does not exist"}};
        }

        int steps = 1;
        auto stepsIt = action.params.find("steps");
        if (stepsIt != action.params.end()) {
            if (!tryParseInt(stepsIt->second, steps))
                return ActionResult::invalid("steps must be an integer");
        }

        if (steps < 0)
            return ActionResult::invalid("steps must be non-negative");

        std::string direction = "clockwise";
        auto dirIt = action.params.find("direction");
        if (dirIt != action.params.end())
            direction = dirIt->second;

        int currentRotation = tile->getRotation();
        int normalizedSteps = steps % Tile::TILE_SIDES;
        int newRotation = currentRotation;

        if (direction == "clockwise")
            newRotation = (currentRotation + normalizedSteps) % Tile::TILE_SIDES;
        else if (direction == "counterclockwise")
            newRotation = (currentRotation - normalizedSteps + Tile::TILE_SIDES) % Tile::TILE_SIDES;
        else
            return ActionResult::invalid("direction must be clockwise or counterclockwise");

        tile->setRotation(newRotation);

        return ActionResult::success({
            "info",
            "Stack rotated on tile " + std::to_string(targetTile) +
            ", direction " + direction +
            ", steps " + std::to_string(normalizedSteps) +
            ", new rotation = " + std::to_string(newRotation)
        });
    }

    return ActionResult::invalid("Unsupported set_course mode");
}

/**
 * Connect： Trade 2 resources for 1 
 */
ActionResult EnvisionPhaseHandler::handleConnect(const Action& action, GameState& state)
{

    // We need 2 parameters from the front end: cost relationship type & gain relationship type
    if (action.params.find("cost") == action.params.end() ||
        action.params.find("gain") == action.params.end()) {
        return ActionResult::invalid({"Envision: ", "Required cost and gain parameters"});
    }

    CyberParameter cost, gain;
    if (!(parseCyberParameter(action.params.at("cost"), cost)))
        return ActionResult::invalid({"Envision: ", "Invalid cost parameters"});
    
    if (!(parseCyberParameter(action.params.at("gain"), gain)))
        return ActionResult::invalid({"Envision: ", "Invalid cost parameters"});
 
    // check if it is enough to pay 2 relationships.
    if (state.params.getParamAmount(cost) < 2) {
        return {
            ActionStatus::INSUFFICIENT_RESOURCE,
            {"error", "Not enough cost for CONNECT (need 2)"}
        };
    }

    state.params.adjustParam(cost, -2);
    state.params.adjustParam(gain, +1);

    return ActionResult::success({
        "info",
        "Spent 2 " + cyberParameterToLabel(cost) + " and gained 1 " + cyberParameterToLabel(gain)
    });
}

/**
 * Steer:
 * - Cost: 2 Env
 * - Gain: 
 *  - Put 1 feedback token into bags (from reserve)
 *  - Draw token from bags & choose whether discard or not  
 */
ActionResult EnvisionPhaseHandler::handleSteer(const Action& action, GameState& state)
{
    auto tokenIt = action.params.find("tokenType");
    if (tokenIt == action.params.end()) {
        return ActionResult::invalid("Missing param: tokenType");
    }

    TokenEffect effect = strToTokenEffect(tokenIt->second);
    if (effect == TokenEffect::UNKNOWN) {
        return ActionResult::invalid("Invalid tokenType");
    }

    if (!state.pool.draw(effect)) {
        return {
            ActionStatus::INSUFFICIENT_RESOURCE,
            {"Envision: ", "Selected feedback token is not available in reserve"}
        };
    }

    std::vector<TokenEffect> bag = state.tokenManager.getBag();
    bag.push_back(effect);
    state.tokenManager.setBag(bag);
    state.syncTokenBagFromManager();

    return ActionResult::success({
        "Envision: ",
        "Feedback token " + tokenEffectToStr(effect) + " added from reserve to bag"
    });
}
