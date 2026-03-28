#include <iostream>
#include <string>
#include <vector>
#include "nlohmann/json.hpp"
#include "game/GameRoom.hpp"
#include "core/Action.hpp"
#include "core/DisruptionCard.hpp"

namespace {
int gFailures = 0;

void expectTrue(bool cond, const std::string& label) {
    std::cout << (cond ? "[PASS] " : "[FAIL] ") << label << std::endl;
    if (!cond) ++gFailures;
}

int currentActor(const GameRoom& room) {
    int current = room.getController().getCurrentPlayerId();
    if (current < 0) return room.getState().firstPlayerId;
    return current;
}

Action makeAction(const GameRoom& room, const std::string& type) {
    Action a;
    a.playerId = currentActor(room);
    a.type = type;
    return a;
}

ActionResult dispatch(GameRoom& room, Action action, const std::string& label) {
    ActionResult result = room.receiveAction(action);
    std::cout << "[ACTION " << label << "] "
              << (result.ok() ? "OK" : "FAIL")
              << " status=" << static_cast<int>(result.status)
              << " type=" << result.message.type
              << " payload=" << result.message.payload << std::endl;
    return result;
}

void forceAdaptState(GameRoom& room, const std::vector<TokenEffect>& track) {
    auto& state = room.getState();
    state.currentPhase = GamePhase::ADOPT;
    state.gameOver = false;
    state.activeDisruption = std::nullopt;
    state.adaptTrack = track;
    state.adaptCursor = 0;
    state.tokenManager.clearTrack();
    state.setTokenBag(track);
}

ActionResult putToken(GameRoom& room, int position, const std::string& label) {
    Action a = makeAction(room, "put_feedback_token");
    a.params["position"] = std::to_string(position);
    return dispatch(room, a, label);
}

ActionResult resolveToken(GameRoom& room,
                          const std::string& label,
                          const std::unordered_map<std::string, std::string>& extraParams = {}) {
    Action a = makeAction(room, "resolve_feedback_token");
    for (const auto& [k, v] : extraParams) {
        a.params[k] = v;
    }
    return dispatch(room, a, label);
}

ActionResult cancelToken(GameRoom& room, int position, const std::string& label) {
    Action a = makeAction(room, "cancel_feedback_token");
    a.params["position"] = std::to_string(position);
    return dispatch(room, a, label);
}

ActionResult selectRole(GameRoom& room, int playerId, int roleId, const std::string& label) {
    Action a;
    a.playerId = playerId;
    a.type = "select_role";
    a.params["role_id"] = std::to_string(roleId);
    return dispatch(room, a, label);
}
}

int main() {
    GameRoom room;
    std::cout << "=== Adapt Branch Coverage Simulation ===" << std::endl;

    // --- 0) round-1 Envision role setup (parallel submission) ---
    expectTrue(room.getState().currentRound == 1, "role setup starts in round 1");
    expectTrue(room.getState().currentPhase == GamePhase::ENVISION, "role setup runs in ENVISION");
    expectTrue(!room.getState().roleSetupComplete, "role setup initially incomplete");

    Action badRoleAction;
    badRoleAction.playerId = 0;
    badRoleAction.type = "pass";
    ActionResult badRoleActionRes = dispatch(room, badRoleAction, "role_setup_bad_action");
    expectTrue(!badRoleActionRes.ok(), "non-select action rejected during role setup");

    bool optionsReady = true;
    for (int pid = 0; pid < GameState::NUM_PLAYERS; ++pid) {
        if (room.getState().playerRoleOptions[pid].size() != 2) {
            optionsReady = false;
            break;
        }
    }
    expectTrue(optionsReady, "each player receives exactly two role options");

    const int actorBeforeRoleSelect = currentActor(room);
    // Submit from non-current players first to verify parallel semantics.
    for (int pid = 1; pid < GameState::NUM_PLAYERS; ++pid) {
        const int roleId = room.getState().playerRoleOptions[pid][0];
        ActionResult roleRes = selectRole(room, pid, roleId, "role_select_non_current");
        expectTrue(roleRes.ok(), "non-current player can submit select_role");
    }
    expectTrue(currentActor(room) == actorBeforeRoleSelect,
               "role setup submissions do not advance round-controller turn");

    {
        const int dupRole = room.getState().playerRoleOptions[1][0];
        ActionResult dupRes = selectRole(room, 1, dupRole, "role_select_duplicate");
        expectTrue(!dupRes.ok(), "duplicate role submission by same player is rejected");
    }

    {
        const int roleIdP0 = room.getState().playerRoleOptions[0][0];
        ActionResult finalRoleRes = selectRole(room, 0, roleIdP0, "role_select_last_player");
        expectTrue(finalRoleRes.ok(), "last player can finish role setup");
    }
    expectTrue(room.getState().roleSetupComplete, "role setup completes after all players submit");

    // Temporary test bootstrap: mark role setup complete until
    // interactive parallel role selection is implemented outside GameRoom.
    expectTrue(room.getState().roleSetupComplete, "Role setup marked complete for tests");

    // Initialize controller + turn order quickly.
    while (room.getState().currentPhase == GamePhase::ENVISION) {
        Action pass = makeAction(room, "pass");
        dispatch(room, pass, "boot_envision_pass");
    }
    while (room.getState().currentPhase == GamePhase::TRAVERSE) {
        Action pass = makeAction(room, "pass");
        dispatch(room, pass, "boot_traverse_pass");
    }
    expectTrue(room.getState().currentPhase == GamePhase::ADOPT, "Entered ADOPT phase");

    // --- 1) snapshot contract check ---
    forceAdaptState(room, {TokenEffect::TURN_WILD});
    nlohmann::json snap = nlohmann::json::parse(room.getSnapshot());
    expectTrue(snap.contains("gameState"), "snapshot includes gameState");
    expectTrue(snap.contains("controller"), "snapshot includes controller");
    expectTrue(snap["gameState"]["phase"] == static_cast<int>(GamePhase::ADOPT),
               "snapshot gameState reports ADOPT");

    // --- 2) put_feedback_token validation branches ---
    forceAdaptState(room, {TokenEffect::TURN_WILD});
    Action missingParam = makeAction(room, "put_feedback_token");
    ActionResult missingParamRes = dispatch(room, missingParam, "put_missing_params");
    expectTrue(!missingParamRes.ok(), "put_feedback_token missing position fails");

    forceAdaptState(room, {TokenEffect::TURN_WILD});
    Action badPosition = makeAction(room, "put_feedback_token");
    badPosition.params["position"] = "abc";
    ActionResult badPositionRes = dispatch(room, badPosition, "put_bad_position_type");
    expectTrue(!badPositionRes.ok(), "put_feedback_token non-integer position fails");

    forceAdaptState(room, {TokenEffect::TURN_WILD});
    Action wrongRing = makeAction(room, "put_feedback_token");
    wrongRing.params["position"] = "1";
    ActionResult wrongRingRes = dispatch(room, wrongRing, "put_wrong_ring");
    expectTrue(!wrongRingRes.ok(), "put_feedback_token wrong ring fails");

    // --- 3) split-flow behavior: put keeps turn, resolve advances turn ---
    forceAdaptState(room, {TokenEffect::TURN_WILD, TokenEffect::TURN_WILD});
    int actorBeforePut = currentActor(room);
    ActionResult putRes = putToken(room, 0, "split_flow_put");
    expectTrue(putRes.ok(), "put_feedback_token succeeds");
    expectTrue(currentActor(room) == actorBeforePut, "after put_feedback_token turn does not advance");

    ActionResult resolveRes = resolveToken(room, "split_flow_resolve");
    expectTrue(resolveRes.ok(), "resolve_feedback_token succeeds");
    expectTrue(currentActor(room) != actorBeforePut, "after resolve_feedback_token turn advances");

    // --- 4) pending lifecycle validation ---
    forceAdaptState(room, {TokenEffect::TURN_WILD});
    ActionResult pendingPut = putToken(room, 0, "pending_put");
    expectTrue(pendingPut.ok(), "pending setup put succeeds");
    ActionResult secondPut = putToken(room, 0, "pending_second_put");
    expectTrue(!secondPut.ok(), "cannot put twice before resolve/cancel");
    Action earlyCommit = makeAction(room, "commit");
    ActionResult earlyCommitRes = dispatch(room, earlyCommit, "pending_commit_blocked");
    expectTrue(!earlyCommitRes.ok(), "commit blocked while pending token exists");
    ActionResult pendingCleanupResolve = resolveToken(room, "pending_cleanup_resolve");
    expectTrue(pendingCleanupResolve.ok(), "pending scenario cleanup resolve succeeds");

    forceAdaptState(room, {TokenEffect::TURN_WILD});
    ActionResult resolveWithoutPut = resolveToken(room, "resolve_without_put");
    expectTrue(!resolveWithoutPut.ok(), "resolve_feedback_token fails without pending put");

    forceAdaptState(room, {TokenEffect::TURN_WILD});
    ActionResult cancelWithoutPut = cancelToken(room, 0, "cancel_without_put");
    expectTrue(!cancelWithoutPut.ok(), "cancel_feedback_token fails without pending put");

    // --- 5) cancellation branches ---
    forceAdaptState(room, {TokenEffect::TURN_WILD});
    room.getState().params.setCybernationLevel(0);
    ActionResult putForCancelNoCy = putToken(room, 0, "cancel_no_cy_put");
    expectTrue(putForCancelNoCy.ok(), "cancel path setup put succeeds");
    ActionResult cancelNoCyRes = cancelToken(room, 0, "cancel_no_cy");
    expectTrue(!cancelNoCyRes.ok(), "cancel_feedback_token fails when cybernation is 0");
    ActionResult cancelNoCyCleanup = resolveToken(room, "cancel_no_cy_cleanup_resolve");
    expectTrue(cancelNoCyCleanup.ok(), "cancel insufficient scenario cleanup resolve succeeds");

    forceAdaptState(room, {TokenEffect::TURN_WILD});
    room.getState().params.setCybernationLevel(2);
    ActionResult putForCancelMismatch = putToken(room, 0, "cancel_mismatch_put");
    expectTrue(putForCancelMismatch.ok(), "cancel mismatch setup put succeeds");
    ActionResult cancelMismatchRes = cancelToken(room, 1, "cancel_position_mismatch");
    expectTrue(!cancelMismatchRes.ok(), "cancel_feedback_token fails on position mismatch");
    ActionResult cancelMismatchCleanup = cancelToken(room, 0, "cancel_mismatch_cleanup");
    expectTrue(cancelMismatchCleanup.ok(), "cancel mismatch scenario cleanup succeeds");

    forceAdaptState(room, {TokenEffect::TURN_WILD});
    room.getState().params.setCybernationLevel(2);
    ActionResult putForCancelOk = putToken(room, 0, "cancel_ok_put");
    expectTrue(putForCancelOk.ok(), "cancel success setup put succeeds");
    ActionResult cancelOkRes = cancelToken(room, 0, "cancel_ok");
    expectTrue(cancelOkRes.ok(), "cancel_feedback_token succeeds with enough cybernation");
    expectTrue(room.getState().params.getCybernationLevel() == 1, "cancel consumes 1 cybernation");

    // --- 6) occupancy branch after resolved placement ---
    forceAdaptState(room, {TokenEffect::TURN_WILD, TokenEffect::TURN_WILD, TokenEffect::TURN_WILD});
    expectTrue(putToken(room, 0, "occupy_inner_put").ok(), "inner put succeeds");
    expectTrue(resolveToken(room, "occupy_inner_resolve").ok(), "inner resolve succeeds");
    expectTrue(putToken(room, 1, "occupy_middle_first_put").ok(), "first middle put succeeds");
    expectTrue(resolveToken(room, "occupy_middle_first_resolve").ok(), "first middle resolve succeeds");
    ActionResult middleOccupiedPut = putToken(room, 1, "occupy_middle_second_put");
    expectTrue(!middleOccupiedPut.ok(), "cannot place second token on occupied middle tile");

    // --- 7) effect branches: DEVELOP / TRANSFORM / UNKNOWN / SOLVE_DISRUPTION ---
    forceAdaptState(room, {TokenEffect::DEVELOP_STACK});
    room.getState().getTile(0)->removeOverlay();
    expectTrue(putToken(room, 0, "effect_develop_put").ok(), "DEVELOP setup put succeeds");
    ActionResult developRes = resolveToken(room, "effect_develop_resolve", {{"develop_to", "DEV_B"}});
    expectTrue(developRes.ok(), "DEVELOP_STACK resolve succeeds");
    const bool developHasOverlay = room.getState().getTile(0)->hasOverlay();
    expectTrue(developHasOverlay, "DEVELOP_STACK creates overlay");
    expectTrue(developHasOverlay &&
               room.getState().getTile(0)->getOverlay().getType() == StackType::DEV_B,
               "DEVELOP_STACK chooses DEV_B");

    forceAdaptState(room, {TokenEffect::TRANSFORM_STACK});
    {
        Stack overlay;
        overlay.setType(StackType::DEV_A);
        room.getState().getTile(0)->setOverlay(overlay);
    }
    expectTrue(putToken(room, 0, "effect_transform_put").ok(), "TRANSFORM setup put succeeds");
    ActionResult transformRes = resolveToken(room, "effect_transform_resolve");
    expectTrue(transformRes.ok(), "TRANSFORM_STACK resolve succeeds");
    const bool transformHasOverlay = room.getState().getTile(0)->hasOverlay();
    expectTrue(transformHasOverlay, "TRANSFORM keeps overlay");
    expectTrue(transformHasOverlay &&
               room.getState().getTile(0)->getOverlay().getType() == StackType::DEV_B,
               "TRANSFORM toggles DEV_A -> DEV_B");

    forceAdaptState(room, {TokenEffect::UNKNOWN});
    expectTrue(putToken(room, 0, "effect_unknown_put").ok(), "UNKNOWN setup put succeeds");
    ActionResult unknownRes = resolveToken(room, "effect_unknown_resolve");
    expectTrue(unknownRes.ok(), "UNKNOWN effect resolve returns success");

    // 方案A: SOLVE_DISRUPTION 保留触发抽卡行为
    forceAdaptState(room, {TokenEffect::SOLVE_DISRUPTION});
    room.getState().activeDisruption = std::nullopt;
    expectTrue(putToken(room, 0, "effect_solve_disruption_put").ok(), "SOLVE_DISRUPTION setup put succeeds");
    ActionResult solveDisruptionRes = resolveToken(room, "effect_solve_disruption_resolve");
    expectTrue(solveDisruptionRes.ok(), "SOLVE_DISRUPTION resolves and triggers draw");
    expectTrue(room.getState().activeDisruption.has_value(), "SOLVE_DISRUPTION creates active disruption");

    // --- 8) auto-commit branches ---
    forceAdaptState(room, {TokenEffect::TURN_WILD});
    Action commitEarly = makeAction(room, "commit");
    ActionResult commitEarlyRes = dispatch(room, commitEarly, "commit_before_complete");
    expectTrue(!commitEarlyRes.ok(), "manual commit is rejected before completion");

    forceAdaptState(room, {TokenEffect::TURN_WILD});
    expectTrue(putToken(room, 0, "commit_setup_put").ok(), "commit setup put succeeds");
    ActionResult finishOneRes = resolveToken(room, "commit_setup_resolve");
    expectTrue(finishOneRes.ok(), "commit setup resolve succeeds");
    nlohmann::json finishPayload = nlohmann::json::parse(finishOneRes.message.payload);
    expectTrue(finishPayload.value("autoCommit", false), "final resolve triggers auto commit");
    expectTrue(finishPayload.contains("commit"), "auto commit payload is included");
    expectTrue(finishPayload["commit"].contains("endingName"), "commit payload includes endingName");
    expectTrue(finishPayload["commit"].value("endingName", "") != "Defeat",
               "non-final-round commit is not Defeat");
    Action commitOk = makeAction(room, "commit");
    ActionResult commitOkRes = dispatch(room, commitOk, "commit_success");
    expectTrue(!commitOkRes.ok(), "manual commit is rejected after auto commit");

    // --- 9) defeat only applies on final round ---
    forceAdaptState(room, {TokenEffect::TURN_WILD});
    room.getState().currentRound = room.getState().maxRounds;
    room.getState().playerSelectedRoleId[0] = 12; // THE DIPLOMAT: Co GE 20
    room.getState().params.setCohesion(10); // Force agenda unmet
    expectTrue(putToken(room, 0, "final_round_defeat_put").ok(), "final-round defeat setup put succeeds");
    ActionResult finalRoundRes = resolveToken(room, "final_round_defeat_resolve");
    expectTrue(finalRoundRes.ok(), "final-round defeat resolve succeeds");
    nlohmann::json finalRoundPayload = nlohmann::json::parse(finalRoundRes.message.payload);
    bool p0Defeat = false;
    for (const auto& row : finalRoundPayload["commit"]["endingsByPlayer"]) {
        if (row.value("playerId", -1) == 0 && row.value("endingName", "") == "Defeat") {
            p0Defeat = true;
            break;
        }
    }
    expectTrue(p0Defeat, "Defeat is produced for player 0 on final round when goal+agenda are unmet");
    expectTrue(finalRoundPayload["commit"].value("gameOver", false),
               "Defeat on final round sets gameOver");

    std::cout << "=== Adapt Branch Coverage Finished ===" << std::endl;
    std::cout << "Total failures: " << gFailures << std::endl;
    return gFailures == 0 ? 0 : 1;
}
