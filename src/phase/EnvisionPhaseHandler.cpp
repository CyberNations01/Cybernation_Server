#include "phase/EnvisionPhaseHandler.hpp"
#include <algorithm>
#include "nlohmann/json.hpp"

ActionResult EnvisionPhaseHandler::handle(const Action& action, GameState& state) {
    // Initial role setup gate: only round 1 / Envision / before setup complete.
    if (state.currentRound == 1 &&
        state.currentPhase == GamePhase::ENVISION &&
        !state.roleSetupComplete) {
        return handleInitialRoleSetup(action, state);
    }

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

    return ActionResult::invalid("Unknown envision action: " + action.type);
}

ActionResult EnvisionPhaseHandler::handleInitialRoleSetup(const Action& action, GameState& state) {
    ensureInitialRoleOptions(state);

    if (action.type != "select_role") {
        return ActionResult::invalid("role setup in progress: only select_role is allowed");
    }
    if (action.playerId < 0 || action.playerId >= GameState::NUM_PLAYERS) {
        return ActionResult::invalid("invalid player id");
    }
    if (state.playerSelectedRoleId[action.playerId] >= 0) {
        return ActionResult::invalid("player already selected a role");
    }

    auto it = action.params.find("role_id");
    if (it == action.params.end()) {
        return ActionResult::invalid("select_role requires param: role_id");
    }

    int selectedRoleId = -1;
    if (!tryParseInt(it->second, selectedRoleId)) {
        return ActionResult::invalid("role_id must be an integer");
    }

    const auto& options = state.playerRoleOptions[action.playerId];
    if (std::find(options.begin(), options.end(), selectedRoleId) == options.end()) {
        return {ActionStatus::INVALID_TARGET, ActionMessage("error", "selected role_id is not in player's options")};
    }

    state.playerSelectedRoleId[action.playerId] = selectedRoleId;
    int selectedCount = 0;
    for (int pid = 0; pid < GameState::NUM_PLAYERS; ++pid) {
        if (state.playerSelectedRoleId[pid] >= 0) ++selectedCount;
    }
    if (selectedCount >= GameState::NUM_PLAYERS) {
        state.roleSetupComplete = true;
    }

    nlohmann::json payload = {
        {"playerId", action.playerId},
        {"selectedRoleId", selectedRoleId},
        {"selectedCount", selectedCount},
        {"totalPlayers", GameState::NUM_PLAYERS},
        {"setupComplete", state.roleSetupComplete}
    };
    return ActionResult::success(ActionMessage("role_selected", payload.dump()));
}

void EnvisionPhaseHandler::ensureInitialRoleOptions(GameState& state) {
    bool alreadyInitialized = true;
    for (const auto& options : state.playerRoleOptions) {
        if (options.size() < 2) {
            alreadyInitialized = false;
            break;
        }
    }
    if (alreadyInitialized) return;

    state.playerRoleOptions.assign(GameState::NUM_PLAYERS, std::vector<int>{});
    state.playerSelectedRoleId.fill(-1);
    state.roleSetupComplete = false;

    const auto& roleDeck = state.roleManager.getDeck();
    const int needed = GameState::NUM_PLAYERS * 2;
    if (static_cast<int>(roleDeck.size()) < needed) {
        return;
    }

    for (int pid = 0; pid < GameState::NUM_PLAYERS; ++pid) {
        state.playerRoleOptions[pid].push_back(roleDeck[pid * 2].getId());
        state.playerRoleOptions[pid].push_back(roleDeck[pid * 2 + 1].getId());
    }
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

    // Cost: one People Relationship
    if (state.params.getParamAmount(CyberParameter::HUMAN_RELATION) < 1){
        return {ActionStatus::INSUFFICIENT_RESOURCE, {"error", "Not enough People Relationship"}};
    }

    // Consume the cost
    state.params.adjustParam(CyberParameter::HUMAN_RELATION, -1);

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
    int currentEnvironment = state.params.getParamAmount(CyberParameter::ENVIRONMENT);
    if (currentEnvironment < 1){
        return ActionResult::invalid("Not enough environment relationship for come_together");
    }
    // Cost 1 environment, gain 1 cohesion
    state.params.adjustParam(CyberParameter::ENVIRONMENT,-1);
    state.params.adjustParam(CyberParameter::COHESION,1);

    return {ActionResult::success({"info", "Spent 1 environment relationship to increase cohesion by 1"})};
}

ActionResult EnvisionPhaseHandler::handlePrepare(const Action& action, GameState& state) {
    (void)action; // not used currently
    int currentPeople = state.params.getParamAmount(CyberParameter::HUMAN_RELATION);
    if(currentPeople < 2){
        return ActionResult::invalid("Not enough people relationship for Prepare");
    }
    // Cost 2 people relationships, gain 1 cybernation level
    state.params.adjustParam(CyberParameter::HUMAN_RELATION, -2);
    state.params.adjustParam(CyberParameter::CYBERNATION_LEVEL,1);

    return {ActionResult::success({"info","Spent 2 people relationships to increase cybernation level by 1"})};
}

ActionResult EnvisionPhaseHandler::handleSetCourse(const Action& action, GameState& state){
    auto modeIt = action.params.find("mode");
    if (modeIt == action.params.end()){
        return ActionResult::invalid("Missing param:mode");
    }
    const std::string& mode = modeIt-> second;

    // Minimal version: only support moving the people token
    if (mode != "move_people"){
        return ActionResult::invalid("Currently only set_course mode=move_people is supported");

    }

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

ActionResult EnvisionPhaseHandler::handleConnect(const Action& action, GameState& state){
    // To do: Gain one relationship of player's choice
    (void) action;
    (void) state;
    return ActionResult::invalid("connect is not implemented yet");
}

ActionResult EnvisionPhaseHandler::handleSteer(const Action& action, GameState& state){
    // To do: Cost 2 Environment Relationships to gain the effect.
    (void) action;
    (void) state;
    return ActionResult::invalid("steer is not implemented yet.");
}
