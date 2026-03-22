#include "game/GameRoom.hpp"
#include "phase/AdoptPhaseHandler.hpp"

namespace {
std::string tokenEffectToString(TokenEffect effect) {
    switch (effect) {
        case TokenEffect::TURN_WILD: return "TURN_WILD";
        case TokenEffect::LOSE_COHESION: return "LOSE_COHESION";
        case TokenEffect::TURN_WASTE: return "TURN_WASTE";
        case TokenEffect::SOLVE_DISRUPTION: return "SOLVE_DISRUPTION";
        case TokenEffect::DEVELOP_STACK: return "DEVELOP_STACK";
        case TokenEffect::TRANSFORM_STACK: return "TRANSFORM_STACK";
        case TokenEffect::UNKNOWN:
        default: return "UNKNOWN";
    }
}

std::string phaseToString(GamePhase phase) {
    switch (phase) {
        case GamePhase::ENVISION: return "ENVISION";
        case GamePhase::TRAVERSE: return "TRAVERSE";
        case GamePhase::ADOPT: return "ADOPT";
        default: return "UNKNOWN";
    }
}

std::string ringLabelForCursor(int cursor) {
    if (cursor <= 0) return "INNER";
    if (cursor <= 6) return "MIDDLE";
    return "OUTER";
}

bool isValidTargetForCursor(int cursor, int tilePos) {
    if (cursor < 0 || cursor >= GameState::NUM_TILE) return false;
    if (tilePos < 0 || tilePos >= GameState::NUM_TILE) return false;
    if (cursor == 0) return tilePos == 0;
    if (cursor >= 1 && cursor <= 6) return tilePos >= 1 && tilePos <= 6;
    return tilePos >= 7 && tilePos <= 10;
}

bool isAdaptComplete(const GameState& state) {
    return !state.adaptTrack.empty() &&
           state.adaptCursor >= static_cast<int>(state.adaptTrack.size());
}
}

GameRoom::GameRoom()
    : state(), controller() {}

ActionResult GameRoom::receiveAction(const Action& action) {
    return controller.processAction(action, state);
}

std::string GameRoom::getSnapshot() const {
    nlohmann::json combined;
    combined["gameState"]  = state.toJson();
    combined["controller"] = controller.toJson();
    combined["nextPrompt"] = buildNextPrompt();
    return combined.dump(2);
}

std::string GameRoom::getControllerSnapshot() const {
    return controller.toJson().dump(2);
}

nlohmann::json GameRoom::buildNextPrompt() const {
    nlohmann::json prompt = {
        {"phase", phaseToString(state.currentPhase)},
        {"currentPlayerId", state.currentPlayerId},
        {"allowedActions", nlohmann::json::array()},
        {"requiredParamsByAction", nlohmann::json::object()},
        {"uiHints", nlohmann::json::object()}
    };

    if (state.gameOver) {
        prompt["allowedActions"] = nlohmann::json::array({"none"});
        prompt["uiHints"] = {
            {"gameOver", true},
            {"reason", "Game already ended"}
        };
        return prompt;
    }

    if (state.currentPhase == GamePhase::ADOPT) {
        nlohmann::json allowed = nlohmann::json::array({"resolve_feedback", "cancel_disruption_effect", "trade"});
        if (isAdaptComplete(state)) {
            allowed.push_back("commit");
        }

        prompt["allowedActions"] = allowed;
        prompt["requiredParamsByAction"] = {
            {"resolve_feedback", nlohmann::json::array({"target_tile", "decision"})},
            {"cancel_disruption_effect", nlohmann::json::array({"disruption_name"})},
            {"trade", nlohmann::json::array({"source", "give_param", "receive_param"})},
            {"commit", nlohmann::json::array()}
        };

        nlohmann::json validTargets = nlohmann::json::array();
        if (!isAdaptComplete(state) && state.adaptCursor < static_cast<int>(state.adaptTrack.size())) {
            for (int tile = 0; tile < GameState::NUM_TILE; ++tile) {
                if (isValidTargetForCursor(state.adaptCursor, tile) &&
                    !AdoptPhaseHandler::isTileOccupiedForAdaptPrompt(state, tile)) {
                    validTargets.push_back(tile);
                }
            }
        }

        prompt["uiHints"] = {
            {"adapt", {
                {"cursor", state.adaptCursor},
                {"trackSize", static_cast<int>(state.adaptTrack.size())},
                {"complete", isAdaptComplete(state)},
                {"ring", isAdaptComplete(state) ? "NONE" : ringLabelForCursor(state.adaptCursor)},
                {"nextToken", (isAdaptComplete(state) || state.adaptCursor >= static_cast<int>(state.adaptTrack.size()))
                    ? "NONE"
                    : tokenEffectToString(state.adaptTrack[state.adaptCursor])},
                {"validTargets", validTargets},
                {"developChoiceRequired", (!isAdaptComplete(state)
                    && state.adaptCursor < static_cast<int>(state.adaptTrack.size())
                    && state.adaptTrack[state.adaptCursor] == TokenEffect::DEVELOP_STACK)},
                {"developChoices", nlohmann::json::array({"DEV_A", "DEV_B"})}
            }},
            {"cohesion", state.params.getCohesion()},
            {"cybernationLevel", state.params.getCybernationLevel()}
        };
        return prompt;
    }

    // Keep prompts minimal for non-Adapt phases since their action contracts are still evolving.
    prompt["allowedActions"] = nlohmann::json::array({"pass"});
    prompt["requiredParamsByAction"] = {
        {"pass", nlohmann::json::array()}
    };
    prompt["uiHints"] = {
        {"note", "Non-Adapt action prompts are not finalized yet"}
    };
    return prompt;
}
