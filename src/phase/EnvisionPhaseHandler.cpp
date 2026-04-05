#include "phase/EnvisionPhaseHandler.hpp"

ActionResult EnvisionPhaseHandler::handle(const Action& action, GameState& state) {
    // "pass" is valid in every phase — RoundController handles the pass 
    // mechanics, but we still return SUCCESS so it knows to proceed.

    if (action.type == "shift_power") {
        return handleShiftPower(action, state);
    }
    if (action.type == "come_together") {
        return handleComeTogether(action, state);
    }
    if (action.type == "prepare") { 
        return handlePrepare(action, state);
    }
    if (action.type == "set_course") {
        return handleSetCourse(action, state);
    }
    if (action.type == "connect") {
        return handleConnect(action, state);
    }
    if (action.type == "steer") {
        return handleSteer(action, state);
    }

    if (action.type == "draw_feedback_track"){
        return handleDrawFeedbackTrack(action, state);
    }

    return ActionResult::invalid("Unknown envision action: " + action.type);
}
Player* EnvisionPhaseHandler::findPlayer(GameState& state, int playerId){
    return state.getPlayer(playerId);
}

bool EnvisionPhaseHandler::tryParseInt(const std::string& s, int& value){
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


ActionResult EnvisionPhaseHandler::handleShiftPower(const Action& action, GameState& state) {
    auto it = action.params.find("targetPlayerId");
    if (it == action.params.end()){
        return ActionResult::invalid("Missing param: targetPlayerId");
    }

    int targetPlayerId = -1;
    if (!tryParseInt(it->second, targetPlayerId)){
        return ActionResult::invalid("targetPlayerId must be an integer");
    }

    Player* target = findPlayer(state, targetPlayerId);
    if(target == nullptr){
        return {ActionStatus::INVALID_TARGET,{"error","Target player does not exist"}};
    }

    // Cost: one People Relationship + extra cost
    int extraCost = state.getCurrentAdditionalActionCost();
    int totalCost = 1 + extraCost;
    if (state.params.getParamAmount(CyberParameter::HUMAN_RELATION) < totalCost){
        return {ActionStatus::INSUFFICIENT_RESOURCE, {"error", "Not enough People Relationship"}};
    }

    // Consume the cost
    state.params.adjustParam(CyberParameter::HUMAN_RELATION, -totalCost);

    // Remove first-player token from current holder.
    int currentFirstPlayerId = state.findFirstPlayer();
    if(currentFirstPlayerId >= 0){
        Player* currentHolder = findPlayer(state, currentFirstPlayerId);
        if (currentHolder != nullptr){
            currentHolder->setFirstPlayer(false);
        }
    }

    // Give token to the target player.
    target->setFirstPlayer(true);
    state.firstPlayerId = targetPlayerId;

    return  ActionResult::success({"info","First-player token shifted to player" + std::to_string(targetPlayerId)});
}

ActionResult EnvisionPhaseHandler::handleComeTogether(const Action& action, GameState& state) {
    (void)action; 
    int extraCost = state.getCurrentAdditionalActionCost();
    int totalCost = 1+ extraCost;
    int currentEnvironment = state.params.getParamAmount(CyberParameter::ENVIRONMENT);
    if (currentEnvironment < totalCost){
        return ActionResult::invalid("Not enough environment relationship for come_together");
    }
    // Cost 1 environment, gain 1 cohesion
    state.params.adjustParam(CyberParameter::ENVIRONMENT,-totalCost);
    state.params.adjustParam(CyberParameter::COHESION,1);

    return {ActionResult::success({"info", "Spent " + std::to_string(totalCost) + " environment relationship to increase cohesion by 1"})};
}

ActionResult EnvisionPhaseHandler::handlePrepare(const Action& action, GameState& state) {
    (void)action; // not used currently
    int extraCost = state.getCurrentAdditionalActionCost();
    int totalCost = 2 + extraCost;
    int currentPeople = state.params.getParamAmount(CyberParameter::HUMAN_RELATION);
    if(currentPeople < totalCost){
        return ActionResult::invalid("Not enough people relationship for Prepare");
    }
    // Cost 2 people relationships, gain 1 cybernation level
    state.params.adjustParam(CyberParameter::HUMAN_RELATION, -totalCost);
    state.params.adjustParam(CyberParameter::CYBERNATION_LEVEL,1);

    return {ActionResult::success({"info","Spent " + std::to_string(totalCost) + " people relationships to increase cybernation level by 1"})};
}

ActionResult EnvisionPhaseHandler::handleSetCourse(const Action& action, GameState& state){
    auto modeIt = action.params.find("mode");
    if (modeIt == action.params.end()){
        return ActionResult::invalid("Missing param:mode");
    }
    const std::string& mode = modeIt-> second;

    // Minimal version: only support moving the people token
    if (mode != "move_people"){
        auto tileIt = action.params.find("targetTile");
        auto sideIt = action.params.find("targetSide");
        if (tileIt == action.params.end()){
            return ActionResult::invalid("Missing param: targetTile");
        }
        if (sideIt == action.params.end()){
            return ActionResult::invalid("Missing param: targetSide");
        }
        int targetTile = -1;
        int targetSide = -1;
        if(!tryParseInt(tileIt->second, targetTile)){
            return ActionResult::invalid("targetTile must be an integer");
        }
        if (!tryParseInt(sideIt->second, targetSide)){
            return ActionResult::invalid("targetSide must be an integer");
        }
        if(targetTile < 0 || targetTile >= static_cast<int>(state.board.size())){
            return {ActionStatus::INVALID_TARGET,{"error","Target tile is our of range"}};
        }
        if(targetSide<0 || targetSide >= Tile::TILE_SIDES){
            return {ActionStatus::INVALID_TARGET, {"error","Target side must be between 0 and 5"}};
        }

        Tile* tile = state.getTile(targetTile);
        if (tile == nullptr){
            return {ActionStatus::INVALID_TARGET,{"error","Target tile does not exist"}};
        }

        state.setPeopleToken({targetTile, targetSide});

        return ActionResult::success({
            "info",
            "People token moved to tile "+ std::to_string(targetTile) + 
            ", side " + std::to_string(targetSide)
        });
    }
    else if (mode == "rotate_stack"){
        auto tileIt = action.params.find("targettile");
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
            if (!tryParseInt(stepsIt->second, steps)) {
                return ActionResult::invalid("steps must be an integer");
            }
        }

        if (steps < 0) {
            return ActionResult::invalid("steps must be non-negative");
        }

        std::string direction = "clockwise";
        auto dirIt = action.params.find("direction");
        if (dirIt != action.params.end()) {
            direction = dirIt->second;
        }

        int currentRotation = tile->getRotation();
        int normalizedSteps = steps % Tile::TILE_SIDES;
        int newRotation = currentRotation;

        if (direction == "clockwise") {
            newRotation = (currentRotation + normalizedSteps) % Tile::TILE_SIDES;
        } else if (direction == "counterclockwise") {
            newRotation = (currentRotation - normalizedSteps + Tile::TILE_SIDES) % Tile::TILE_SIDES;
        } else {
            return ActionResult::invalid("direction must be clockwise or counterclockwise");
        }

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

ActionResult EnvisionPhaseHandler::handleConnect(const Action& action, GameState& state){
    auto costIt = action.params.find("costRelationshipType");
    auto gainIt = action.params.find("gainRelationshipType");

    // We need two parameter from the front end: cost relationship type & gain relationship type
    if (costIt == action.params.end() || gainIt == action.params.end()) {
        return ActionResult::success({
    "choice_required",
    R"({"action":"connect","costOptions":["technology","people","environment"],"gainOptions":["technology","people","environment"]})"
});
    }

    const std::string& costType = costIt->second;
    const std::string& gainType = gainIt->second;

    CyberParameter costParam;
    std::string costName;

    if (costType == "technology") {
        costParam = CyberParameter::TECHNOLOGY;
        costName = "Technology";
    } else if (costType == "people") {
        costParam = CyberParameter::HUMAN_RELATION;
        costName = "People Relationship";
    } else if (costType == "environment") {
        costParam = CyberParameter::ENVIRONMENT;
        costName = "Environment Relationship";
    } else {
        return ActionResult::invalid(
            "Invalid costRelationshipType. Must be one of: technology, people, environment"
        );
    }

    CyberParameter gainParam;
    std::string gainName;

    if (gainType == "technology") {
        gainParam = CyberParameter::TECHNOLOGY;
        gainName = "Technology";
    } else if (gainType == "people") {
        gainParam = CyberParameter::HUMAN_RELATION;
        gainName = "People Relationship";
    } else if (gainType == "environment") {
        gainParam = CyberParameter::ENVIRONMENT;
        gainName = "Environment Relationship";
    } else {
        return ActionResult::invalid(
            "Invalid gainRelationshipType. Must be one of: technology, people, environment"
        );
    }

    // check if it is enough to pay 2 relationships.
    int currentCostAmount = state.params.getParamAmount(costParam);
    if (currentCostAmount < 2) {
        return {
            ActionStatus::INSUFFICIENT_RESOURCE,
            {"error", "Not enough " + costName + " for CONNECT (need 2)"}
        };
    }

    state.params.adjustParam(costParam, -2);
    state.params.adjustParam(gainParam, +1);

    return ActionResult::success({
    "info",
    "Spent 2 " + costName + " and gained 1 " + gainName
});
}

ActionResult EnvisionPhaseHandler::handleSteer(const Action& action, GameState& state) {
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
            {"error", "Selected feedback token is not available in reserve"}
        };
    }

    std::vector<TokenEffect> bag = state.tokenManager.getBag();
    bag.push_back(effect);
    state.tokenManager.setBag(bag);
    state.syncTokenBagFromManager();

    return ActionResult::success({
        "info",
        "Feedback token " + tokenEffectToStr(effect) + " added from reserve to bag"
    });
}

ActionResult EnvisionPhaseHandler::handleDrawFeedbackTrack(const Action& action, GameState& state){
    (void) action;

    Player* current = state.getPlayer(state.currentPlayerId);
    if(current == nullptr){
        return ActionResult::invalid("Current player does not exist");
    }

    // Only the first player can draw the feedbacktokens.
    if (! current->isFirstPlayer()){
        return ActionResult::invalid("Only the first player can draw feedback tokens");
    }
    state.fillFeedbackTrackFromBag();

    return ActionResult::success({
        "info",
        "First player drew feedback tokens and filled the feedback track"
    });
}
