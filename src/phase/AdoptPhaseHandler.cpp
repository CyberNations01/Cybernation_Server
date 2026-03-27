#include "phase/AdoptPhaseHandler.hpp"
#include <string>
#include <array>
#include <unordered_map>
#include "nlohmann/json.hpp"
#include "game/GameUtility.hpp"

namespace {
std::string stackTypeToLabel(StackType type) {
    switch (type) {
        case StackType::WILD: return "WILD";
        case StackType::WASTE: return "WASTE";
        case StackType::DEV_A: return "DEV_A";
        case StackType::DEV_B: return "DEV_B";
        case StackType::UNKNOWN:
        default: return "UNKNOWN";
    }
}

std::string ringLabelForCursor(int cursor) {
    if (cursor <= 0) return "INNER";
    if (cursor <= 6) return "MIDDLE";
    return "OUTER";
}

ActionResult fail(ActionStatus status, const std::string& reason) {
    return {status, ActionMessage("error", reason)};
}

struct AdaptRuntimeState {
    std::vector<TokenEffect> trackSnapshot;
    std::vector<int> placedOn;
    std::vector<bool> cancelled;
    std::vector<bool> tileOccupied;
    bool pendingFeedback = false;
    int pendingTile = -1;
    int pendingPlayerId = -1;
    int pendingCursor = -1;
};

std::unordered_map<const GameState*, AdaptRuntimeState> gAdaptRuntimeByState;

void resetRuntimeFromTrack(AdaptRuntimeState& runtime, const std::vector<TokenEffect>& track) {
    runtime.trackSnapshot = track;
    runtime.placedOn.assign(track.size(), -1);
    runtime.cancelled.assign(track.size(), false);
    runtime.tileOccupied.assign(GameState::NUM_TILE, false);
    runtime.pendingFeedback = false;
    runtime.pendingTile = -1;
    runtime.pendingPlayerId = -1;
    runtime.pendingCursor = -1;
}

AdaptRuntimeState& runtimeFor(GameState& state) {
    AdaptRuntimeState& runtime = gAdaptRuntimeByState[&state];
    if (runtime.trackSnapshot != state.adaptTrack) {
        resetRuntimeFromTrack(runtime, state.adaptTrack);
    } else if (runtime.tileOccupied.size() != static_cast<size_t>(GameState::NUM_TILE)) {
        runtime.tileOccupied.assign(GameState::NUM_TILE, false);
    } else if (state.adaptCursor == 0 && !state.adaptTrack.empty()) {
        bool hasStaleProgress = false;
        for (int pos : runtime.placedOn) {
            if (pos >= 0) { hasStaleProgress = true; break; }
        }
        if (!hasStaleProgress) {
            for (bool c : runtime.cancelled) {
                if (c) { hasStaleProgress = true; break; }
            }
        }
        if (!hasStaleProgress) {
            for (bool occ : runtime.tileOccupied) {
                if (occ) { hasStaleProgress = true; break; }
            }
        }
        if (hasStaleProgress && !runtime.pendingFeedback) {
            resetRuntimeFromTrack(runtime, state.adaptTrack);
        }
    }
    return runtime;
}

bool parsePositionParam(const Action& action, int& outPos) {
    auto it = action.params.find("position");
    if (it == action.params.end()) return false;
    try {
        outPos = std::stoi(it->second);
    } catch (...) {
        return false;
    }
    return true;
}

nlohmann::json finalizeAdaptCommit(GameState& state, AdaptRuntimeState& runtime) {
    std::array<bool, GameState::NUM_TILE> returnable{};
    returnable.fill(false);
    returnable[0] = true;
    for (int t = 7; t <= 10; ++t) returnable[t] = true;

    std::vector<TokenEffect> nextBag;
    int returnedToBag = 0;
    int discarded = 0;
    for (int i = 0; i < static_cast<int>(state.adaptTrack.size()); ++i) {
        const int tilePos = (i < static_cast<int>(runtime.placedOn.size())) ? runtime.placedOn[i] : -1;
        const bool cancelled = (i < static_cast<int>(runtime.cancelled.size())) ? runtime.cancelled[i] : true;
        if (tilePos < 0 || tilePos >= GameState::NUM_TILE) continue;

        if (!cancelled && returnable[tilePos]) {
            nextBag.push_back(state.adaptTrack[i]);
            ++returnedToBag;
        } else {
            state.pool.putBack(state.adaptTrack[i]);
            ++discarded;
        }
    }
    state.setTokenBag(nextBag);

    const bool goalMet = state.isActiveGoalMet();
    const bool turnLimitReached = false;
    if (goalMet) {
        state.gameOver = true;
    }

    nlohmann::json payload = {
        {"returnedToBag", returnedToBag},
        {"discardedToReserve", discarded},
        {"bagSize", static_cast<int>(state.tokenBag.size())},
        {"goalMet", goalMet},
        {"turnLimitReached", turnLimitReached},
        {"gameOver", state.gameOver},
        {"nextAction", state.gameOver ? "END_GAME" : "START_NEW_TURN"},
        {"outcomeResolution", "TODO_ROLE_AGENDA_NOT_ENABLED"}
    };

    resetRuntimeFromTrack(runtime, std::vector<TokenEffect>{});
    return payload;
}

}

bool AdoptPhaseHandler::isTileOccupiedForAdaptPrompt(const GameState& state, int tilePos) {
    if (tilePos < 0 || tilePos >= GameState::NUM_TILE) return true;
    auto it = gAdaptRuntimeByState.find(&state);
    if (it == gAdaptRuntimeByState.end()) return false;
    const auto& occupied = it->second.tileOccupied;
    if (occupied.empty() || static_cast<int>(occupied.size()) <= tilePos) return false;
    return occupied[tilePos];
}

ActionResult AdoptPhaseHandler::handle(const Action& action, GameState& state) {
    if (action.isPass()) {
        return ActionResult::success(ActionMessage("status", "Player passed"));
    }

    if (action.type == "put_feedback_token") {
        return handlePutFeedbackToken(action, state);
    }
    if (action.type == "resolve_feedback_token") {
        return handleResolveFeedbackToken(action, state);
    }
    if (action.type == "cancel_feedback_token") {
        return handleCancelFeedbackToken(action, state);
    }
    if (action.type == "commit") {
        return handleCommit(action, state);
    }

    return fail(ActionStatus::INVALID_ACTION,
                "Action '" + action.type + "' is not valid during Adopt phase");
}

bool AdoptPhaseHandler::ensureAdaptTrackInitialized(GameState& state) {
    if (!state.adaptTrack.empty()) {
        runtimeFor(state);
        return true;
    }

    if (state.tokenManager.getBag().empty()) {
        if (!state.tokenBag.empty()) {
            state.tokenManager.setBag(state.tokenBag);
            state.syncTokenBagFromManager();
        } else {
            state.rebuildTokenBag();
        }
    }
    if (state.tokenManager.getBag().empty()) return false;
    if (!state.tokenManager.drawTrackFromBag(GameState::NUM_TILE)) return false;

    state.adaptTrack = state.tokenManager.getTrack();
    state.syncTokenBagFromManager();
    state.adaptCursor = 0;
    runtimeFor(state);
    return true;
}

bool AdoptPhaseHandler::isAdaptComplete(const GameState& state) const {
    return !state.adaptTrack.empty() &&
           state.adaptCursor >= static_cast<int>(state.adaptTrack.size());
}

bool AdoptPhaseHandler::isAdaptTileAvailable(GameState& state, int tilePos) const {
    if (tilePos < 0 || tilePos >= GameState::NUM_TILE) return false;
    AdaptRuntimeState& runtime = runtimeFor(state);
    if (runtime.tileOccupied.empty()) return true;
    return !runtime.tileOccupied[tilePos];
}

ActionResult AdoptPhaseHandler::handlePutFeedbackToken(const Action& action, GameState& state) {
    if (!ensureAdaptTrackInitialized(state)) {
        return fail(ActionStatus::UNKNOWN_ERROR, "Adapt track could not be initialized");
    }
    if (isAdaptComplete(state)) {
        return fail(ActionStatus::INVALID_ACTION, "All Adapt feedback tokens were already processed");
    }

    AdaptRuntimeState& runtime = runtimeFor(state);
    if (runtime.pendingFeedback) {
        return fail(ActionStatus::INVALID_ACTION, "Current feedback token is already placed; resolve or cancel it first");
    }

    int targetTile = -1;
    if (!parsePositionParam(action, targetTile)) {
        return fail(ActionStatus::INVALID_ACTION, "put_feedback_token requires integer param: position");
    }
    if (!isValidTargetForCursor(state.adaptCursor, targetTile)) {
        return fail(ActionStatus::INVALID_TARGET, "position not valid for current Adapt slot");
    }
    if (!isAdaptTileAvailable(state, targetTile)) {
        return fail(ActionStatus::INVALID_TARGET, "position already occupied this Adapt phase");
    }

    runtime.pendingFeedback = true;
    runtime.pendingTile = targetTile;
    runtime.pendingPlayerId = action.playerId;
    runtime.pendingCursor = state.adaptCursor;

    const TokenEffect effect = state.adaptTrack[state.adaptCursor];
    nlohmann::json payload = {
        {"cursor", state.adaptCursor},
        {"token", tokenEffectToStr(effect)},
        {"position", targetTile},
        {"ring", ringLabelForCursor(state.adaptCursor)},
        {"nextStep", "resolve_feedback_token_or_cancel_feedback_token"}
    };
    return ActionResult::success(ActionMessage("adapt_feedback_token_put", payload.dump()));
}

ActionResult AdoptPhaseHandler::handleResolveFeedbackToken(const Action& action, GameState& state) {
    if (!ensureAdaptTrackInitialized(state)) {
        return fail(ActionStatus::UNKNOWN_ERROR, "Adapt track could not be initialized");
    }
    AdaptRuntimeState& runtime = runtimeFor(state);
    if (!runtime.pendingFeedback) {
        return fail(ActionStatus::INVALID_ACTION, "No pending feedback token; call put_feedback_token first");
    }
    if (runtime.pendingPlayerId != action.playerId) {
        return fail(ActionStatus::NOT_YOUR_TURN, "Only the placing player can resolve this feedback token");
    }
    if (runtime.pendingCursor != state.adaptCursor) {
        return fail(ActionStatus::INVALID_ACTION, "Pending feedback cursor is out of sync");
    }

    const int targetTile = runtime.pendingTile;
    const TokenEffect effect = state.adaptTrack[state.adaptCursor];
    ActionResult effectResult = applyFeedbackEffect(effect, targetTile, action, state);
    if (!effectResult.ok()) return effectResult;

    nlohmann::json appliedEffect = nullptr;
    if (effectResult.message.type == "adapt_effect_applied") {
        try {
            appliedEffect = nlohmann::json::parse(effectResult.message.payload);
        } catch (...) {
            appliedEffect = effectResult.message.payload;
        }
    }

    const int resolvedIndex = state.adaptCursor;
    runtime.cancelled[state.adaptCursor] = false;
    runtime.placedOn[state.adaptCursor] = targetTile;
    runtime.tileOccupied[targetTile] = true;
    runtime.pendingFeedback = false;
    runtime.pendingTile = -1;
    runtime.pendingPlayerId = -1;
    runtime.pendingCursor = -1;
    state.adaptCursor++;

    nlohmann::json payload = {
        {"resolvedIndex", resolvedIndex},
        {"token", tokenEffectToStr(effect)},
        {"targetTile", targetTile},
        {"decision", "allow"},
        {"effectApplied", appliedEffect},
        {"ring", ringLabelForCursor(resolvedIndex)},
        {"remaining", static_cast<int>(state.adaptTrack.size()) - state.adaptCursor},
        {"isComplete", isAdaptComplete(state)},
        {"cybernationLevel", state.params.getCybernationLevel()},
        {"cohesion", state.params.getCohesion()}
    };

    if (payload["isComplete"].get<bool>()) {
        payload["autoCommit"] = true;
        payload["commit"] = finalizeAdaptCommit(state, runtime);
    } else {
        payload["autoCommit"] = false;
    }

    return ActionResult::success(ActionMessage("adapt_feedback_resolved", payload.dump()));
}

ActionResult AdoptPhaseHandler::handleCancelFeedbackToken(const Action& action, GameState& state) {
    if (!ensureAdaptTrackInitialized(state)) {
        return fail(ActionStatus::UNKNOWN_ERROR, "Adapt track could not be initialized");
    }
    AdaptRuntimeState& runtime = runtimeFor(state);
    if (!runtime.pendingFeedback) {
        return fail(ActionStatus::INVALID_ACTION, "No pending feedback token; call put_feedback_token first");
    }
    if (runtime.pendingPlayerId != action.playerId) {
        return fail(ActionStatus::NOT_YOUR_TURN, "Only the placing player can cancel this feedback token");
    }
    if (runtime.pendingCursor != state.adaptCursor) {
        return fail(ActionStatus::INVALID_ACTION, "Pending feedback cursor is out of sync");
    }

    int position = -1;
    if (!parsePositionParam(action, position)) {
        return fail(ActionStatus::INVALID_ACTION, "cancel_feedback_token requires integer param: position");
    }
    if (position != runtime.pendingTile) {
        return fail(ActionStatus::INVALID_TARGET, "position mismatch with pending feedback token");
    }

    if (state.params.getCybernationLevel() < 1) {
        return fail(ActionStatus::INSUFFICIENT_RESOURCE, "Not enough cybernation to cancel feedback token");
    }
    state.params.adjustParam(CyberParameter::CYBERNATION_LEVEL, -1);

    const int cancelledIndex = state.adaptCursor;
    const TokenEffect effect = state.adaptTrack[state.adaptCursor];
    runtime.cancelled[state.adaptCursor] = true;
    runtime.placedOn[state.adaptCursor] = runtime.pendingTile;
    runtime.tileOccupied[runtime.pendingTile] = true;
    runtime.pendingFeedback = false;
    runtime.pendingTile = -1;
    runtime.pendingPlayerId = -1;
    runtime.pendingCursor = -1;
    state.adaptCursor++;

    nlohmann::json payload = {
        {"cancelledIndex", cancelledIndex},
        {"token", tokenEffectToStr(effect)},
        {"position", position},
        {"decision", "cancel"},
        {"remaining", static_cast<int>(state.adaptTrack.size()) - state.adaptCursor},
        {"isComplete", isAdaptComplete(state)},
        {"cybernationLevel", state.params.getCybernationLevel()}
    };

    if (payload["isComplete"].get<bool>()) {
        payload["autoCommit"] = true;
        payload["commit"] = finalizeAdaptCommit(state, runtime);
    } else {
        payload["autoCommit"] = false;
    }

    return ActionResult::success(ActionMessage("adapt_feedback_cancelled", payload.dump()));
}

bool AdoptPhaseHandler::isValidTargetForCursor(int cursor, int tilePos) const {
    if (cursor < 0 || cursor >= GameState::NUM_TILE) return false;
    if (tilePos < 0 || tilePos >= GameState::NUM_TILE) return false;
    if (cursor == 0) return tilePos == 0;
    if (cursor >= 1 && cursor <= 6) return tilePos >= 1 && tilePos <= 6;
    return tilePos >= 7 && tilePos <= 10;
}

ActionResult AdoptPhaseHandler::applyFeedbackEffect(TokenEffect effect, int tilePos, const Action& action, GameState& state) {
    Tile* tile = state.getTile(tilePos);
    if (!tile) {
        return fail(ActionStatus::INVALID_TARGET, "Invalid target tile");
    }

    switch (effect) {
        case TokenEffect::TURN_WILD: {
            GameUtility::changeTileStack(state, tilePos, StackType::WILD);
            return ActionResult::success(ActionMessage("adapt_effect_applied", R"({"effect":"TURN_WILD"})"));
        }
        case TokenEffect::TURN_WASTE: {
            GameUtility::changeTileStack(state, tilePos, StackType::WASTE);
            return ActionResult::success(ActionMessage("adapt_effect_applied", R"({"effect":"TURN_WASTE"})"));
        }
        case TokenEffect::LOSE_COHESION:
            state.params.adjustParam(CyberParameter::COHESION, -2);
            return ActionResult::success(ActionMessage("adapt_effect_applied", R"({"effect":"LOSE_COHESION","delta":-2})"));
        case TokenEffect::SOLVE_DISRUPTION: {
            ActionResult drawResult = GameUtility::drawDisruption(state);
            if (!drawResult.ok()) {
                return fail(drawResult.status, drawResult.message.payload);
            }
            nlohmann::json payload = {
                {"effect", "SOLVE_DISRUPTION"},
                {"drawn", true},
                {"disruptionName", state.activeDisruption.has_value() ? state.activeDisruption->getName() : "UNKNOWN"},
                {"cancellable", state.activeDisruption.has_value() ? state.activeDisruption->isCancellable() : false}
            };
            return ActionResult::success(ActionMessage("adapt_effect_applied", payload.dump()));
        }
        case TokenEffect::DEVELOP_STACK:
            if (!tile->hasOverlay()) {
                StackType targetType = StackType::DEV_A;
                auto it = action.params.find("develop_to");
                if (it != action.params.end()) {
                    const std::string& v = it->second;
                    if (v == "DEV_B" || v == "AGORA") {
                        targetType = StackType::DEV_B;
                    }
                }
                GameUtility::changeTileStack(state, tilePos, targetType);
            }
            {
                nlohmann::json payload = {
                    {"effect", "DEVELOP_STACK"},
                    {"overlayType", tile->hasOverlay() ? stackTypeToLabel(tile->getOverlay().getType()) : "NONE"}
                };
                return ActionResult::success(ActionMessage("adapt_effect_applied", payload.dump()));
            }
        case TokenEffect::TRANSFORM_STACK:
            if (tile->hasOverlay()) {
                Stack overlay = tile->getOverlay();
                StackType t = overlay.getType();
                if (t == StackType::DEV_A) {
                    GameUtility::changeTileStack(state, tilePos, StackType::DEV_B);
                } else if (t == StackType::DEV_B) {
                    GameUtility::changeTileStack(state, tilePos, StackType::DEV_A);
                }
            }
            return ActionResult::success(ActionMessage("adapt_effect_applied", R"({"effect":"TRANSFORM_STACK"})"));
        case TokenEffect::UNKNOWN:
        default:
            return ActionResult::success(ActionMessage("adapt_effect_applied", R"({"effect":"UNKNOWN","ignored":true})"));
    }
}

ActionResult AdoptPhaseHandler::handleCommit(const Action& action, GameState& state) {
    (void)action;
    (void)state;
    return fail(ActionStatus::INVALID_ACTION, "commit is automatic after final resolve/cancel");
}
