#include "phase/AdoptPhaseHandler.hpp"
#include <string>
#include <array>
#include "nlohmann/json.hpp"

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

bool parseCyberParameter(const std::string& raw, CyberParameter& out) {
    if (raw == "HR" || raw == "HUMAN_RELATION") {
        out = CyberParameter::HUMAN_RELATION; return true;
    }
    if (raw == "ENV" || raw == "ENVIRONMENT") {
        out = CyberParameter::ENVIRONMENT; return true;
    }
    if (raw == "TECH" || raw == "TECHNOLOGY") {
        out = CyberParameter::TECHNOLOGY; return true;
    }
    if (raw == "CY" || raw == "CYBERNATION" || raw == "CYBERNATION_LEVEL") {
        out = CyberParameter::CYBERNATION_LEVEL; return true;
    }
    if (raw == "CO" || raw == "COHESION") {
        out = CyberParameter::COHESION; return true;
    }
    return false;
}

bool mapDisruptionEffectToCyberParameter(DisruptionEffect effect, CyberParameter& out) {
    switch (effect) {
        case DisruptionEffect::COHESION: out = CyberParameter::COHESION; return true;
        case DisruptionEffect::HUMAN_RELATION: out = CyberParameter::HUMAN_RELATION; return true;
        case DisruptionEffect::CYBERNATION: out = CyberParameter::CYBERNATION_LEVEL; return true;
        case DisruptionEffect::TECHNOLOGY: out = CyberParameter::TECHNOLOGY; return true;
        case DisruptionEffect::ENVIRONMENT: out = CyberParameter::ENVIRONMENT; return true;
        default: return false;
    }
}

int getParamValue(const Params& params, CyberParameter p) {
    switch (p) {
        case CyberParameter::COHESION: return params.getCohesion();
        case CyberParameter::CYBERNATION_LEVEL: return params.getCybernationLevel();
        case CyberParameter::HUMAN_RELATION: return params.getHumanRelation();
        case CyberParameter::ENVIRONMENT: return params.getEnvironment();
        case CyberParameter::TECHNOLOGY: return params.getTechnology();
        default: return 0;
    }
}

std::string cyberParameterToLabel(CyberParameter p) {
    switch (p) {
        case CyberParameter::COHESION: return "COHESION";
        case CyberParameter::CYBERNATION_LEVEL: return "CYBERNATION_LEVEL";
        case CyberParameter::HUMAN_RELATION: return "HUMAN_RELATION";
        case CyberParameter::ENVIRONMENT: return "ENVIRONMENT";
        case CyberParameter::TECHNOLOGY: return "TECHNOLOGY";
        default: return "UNKNOWN";
    }
}

bool canApplyParamDelta(const Params& params, CyberParameter p, int delta) {
    const int cur = getParamValue(params, p);
    const int next = cur + delta;
    if (next < 0 || next > 20) return false;

    // Relationship tracks are capped by cohesion.
    if (p == CyberParameter::HUMAN_RELATION ||
        p == CyberParameter::ENVIRONMENT ||
        p == CyberParameter::TECHNOLOGY) {
        if (next > params.getCohesion()) return false;
    }
    return true;
}

ActionResult fail(ActionStatus status, const std::string& reason) {
    return {status, ActionMessage("error", reason)};
}

}

ActionResult AdoptPhaseHandler::handle(const Action& action, GameState& state) {
    if (action.isPass()) {
        return ActionResult::success(ActionMessage("status", "Player passed"));
    }

    if (action.type == "resolve_feedback") {
        return handleResolveFeedback(action, state);
    }
    if (action.type == "cancel_disruption_effect") {
        return handleCancelDisruptionEffect(action, state);
    }
    if (action.type == "get_adapt_status") {
        return handleGetAdaptStatus(state);
    }

    if (action.type == "trade") {
        return handleTrade(action, state);
    }

    if (action.type == "commit") {
        return handleCommit(action, state);
    }

    return fail(ActionStatus::INVALID_ACTION,
                "Action '" + action.type + "' is not valid during Adopt phase");
}

ActionResult AdoptPhaseHandler::handleGetAdaptStatus(GameState& state) {
    if (!state.initAdaptTrackIfNeeded()) {
        return fail(ActionStatus::UNKNOWN_ERROR, "Adapt track could not be initialized");
    }

    int cursor = state.adaptCursor;
    nlohmann::json validTargets = nlohmann::json::array();
    if (!state.isAdaptComplete()) {
        for (int tile = 0; tile < GameState::NUM_TILE; ++tile) {
            if (isValidTargetForCursor(cursor, tile) && state.isAdaptTileAvailable(tile)) {
                validTargets.push_back(tile);
            }
        }
    }

    nlohmann::json payload = {
        {"cursor", cursor},
        {"trackSize", static_cast<int>(state.adaptTrack.size())},
        {"complete", state.isAdaptComplete()},
        {"nextToken", state.isAdaptComplete() ? "NONE" : tokenEffectToString(state.adaptTrack[cursor])},
        {"ring", state.isAdaptComplete() ? "NONE" : ringLabelForCursor(cursor)},
        {"developChoiceRequired",
            (!state.isAdaptComplete() && state.adaptTrack[cursor] == TokenEffect::DEVELOP_STACK)},
        {"developChoices", nlohmann::json::array({"DEV_A", "DEV_B"})},
        {"validTargets", validTargets},
        {"cohesion", state.params.getCohesion()},
        {"cybernationLevel", state.params.getCybernationLevel()}
    };

    return ActionResult::success(ActionMessage("adapt_status", payload.dump()));
}

ActionResult AdoptPhaseHandler::handleResolveFeedback(const Action& action, GameState& state) {
    if (!state.initAdaptTrackIfNeeded()) {
        return fail(ActionStatus::UNKNOWN_ERROR, "Adapt track could not be initialized");
    }

    if (state.isAdaptComplete()) {
        return ActionResult::success(ActionMessage("status", "All Adapt feedback tokens were already resolved"));
    }

    auto tileIt = action.params.find("target_tile");
    auto decisionIt = action.params.find("decision");
    if (tileIt == action.params.end() || decisionIt == action.params.end()) {
        return fail(ActionStatus::INVALID_ACTION, "resolve_feedback requires params: target_tile, decision");
    }

    int targetTile = -1;
    try {
        targetTile = std::stoi(tileIt->second);
    } catch (...) {
        return fail(ActionStatus::INVALID_TARGET, "target_tile must be an integer");
    }

    if (!isValidTargetForCursor(state.adaptCursor, targetTile)) {
        return fail(ActionStatus::INVALID_TARGET, "target_tile not valid for current Adapt slot");
    }

    if (!state.isAdaptTileAvailable(targetTile)) {
        return fail(ActionStatus::INVALID_TARGET, "target_tile already occupied this Adapt phase");
    }

    const std::string decision = decisionIt->second;
    const TokenEffect effect = state.adaptTrack[state.adaptCursor];

    nlohmann::json appliedEffect = nullptr;
    if (decision == "cancel") {
        if (state.params.getCybernationLevel() < 1) {
            return fail(ActionStatus::INSUFFICIENT_RESOURCE, "Not enough cybernation to cancel feedback");
        }
        state.params.adjustParam(CyberParameter::CYBERNATION_LEVEL, -1);
        state.adaptCancelled[state.adaptCursor] = true;
    } else if (decision == "allow") {
        ActionResult effectResult = applyFeedbackEffect(effect, targetTile, action, state);
        if (!effectResult.ok()) return effectResult;
        state.adaptCancelled[state.adaptCursor] = false;
        if (effectResult.message.type == "adapt_effect_applied") {
            try {
                appliedEffect = nlohmann::json::parse(effectResult.message.payload);
            } catch (...) {
                appliedEffect = effectResult.message.payload;
            }
        }
    } else {
        return fail(ActionStatus::INVALID_ACTION, "decision must be 'cancel' or 'allow'");
    }

    const int resolvedIndex = state.adaptCursor;
    state.adaptPlacedOn[state.adaptCursor] = targetTile;
    state.adaptTileOccupied[targetTile] = true;
    state.adaptCursor++;

    nlohmann::json payload = {
        {"resolvedIndex", resolvedIndex},
        {"token", tokenEffectToString(effect)},
        {"targetTile", targetTile},
        {"decision", decision},
        {"effectApplied", appliedEffect},
        {"ring", ringLabelForCursor(resolvedIndex)},
        {"remaining", static_cast<int>(state.adaptTrack.size()) - state.adaptCursor},
        {"isComplete", state.isAdaptComplete()},
        {"cybernationLevel", state.params.getCybernationLevel()},
        {"cohesion", state.params.getCohesion()}
    };
    return ActionResult::success(ActionMessage("adapt_feedback_resolved", payload.dump()));
}

ActionResult AdoptPhaseHandler::handleCancelDisruptionEffect(const Action& action, GameState& state) {
    auto nameIt = action.params.find("disruption_name");
    if (nameIt == action.params.end()) {
        return fail(ActionStatus::INVALID_ACTION, "cancel_disruption_effect requires disruption_name");
    }

    const DisruptionCard* card = state.findDisruptionCardByName(nameIt->second);
    if (!card) {
        return fail(ActionStatus::INVALID_TARGET, "Unknown disruption_name");
    }

    if (!card->isCancellable()) {
        return fail(ActionStatus::INVALID_ACTION, "This disruption does not provide a cancel option");
    }

    std::vector<std::pair<CyberParameter, int>> computedCosts;
    int times = 1;
    auto timesIt = action.params.find("times");
    if (timesIt != action.params.end()) {
        try {
            times = std::stoi(timesIt->second);
        } catch (...) {
            return fail(ActionStatus::INVALID_ACTION, "times must be integer");
        }
    }
    if (times <= 0) {
        return fail(ActionStatus::INVALID_ACTION, "times must be > 0");
    }

    if (card->getCancelCosts().empty()) {
        return fail(ActionStatus::INVALID_ACTION, "This disruption has no cancel cost");
    }

    for (const auto& cost : card->getCancelCosts()) {
        CyberParameter param;
        if (!mapDisruptionEffectToCyberParameter(cost.first, param)) {
            return fail(ActionStatus::INVALID_ACTION, "Unsupported cancel cost type on disruption card");
        }
        computedCosts.push_back({param, cost.second * times});
    }

    // Pre-check all costs before mutating any state.
    for (const auto& cost : computedCosts) {
        if (!canApplyParamDelta(state.params, cost.first, cost.second)) {
            return fail(ActionStatus::INSUFFICIENT_RESOURCE, "Insufficient resources for disruption cancel cost");
        }
    }

    nlohmann::json paid = nlohmann::json::array();
    for (const auto& cost : computedCosts) {
        int before = getParamValue(state.params, cost.first);
        state.params.adjustParam(cost.first, cost.second);
        int after = getParamValue(state.params, cost.first);
        paid.push_back({
            {"param", cyberParameterToLabel(cost.first)},
            {"delta", cost.second},
            {"before", before},
            {"after", after}
        });
    }

    nlohmann::json payload = {
        {"disruptionName", card->getName()},
        {"times", times},
        {"paidCosts", paid},
        {"cancelApplied", true}
    };
    return ActionResult::success(ActionMessage("disruption_effect_cancelled", payload.dump()));
}

bool AdoptPhaseHandler::isValidTargetForCursor(int cursor, int tilePos) const {
    if (cursor < 0 || cursor >= GameState::NUM_TILE) return false;
    if (tilePos < 0 || tilePos >= GameState::NUM_TILE) return false;

    // Slot 0 -> inner tile only.
    if (cursor == 0) return tilePos == 0;
    // Slots 1..6 -> middle ring (1..6).
    if (cursor >= 1 && cursor <= 6) return tilePos >= 1 && tilePos <= 6;
    // Slots 7..10 -> outer ring (7..10).
    return tilePos >= 7 && tilePos <= 10;
}

ActionResult AdoptPhaseHandler::applyFeedbackEffect(TokenEffect effect, int tilePos, const Action& action, GameState& state) {
    Tile* tile = state.getTile(tilePos);
    if (!tile) {
        return fail(ActionStatus::INVALID_TARGET, "Invalid target tile");
    }

    switch (effect) {
        case TokenEffect::TURN_WILD: {
            Stack base = tile->getBase();
            base.setType(StackType::WILD);
            tile->setBase(base);
            tile->removeOverlay();
            return ActionResult::success(ActionMessage("adapt_effect_applied", R"({"effect":"TURN_WILD"})"));
        }
        case TokenEffect::TURN_WASTE: {
            Stack base = tile->getBase();
            base.setType(StackType::WASTE);
            tile->setBase(base);
            tile->removeOverlay();
            return ActionResult::success(ActionMessage("adapt_effect_applied", R"({"effect":"TURN_WASTE"})"));
        }
        case TokenEffect::LOSE_COHESION:
            state.params.adjustParam(CyberParameter::COHESION, -2);
            return ActionResult::success(ActionMessage("adapt_effect_applied", R"({"effect":"LOSE_COHESION","delta":-2})"));
        case TokenEffect::SOLVE_DISRUPTION:
            // Keep disruption handling minimal for now, as requested.
            return ActionResult::success(ActionMessage("adapt_effect_applied", R"({"effect":"SOLVE_DISRUPTION","placeholder":true})"));
        case TokenEffect::DEVELOP_STACK:
            // If already developed, no effect.
            // If not, create DEV_A/DEV_B based on optional param:
            // develop_to: DEV_A|DEV_B|WORKS|AGORA (default DEV_A)
            if (!tile->hasOverlay()) {
                StackType targetType = StackType::DEV_A;
                auto it = action.params.find("develop_to");
                if (it != action.params.end()) {
                    const std::string& v = it->second;
                    if (v == "DEV_B" || v == "AGORA") {
                        targetType = StackType::DEV_B;
                    }
                }
                Stack overlay = state.nextDevelopmentStack(targetType);
                tile->setOverlay(overlay);
            }
            {
                nlohmann::json payload = {
                    {"effect", "DEVELOP_STACK"},
                    {"overlayType", tile->hasOverlay() ? stackTypeToLabel(tile->getOverlay().getType()) : "NONE"}
                };
                return ActionResult::success(ActionMessage("adapt_effect_applied", payload.dump()));
            }
        case TokenEffect::TRANSFORM_STACK:
            // Minimal implementation:
            // - If no development tile, no effect.
            // - If developed, toggle DEV_A <-> DEV_B with a real template stack.
            if (tile->hasOverlay()) {
                Stack overlay = tile->getOverlay();
                StackType t = overlay.getType();
                if (t == StackType::DEV_A) {
                    overlay = state.nextDevelopmentStack(StackType::DEV_B);
                    tile->setOverlay(overlay);
                } else if (t == StackType::DEV_B) {
                    overlay = state.nextDevelopmentStack(StackType::DEV_A);
                    tile->setOverlay(overlay);
                }
            }
            return ActionResult::success(ActionMessage("adapt_effect_applied", R"({"effect":"TRANSFORM_STACK"})"));
        case TokenEffect::UNKNOWN:
        default:
            return ActionResult::success(ActionMessage("adapt_effect_applied", R"({"effect":"UNKNOWN","ignored":true})"));
    }
}


ActionResult AdoptPhaseHandler::handleTrade(const Action& action, GameState& state) {
    // Trade is NOT a free Adapt action.
    // It is only allowed as a payment mechanic for disruption-cancel flows.
    auto sourceIt = action.params.find("source");
    if (sourceIt == action.params.end()) {
        return fail(ActionStatus::INVALID_ACTION,
                    "trade requires source=disruption_cancel (or disruption_effect_cancel)");
    }
    const std::string& source = sourceIt->second;
    if (source != "disruption_cancel" && source != "disruption_effect_cancel") {
        return fail(ActionStatus::INVALID_ACTION,
                    "trade is restricted to disruption cancel flows");
    }

    auto giveParamIt = action.params.find("give_param");
    auto recvParamIt = action.params.find("receive_param");
    if (giveParamIt == action.params.end() || recvParamIt == action.params.end()) {
        return fail(ActionStatus::INVALID_ACTION, "trade requires params: give_param, receive_param");
    }

    int giveAmount = 1;
    int recvAmount = 1;
    auto giveAmtIt = action.params.find("give_amount");
    auto recvAmtIt = action.params.find("receive_amount");
    if (giveAmtIt != action.params.end()) {
        try { giveAmount = std::stoi(giveAmtIt->second); } catch (...) { return fail(ActionStatus::INVALID_ACTION, "give_amount must be integer"); }
    }
    if (recvAmtIt != action.params.end()) {
        try { recvAmount = std::stoi(recvAmtIt->second); } catch (...) { return fail(ActionStatus::INVALID_ACTION, "receive_amount must be integer"); }
    }
    if (giveAmount <= 0 || recvAmount <= 0) {
        return fail(ActionStatus::INVALID_ACTION, "trade amounts must be > 0");
    }

    CyberParameter giveParam;
    CyberParameter recvParam;
    if (!parseCyberParameter(giveParamIt->second, giveParam) ||
        !parseCyberParameter(recvParamIt->second, recvParam)) {
        return fail(ActionStatus::INVALID_ACTION, "Unknown trade parameter");
    }
    if (giveParam == recvParam) {
        return fail(ActionStatus::INVALID_ACTION, "give_param and receive_param must differ");
    }

    // Cohesion is a cap and system health metric, not tradable currency.
    if (giveParam == CyberParameter::COHESION || recvParam == CyberParameter::COHESION) {
        return fail(ActionStatus::INVALID_ACTION, "Cohesion cannot be used in trade");
    }

    const int beforeGive = getParamValue(state.params, giveParam);
    const int beforeRecv = getParamValue(state.params, recvParam);
    if (beforeGive < giveAmount) {
        return fail(ActionStatus::INSUFFICIENT_RESOURCE, "Not enough resource to trade");
    }

    state.params.adjustParam(giveParam, -giveAmount);
    state.params.adjustParam(recvParam, recvAmount);

    const int afterGive = getParamValue(state.params, giveParam);
    const int afterRecv = getParamValue(state.params, recvParam);

    nlohmann::json payload = {
        {"giveParam", cyberParameterToLabel(giveParam)},
        {"giveAmount", giveAmount},
        {"receiveParam", cyberParameterToLabel(recvParam)},
        {"receiveAmount", recvAmount},
        {"before", {
            {"give", beforeGive},
            {"receive", beforeRecv}
        }},
        {"after", {
            {"give", afterGive},
            {"receive", afterRecv}
        }}
    };

    return ActionResult::success(ActionMessage("adapt_disruption_trade", payload.dump()));
}



ActionResult AdoptPhaseHandler::handleCommit(const Action& action, GameState& state) {
    (void)action;
    if (!state.initAdaptTrackIfNeeded()) {
        return fail(ActionStatus::UNKNOWN_ERROR, "Adapt track could not be initialized");
    }
    if (!state.isAdaptComplete()) {
        return fail(ActionStatus::INVALID_ACTION, "Cannot commit: Adapt feedback is not fully resolved");
    }

    // Adapt step 2:
    // Return face-up feedback tokens on INNER+OUTER stacks to bag.
    // Discard all others to reserve (not persisted in current data model).
    std::array<bool, GameState::NUM_TILE> returnable{};
    returnable.fill(false);
    returnable[0] = true;  // inner
    for (int t = 7; t <= 10; ++t) returnable[t] = true; // outer

    std::vector<TokenEffect> nextBag;
    int returnedToBag = 0;
    int discarded = 0;
    for (int i = 0; i < static_cast<int>(state.adaptTrack.size()); ++i) {
        const int tilePos = (i < static_cast<int>(state.adaptPlacedOn.size())) ? state.adaptPlacedOn[i] : -1;
        const bool cancelled = (i < static_cast<int>(state.adaptCancelled.size())) ? state.adaptCancelled[i] : true;
        if (tilePos < 0 || tilePos >= GameState::NUM_TILE) continue;

        if (!cancelled && returnable[tilePos]) {
            nextBag.push_back(state.adaptTrack[i]);
            ++returnedToBag;
        } else {
            // "Discard to reserve" means tokens return to the finite pool.
            state.pool.putBack(state.adaptTrack[i]);
            ++discarded;
        }
    }
    state.tokenManager.setBag(nextBag);
    state.tokenBag = state.tokenManager.getBag();

    // Adapt step 3 (simplified for current milestone):
    // Only check active Goal completion.
    // TODO: Re-introduce turn-track end condition and full game outcomes
    // once Role/Agenda pipeline is implemented.
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

    return ActionResult::success(ActionMessage("adapt_commit", payload.dump()));
}
