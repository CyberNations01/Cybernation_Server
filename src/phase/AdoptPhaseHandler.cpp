#include "phase/AdoptPhaseHandler.hpp"
#include <array>
#include <queue>
#include <sstream>
#include <string>
#include <unordered_map>
#include "game/GameUtility.hpp"
#include "nlohmann/json.hpp"

namespace {
std::string stackTypeToLabel(StackType type) {
    switch (type) {
        case StackType::WILD: return "WILD";
        case StackType::WASTE: return "WASTE";
        case StackType::DEV_A: return "DEV_A";
        case StackType::DEV_B: return "DEV_B";
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

bool parseComparatorToken(const std::string& token, comparator& outOp) {
    if (token != "GT" && token != "GE" && token != "EQ" &&
        token != "LE" && token != "LT" && token != "NE") {
        return false;
    }
    outOp = strToComparator(token);
    return true;
}

bool evaluateComparatorTokens(const std::string& opToken, const std::string& rhsToken, int lhs) {
    comparator op = comparator::EQ;
    if (!parseComparatorToken(opToken, op)) return false;
    int rhs = 0;
    try {
        rhs = std::stoi(rhsToken);
    } catch (...) {
        return false;
    }
    return compareByComparator(lhs, op, rhs);
}

std::vector<std::string> splitByColon(const std::string& input) {
    std::vector<std::string> parts;
    std::stringstream ss(input);
    std::string segment;
    while (std::getline(ss, segment, ':')) {
        parts.push_back(segment);
    }
    return parts;
}

const Role* findRoleById(const GameState& state, int roleId) {
    if (roleId < 0) return nullptr;
    for (const auto& role : state.roleManager.getDeck()) {
        if (role.getId() == roleId) return &role;
    }
    for (const auto& role : state.roleManager.getDiscard()) {
        if (role.getId() == roleId) return &role;
    }
    return nullptr;
}

StackType parseRoleStateType(const std::string& stateLabel) {
    if (stateLabel == "Wilds") return StackType::WILD;
    if (stateLabel == "Wastes") return StackType::WASTE;
    if (stateLabel == "Works") return StackType::DEV_A;
    if (stateLabel == "Agora") return StackType::DEV_B;
    return StackType::UNKNOWN;
}

int countTilesByType(const GameState& state, StackType target) {
    int count = 0;
    for (const auto& tile : state.board) {
        if (tile.getEffectiveType() == target) ++count;
    }
    return count;
}

bool hasAgoraAdjacentToAtLeastTwoWilds(const GameState& state) {
    for (const auto& tile : state.board) {
        if (tile.getEffectiveType() != StackType::DEV_B) continue;
        int wildNeighborCount = 0;
        for (const auto& n : tile.getNeighbours()) {
            if (n.first < 0 || n.first >= GameState::NUM_TILE) continue;
            if (state.board[n.first].getEffectiveType() == StackType::WILD) ++wildNeighborCount;
        }
        if (wildNeighborCount >= 2) return true;
    }
    return false;
}

bool hasOuterToOuterPath(const GameState& state) {
    struct Node { int tile; int side; };
    auto encode = [](int tile, int side) { return tile * Tile::TILE_SIDES + side; };
    auto isOuter = [](int tilePos) { return tilePos >= 7 && tilePos <= 10; };

    const int nodeCount = GameState::NUM_TILE * Tile::TILE_SIDES;
    for (int outerStart = 7; outerStart <= 10; ++outerStart) {
        for (int startSide = 0; startSide < Tile::TILE_SIDES; ++startSide) {
            std::vector<bool> visited(nodeCount, false);
            std::queue<Node> q;
            q.push({outerStart, startSide});
            visited[encode(outerStart, startSide)] = true;

            while (!q.empty()) {
                Node current = q.front();
                q.pop();

                const Tile& tile = state.board[current.tile];
                Stack activeStack = tile.hasOverlay() ? tile.getOverlay() : tile.getBase();
                const int connectedSide = activeStack.getConnectedSide(current.side);
                if (connectedSide < 0 || connectedSide >= Tile::TILE_SIDES) continue;
                if (isOuter(current.tile) && current.tile != outerStart) return true;

                const int localNode = encode(current.tile, connectedSide);
                if (!visited[localNode]) {
                    visited[localNode] = true;
                    q.push({current.tile, connectedSide});
                }

                const auto& next = tile.getNeighbours()[connectedSide];
                const int nextTile = next.first;
                const int nextSide = next.second;
                if (nextTile < 0 || nextTile >= GameState::NUM_TILE ||
                    nextSide < 0 || nextSide >= Tile::TILE_SIDES) {
                    continue;
                }

                const int nextNode = encode(nextTile, nextSide);
                if (!visited[nextNode]) {
                    visited[nextNode] = true;
                    q.push({nextTile, nextSide});
                }
            }
        }
    }
    return false;
}

bool isRoleAgendaMet(const GameState& state, const Role& role, int ownerPlayerId) {
    const auto& conditions = role.getAgendaCondition();
    if (conditions.empty()) return false;

    for (const std::string& cond : conditions) {
        const std::vector<std::string> parts = splitByColon(cond);
        if (parts.empty()) return false;

        if (parts[0] == "Adjacency") {
            if (parts.size() != 4 || parts[1] != "Agora_with_adjacent_Wilds") return false;
            const int lhs = hasAgoraAdjacentToAtLeastTwoWilds(state) ? 2 : 0;
            if (!evaluateComparatorTokens(parts[2], parts[3], lhs)) return false;
            continue;
        }
        if (parts[0] == "CountState") {
            if (parts.size() < 3 || parts.size() > 4) return false;
            StackType stateType = parseRoleStateType(parts[1]);
            if (stateType == StackType::UNKNOWN) return false;
            const int count = countTilesByType(state, stateType);
            if (parts[2] == "EVEN") {
                if ((count % 2) != 0) return false;
            } else if (parts.size() == 4) {
                if (!evaluateComparatorTokens(parts[2], parts[3], count)) return false;
            } else {
                return false;
            }
            continue;
        }
        if (parts[0] == "BagTotal") {
            if (parts.size() != 3) return false;
            if (!evaluateComparatorTokens(parts[1], parts[2], static_cast<int>(state.tokenBag.size()))) return false;
            continue;
        }
        if (parts[0] == "GoalTrackDisruptions") {
            if (parts.size() != 3) return false;
            if (!evaluateComparatorTokens(parts[1], parts[2], state.goalTrackDisruptionCount)) return false;
            continue;
        }
        if (parts[0] == "MiddleStacks" && parts.size() == 2 && parts[1] == "AllSameState") {
            const StackType expected = state.board[1].getEffectiveType();
            for (int i = 2; i <= 6; ++i) {
                if (state.board[i].getEffectiveType() != expected) return false;
            }
            continue;
        }
        if (parts[0] == "PathExists" && parts.size() == 3 && parts[1] == "OuterToOuter") {
            const bool required = (parts[2] == "true");
            if (hasOuterToOuterPath(state) != required) return false;
            continue;
        }
        if (parts[0] == "VictoryCondition" && parts.size() == 2 && parts[1] == "NotAchieved") {
            if (state.isActiveGoalMet()) return false;
            continue;
        }
        if (parts[0] == "IsFirstPlayer" && parts.size() == 2) {
            const bool required = (parts[1] == "true");
            const Player* p = (ownerPlayerId >= 0 && ownerPlayerId < GameState::NUM_PLAYERS)
                ? &state.players[ownerPlayerId]
                : nullptr;
            if (((p != nullptr) && p->isFirstPlayer()) != required) return false;
            continue;
        }
        CyberParameter param;
        if (parts.size() == 3 && parseCyberParameter(parts[0], param)) {
            const int lhs = state.params.getParamAmount(param);
            if (!evaluateComparatorTokens(parts[1], parts[2], lhs)) return false;
            continue;
        }
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
    const bool turnLimitReached = state.currentRound >= state.maxRounds;
    nlohmann::json endingsByPlayer = nlohmann::json::array();
    int focalPlayerId = state.currentPlayerId;
    if (focalPlayerId < 0 || focalPlayerId >= GameState::NUM_PLAYERS) focalPlayerId = state.firstPlayerId;
    std::string endingName = "In Progress";
    int focalRoleId = -1;
    bool focalAgendaMet = false;

    for (int pid = 0; pid < GameState::NUM_PLAYERS; ++pid) {
        const int roleId = state.playerSelectedRoleId[pid];
        const Role* role = findRoleById(state, roleId);
        const bool agendaMet = (role != nullptr) && isRoleAgendaMet(state, *role, pid);
        std::string playerEnding = "In Progress";
        if (goalMet && agendaMet) playerEnding = "Full Victory";
        else if (goalMet && !agendaMet) playerEnding = "Selfless Victory";
        else if (!goalMet && agendaMet) playerEnding = "Selfish Victory";
        else if (!goalMet && !agendaMet && turnLimitReached) playerEnding = "Defeat";

        endingsByPlayer.push_back({
            {"playerId", pid},
            {"roleId", role ? role->getId() : -1},
            {"roleName", role ? role->getName() : "UNKNOWN"},
            {"roleAgendaMet", agendaMet},
            {"endingName", playerEnding}
        });
        if (pid == focalPlayerId) {
            endingName = playerEnding;
            focalRoleId = role ? role->getId() : -1;
            focalAgendaMet = agendaMet;
        }
    }

    if (goalMet || turnLimitReached) state.gameOver = true;
    nlohmann::json payload = {
        {"returnedToBag", returnedToBag},
        {"discardedToReserve", discarded},
        {"bagSize", static_cast<int>(state.tokenBag.size())},
        {"goalMet", goalMet},
        {"roleId", focalRoleId},
        {"roleAgendaMet", focalAgendaMet},
        {"turnLimitReached", turnLimitReached},
        {"gameOver", state.gameOver},
        {"nextAction", state.gameOver ? "END_GAME" : "START_NEW_TURN"},
        {"endingName", endingName},
        {"endingsByPlayer", endingsByPlayer}
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
    if (action.type == "put_feedback_token") return handlePutFeedbackToken(action, state);
    if (action.type == "resolve_feedback_token") return handleResolveFeedbackToken(action, state);
    if (action.type == "cancel_feedback_token") return handleCancelFeedbackToken(action, state);
    if (action.type == "commit") return handleCommit(action, state);
    return fail(ActionStatus::INVALID_ACTION, "Action '" + action.type + "' is not valid during Adopt phase");
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

    nlohmann::json payload = {
        {"cursor", state.adaptCursor},
        {"token", tokenEffectToStr(state.adaptTrack[state.adaptCursor])},
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
    if (!tile) return fail(ActionStatus::INVALID_TARGET, "Invalid target tile");

    switch (effect) {
        case TokenEffect::TURN_WILD:
            GameUtility::changeTileStack(state, tilePos, StackType::WILD);
            return ActionResult::success(ActionMessage("adapt_effect_applied", R"({"effect":"TURN_WILD"})"));
        case TokenEffect::TURN_WASTE:
            GameUtility::changeTileStack(state, tilePos, StackType::WASTE);
            return ActionResult::success(ActionMessage("adapt_effect_applied", R"({"effect":"TURN_WASTE"})"));
        case TokenEffect::LOSE_COHESION:
            state.params.adjustParam(CyberParameter::COHESION, -2);
            return ActionResult::success(ActionMessage("adapt_effect_applied", R"({"effect":"LOSE_COHESION","delta":-2})"));
        case TokenEffect::SOLVE_DISRUPTION: {
            ActionResult drawResult = GameUtility::drawDisruption(state);
            if (!drawResult.ok()) return fail(drawResult.status, drawResult.message.payload);
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
                    if (v == "DEV_B" || v == "AGORA") targetType = StackType::DEV_B;
                }
                GameUtility::changeTileStack(state, tilePos, targetType);
            }
            return ActionResult::success(ActionMessage(
                "adapt_effect_applied",
                nlohmann::json({{"effect", "DEVELOP_STACK"},
                                {"overlayType", tile->hasOverlay() ? stackTypeToLabel(tile->getOverlay().getType()) : "NONE"}}).dump()));
        case TokenEffect::TRANSFORM_STACK:
            if (tile->hasOverlay()) {
                StackType t = tile->getOverlay().getType();
                if (t == StackType::DEV_A) GameUtility::changeTileStack(state, tilePos, StackType::DEV_B);
                else if (t == StackType::DEV_B) GameUtility::changeTileStack(state, tilePos, StackType::DEV_A);
            }
            return ActionResult::success(ActionMessage("adapt_effect_applied", R"({"effect":"TRANSFORM_STACK"})"));
        default:
            return ActionResult::success(ActionMessage("adapt_effect_applied", R"({"effect":"UNKNOWN","ignored":true})"));
    }
}

ActionResult AdoptPhaseHandler::handleCommit(const Action& action, GameState& state) {
    (void)action;
    (void)state;
    return fail(ActionStatus::INVALID_ACTION, "commit is automatic after final resolve/cancel");
}
