#include "game/RoundController.hpp"
#include "phase/EnvisionPhaseHandler.hpp"
#include "phase/TraversePhaseHandler.hpp"
#include "phase/AdoptPhaseHandler.hpp"

RoundController::RoundController(GameState& state)
{
    const auto& players = state.players;
    int playerId = -1;

    for (const auto& p: players) {
        if (p.isFirstPlayer()) {
            playerId = p.getId();
            break;
        }
    }
    
    firstPlayerId  = playerId < 0 ? 0 : playerId;
    buildTurnOrder(firstPlayerId);
}

ActionResult RoundController::processAction(const Action& action, GameState& state)
{
    if (action.playerId != currentPlayerId)
        return ActionResult::ignored();

    const auto& type = action.type;

    if (type == "pass") {
        if (currentPhase != GamePhase::ENVISION)
            return ActionResult::invalid({"RoundController", "Current phase cannot be passed"});
        
        int id = action.playerId;
        if (passedPlayers.find(id) == passedPlayers.end()) {

            passedPlayers.insert(id);
            if (advanceToNextActivePlayer()) {
                return ActionResult::success({"RoundController",
                                              "Player " + std::to_string(id) + " passed." +
                                              "Next player is " + std::to_string(currentPlayerId)});
            }
                
            else {
                advancePhase(state);
                return ActionResult::success({"RoundController",
                                              "Player " + std::to_string(id) + " passed." +
                                              "All players have passed"});
            }

        } else
            return ActionResult::ignored();
    }

    if (type == "advance") {
        if (!advancePhase(state))
            return ActionResult::invalid({"RoundController", "Phase is not advanced"});
        
        if (gameOver) {
            std::string reason = state.isActiveGoalMet() ? "Goal met!" : "Out of rounds";
            return ActionResult::success({"RoundController", "Game Over: " + reason});
        }

        return ActionResult::success({"RoundController", 
            "Phase advanced to " + gamePhaseToStr(currentPhase)});
    }

    PhaseHandler* handler = getHandlerForPhase(currentPhase);
    if (!handler)
        return ActionResult::invalid({"BUG", "No handler for current phase"});
    
    if (currentPhase == GamePhase::ENVISION) {
        if (!handleEnvision(state))
            return ActionResult::invalid({"RoundController", 
                                          "Maximum turn for Envision phase is reached." 
                                          "Rest of the players must passed."});
    }

    return handler->handle(action, state);;
}

PhaseHandler* RoundController::getHandlerForPhase(GamePhase phase)
{
    switch (phase) {
        case GamePhase::ENVISION: return &envisionHandler;
        case GamePhase::TRAVERSE: return &traverseHandler;
        case GamePhase::ADOPT:    return &adoptHandler;
    }
    return nullptr;
}

void RoundController::buildTurnOrder(int firstPlayerId)
{
    int id = firstPlayerId;

    for (size_t i = 0; i < turnOrder.size(); i++, id++)
        turnOrder[i] = id % GameState::NUM_PLAYERS;
    
    currentPlayerId = firstPlayerId;
}

void RoundController::resetRound(const GamePhase& nextPhase)
{
    buildTurnOrder(firstPlayerId);
    passedPlayers.clear();

    if (nextPhase == GamePhase::ENVISION) {
        envision_record.idx = 0;
        envision_record.turn = 0;
    }
}

bool RoundController::handleEnvision(GameState& state)
{
    if (envision_record.turn >= envision_record.maxTurn)
        return false;

    // Action Cost
    switch (envision_record.turn) {
        case 3:
        case 4:
            state.params.adjustParam(CyberParameter::TECHNOLOGY, -1);
            break;
        case 5:
            state.params.adjustParam(CyberParameter::TECHNOLOGY, -2);
            break;
        default:
            break;
    }

    int activePlayers = GameState::NUM_PLAYERS - static_cast<int>(passedPlayers.size());
    ++envision_record.idx;
    if (envision_record.idx >= activePlayers) {
        envision_record.idx = 0;
        ++envision_record.turn;
    }
    return true;
}

bool RoundController::advancePhase(GameState& state)
{
    switch (currentPhase) {
        case GamePhase::ENVISION:
            if (advanceToNextActivePlayer())
                return false;
            currentPhase = GamePhase::TRAVERSE;
            break;

        case GamePhase::TRAVERSE:
            currentPhase = GamePhase::ADOPT;
            break;
        
        case GamePhase::ADOPT:
            advanceRound(state);
            if (gameOver)
                return true;
            currentPhase = GamePhase::ENVISION;
            break;
    }

    buildTurnOrder(firstPlayerId);
    return true;
}

bool RoundController::advanceRound(GameState& state)
{
    if (state.isActiveGoalMet()) {
        gameOver = true;
        return true;
    }

    if (++currentRound == maxRounds) {
        gameOver = true;
        return false;
    }

    resetRound(GamePhase::ENVISION);
    return true;
}

bool RoundController::advanceToNextActivePlayer()
{
    if (passedPlayers.size() == GameState::NUM_PLAYERS)
        return false;

    int nextPlayerId = (currentPlayerId + 1) % GameState::NUM_PLAYERS;
    for (int i = 0; i < GameState::NUM_PLAYERS; i++) {
        if (passedPlayers.find(nextPlayerId) == passedPlayers.end()) {
            currentPlayerId = nextPlayerId;
            return true;
        }
        nextPlayerId = (nextPlayerId + 1) % GameState::NUM_PLAYERS;
    }
    return false;
}

nlohmann::json RoundController::toJson() const 
{
    return {
        {"round", currentRound},
        {"phase", gamePhaseToStr(currentPhase)},
        {"currentPlayerId", currentPlayerId},
        {"gameOver", gameOver}
    };
}