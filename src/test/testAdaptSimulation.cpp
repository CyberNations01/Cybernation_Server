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

void printPrompt(const GameRoom& room, const std::string& label) {
    nlohmann::json snap = nlohmann::json::parse(room.getSnapshot());
    const nlohmann::json& prompt = snap["nextPrompt"];
    std::cout << "[PROMPT " << label << "] phase=" << prompt.value("phase", "UNKNOWN")
              << " currentPlayer=" << prompt.value("currentPlayerId", -1)
              << " allowedActions=" << prompt["allowedActions"].dump() << std::endl;
}

ActionResult dispatch(GameRoom& room, Action action, const std::string& label) {
    printPrompt(room, label + "_before");
    ActionResult result = room.receiveAction(action);
    std::cout << "[ACTION " << label << "] "
              << (result.ok() ? "OK" : "FAIL")
              << " status=" << static_cast<int>(result.status)
              << " type=" << result.message.type
              << " payload=" << result.message.payload << std::endl;
    printPrompt(room, label + "_after");
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
}

int main() {
    GameRoom room;
    std::cout << "=== Adapt Branch Coverage Simulation ===" << std::endl;

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

    // --- 1) Push-first snapshot contract check ---
    forceAdaptState(room, {TokenEffect::TURN_WILD});
    nlohmann::json snap = nlohmann::json::parse(room.getSnapshot());
    expectTrue(snap.contains("nextPrompt"), "snapshot includes nextPrompt");
    expectTrue(snap["nextPrompt"]["phase"] == "ADOPT", "nextPrompt reports ADOPT phase");
    if (snap["nextPrompt"].contains("allowedActions")) {
        std::string allowedDump = snap["nextPrompt"]["allowedActions"].dump();
        expectTrue(allowedDump.find("resolve_feedback") != std::string::npos,
                   "nextPrompt exposes resolve_feedback");
    } else {
        expectTrue(false, "nextPrompt contains allowedActions");
    }

    // --- 2) resolve_feedback validation branches ---
    forceAdaptState(room, {TokenEffect::TURN_WILD});
    Action missingParam = makeAction(room, "resolve_feedback");
    ActionResult missingParamRes = dispatch(room, missingParam, "resolve_missing_params");
    expectTrue(!missingParamRes.ok(), "resolve_feedback missing params fails");

    forceAdaptState(room, {TokenEffect::TURN_WILD});
    Action badTarget = makeAction(room, "resolve_feedback");
    badTarget.params["decision"] = "allow";
    badTarget.params["target_tile"] = "abc";
    ActionResult badTargetRes = dispatch(room, badTarget, "resolve_bad_target_type");
    expectTrue(!badTargetRes.ok(), "resolve_feedback invalid target type fails");

    forceAdaptState(room, {TokenEffect::TURN_WILD});
    Action wrongRing = makeAction(room, "resolve_feedback");
    wrongRing.params["decision"] = "allow";
    wrongRing.params["target_tile"] = "1";
    ActionResult wrongRingRes = dispatch(room, wrongRing, "resolve_wrong_ring");
    expectTrue(!wrongRingRes.ok(), "resolve_feedback wrong ring fails");

    forceAdaptState(room, {TokenEffect::TURN_WILD, TokenEffect::TURN_WILD, TokenEffect::TURN_WILD});
    Action occupyInner = makeAction(room, "resolve_feedback");
    occupyInner.params["decision"] = "allow";
    occupyInner.params["target_tile"] = "0";
    ActionResult occupyInnerRes = dispatch(room, occupyInner, "resolve_occupied_tile_inner");
    expectTrue(occupyInnerRes.ok(), "inner resolve succeeds before occupied check");
    Action occupyMiddleFirst = makeAction(room, "resolve_feedback");
    occupyMiddleFirst.params["decision"] = "allow";
    occupyMiddleFirst.params["target_tile"] = "1";
    ActionResult occupyMiddleFirstRes = dispatch(room, occupyMiddleFirst, "resolve_occupied_tile_middle_first");
    expectTrue(occupyMiddleFirstRes.ok(), "first middle resolve succeeds");
    nlohmann::json occupiedSnap = nlohmann::json::parse(room.getSnapshot());
    bool containsOccupiedTile = false;
    for (const auto& v : occupiedSnap["nextPrompt"]["uiHints"]["adapt"]["validTargets"]) {
        if (v.is_number_integer() && v.get<int>() == 1) {
            containsOccupiedTile = true;
            break;
        }
    }
    expectTrue(!containsOccupiedTile, "nextPrompt validTargets filters already occupied tile");
    Action occupied = makeAction(room, "resolve_feedback");
    occupied.params["decision"] = "allow";
    occupied.params["target_tile"] = "1";
    ActionResult occupiedRes = dispatch(room, occupied, "resolve_occupied_tile_middle_second");
    expectTrue(!occupiedRes.ok(), "resolve_feedback occupied tile fails");

    forceAdaptState(room, {TokenEffect::TURN_WILD});
    room.getState().params.setCybernationLevel(0);
    Action cancelNoCy = makeAction(room, "resolve_feedback");
    cancelNoCy.params["decision"] = "cancel";
    cancelNoCy.params["target_tile"] = "0";
    ActionResult cancelNoCyRes = dispatch(room, cancelNoCy, "resolve_cancel_insufficient_cy");
    expectTrue(!cancelNoCyRes.ok(), "cancel fails when cybernation is 0");

    forceAdaptState(room, {TokenEffect::TURN_WILD});
    room.getState().params.setCybernationLevel(2);
    Action cancelOk = makeAction(room, "resolve_feedback");
    cancelOk.params["decision"] = "cancel";
    cancelOk.params["target_tile"] = "0";
    ActionResult cancelOkRes = dispatch(room, cancelOk, "resolve_cancel_ok");
    expectTrue(cancelOkRes.ok(), "cancel succeeds with enough cybernation");
    expectTrue(room.getState().params.getCybernationLevel() == 1, "cancel consumes 1 cybernation");

    // --- 3) Effect branches: DEVELOP / TRANSFORM / UNKNOWN ---
    forceAdaptState(room, {TokenEffect::DEVELOP_STACK});
    room.getState().getTile(0)->removeOverlay();
    Action develop = makeAction(room, "resolve_feedback");
    develop.params["decision"] = "allow";
    develop.params["target_tile"] = "0";
    develop.params["develop_to"] = "DEV_B";
    ActionResult developRes = dispatch(room, develop, "resolve_develop_dev_b");
    expectTrue(developRes.ok(), "DEVELOP_STACK allow succeeds");
    expectTrue(room.getState().getTile(0)->hasOverlay(), "DEVELOP_STACK creates overlay");
    expectTrue(room.getState().getTile(0)->getOverlay().getType() == StackType::DEV_B,
               "DEVELOP_STACK chooses DEV_B");

    forceAdaptState(room, {TokenEffect::TRANSFORM_STACK});
    {
        Stack overlay;
        overlay.setType(StackType::DEV_A);
        room.getState().getTile(0)->setOverlay(overlay);
    }
    Action transform = makeAction(room, "resolve_feedback");
    transform.params["decision"] = "allow";
    transform.params["target_tile"] = "0";
    ActionResult transformRes = dispatch(room, transform, "resolve_transform");
    expectTrue(transformRes.ok(), "TRANSFORM_STACK allow succeeds");
    expectTrue(room.getState().getTile(0)->hasOverlay(), "TRANSFORM keeps overlay");
    expectTrue(room.getState().getTile(0)->getOverlay().getType() == StackType::DEV_B,
               "TRANSFORM toggles DEV_A -> DEV_B");

    forceAdaptState(room, {TokenEffect::UNKNOWN});
    Action unknown = makeAction(room, "resolve_feedback");
    unknown.params["decision"] = "allow";
    unknown.params["target_tile"] = "0";
    ActionResult unknownRes = dispatch(room, unknown, "resolve_unknown_effect");
    expectTrue(unknownRes.ok(), "UNKNOWN effect branch returns success");

    forceAdaptState(room, {TokenEffect::SOLVE_DISRUPTION});
    room.getState().activeDisruption = std::nullopt;
    Action solveDisruption = makeAction(room, "resolve_feedback");
    solveDisruption.params["decision"] = "allow";
    solveDisruption.params["target_tile"] = "0";
    ActionResult solveDisruptionRes = dispatch(room, solveDisruption, "resolve_solve_disruption");
    expectTrue(solveDisruptionRes.ok(), "SOLVE_DISRUPTION draws disruption via GameUtility");
    expectTrue(room.getState().activeDisruption.has_value(), "SOLVE_DISRUPTION creates active disruption");

    // --- 4) cancel_disruption_effect branches ---
    forceAdaptState(room, {TokenEffect::TURN_WILD});
    Action cancelMissingName = makeAction(room, "cancel_disruption_effect");
    ActionResult cancelMissingRes = dispatch(room, cancelMissingName, "cancel_disruption_missing_name");
    expectTrue(!cancelMissingRes.ok(), "cancel_disruption_effect requires disruption_name");

    forceAdaptState(room, {TokenEffect::TURN_WILD});
    Action cancelUnknown = makeAction(room, "cancel_disruption_effect");
    cancelUnknown.params["disruption_name"] = "NO_SUCH_CARD";
    ActionResult cancelUnknownRes = dispatch(room, cancelUnknown, "cancel_disruption_unknown_name");
    expectTrue(!cancelUnknownRes.ok(), "cancel_disruption_effect unknown name fails");

    forceAdaptState(room, {TokenEffect::TURN_WILD});
    Action cancelNotAllowed = makeAction(room, "cancel_disruption_effect");
    cancelNotAllowed.params["disruption_name"] = "COMMUNITY_LEADERS_1";
    ActionResult cancelNotAllowedRes = dispatch(room, cancelNotAllowed, "cancel_disruption_not_cancellable");
    expectTrue(!cancelNotAllowedRes.ok(), "cancel_disruption_effect blocks non-cancellable card");

    forceAdaptState(room, {TokenEffect::TURN_WILD});
    Action cancelNoCost = makeAction(room, "cancel_disruption_effect");
    cancelNoCost.params["disruption_name"] = "HURRICANE_1";
    ActionResult cancelNoCostRes = dispatch(room, cancelNoCost, "cancel_disruption_no_cost");
    expectTrue(cancelNoCostRes.ok(), "cancel_disruption_effect uses GameUtility cancel path");
    expectTrue(cancelNoCostRes.message.type == "disruption_effect_cancelled",
               "cancel_disruption_effect returns restored message type");

    forceAdaptState(room, {TokenEffect::TURN_WILD});
    Action cancelBadTimes = makeAction(room, "cancel_disruption_effect");
    cancelBadTimes.params["disruption_name"] = "HURRICANE_1";
    cancelBadTimes.params["times"] = "abc";
    ActionResult cancelBadTimesRes = dispatch(room, cancelBadTimes, "cancel_disruption_bad_times");
    expectTrue(!cancelBadTimesRes.ok(), "cancel_disruption invalid times fails");

    forceAdaptState(room, {TokenEffect::TURN_WILD});
    Action cancelTimesZero = makeAction(room, "cancel_disruption_effect");
    cancelTimesZero.params["disruption_name"] = "HURRICANE_1";
    cancelTimesZero.params["times"] = "0";
    ActionResult cancelTimesZeroRes = dispatch(room, cancelTimesZero, "cancel_disruption_times_zero");
    expectTrue(!cancelTimesZeroRes.ok(), "cancel_disruption times <= 0 fails");

    forceAdaptState(room, {TokenEffect::TURN_WILD});
    room.getState().params.setCybernationLevel(0);
    Action cancelInsufficient = makeAction(room, "cancel_disruption_effect");
    cancelInsufficient.params["disruption_name"] = "HURRICANE_1";
    ActionResult cancelInsufficientRes = dispatch(room, cancelInsufficient, "cancel_disruption_insufficient_resource");
    expectTrue(cancelInsufficientRes.ok(), "GameUtility cancel path currently does not hard-fail low resource");

    forceAdaptState(room, {TokenEffect::TURN_WILD});
    room.getState().params.setCybernationLevel(2);
    Action cancelSuccess = makeAction(room, "cancel_disruption_effect");
    cancelSuccess.params["disruption_name"] = "HURRICANE_1";
    ActionResult cancelSuccessRes = dispatch(room, cancelSuccess, "cancel_disruption_success");
    expectTrue(cancelSuccessRes.ok(), "cancel_disruption success branch works");

    forceAdaptState(room, {TokenEffect::TURN_WILD});
    Action applySuccess = makeAction(room, "cancel_disruption_effect");
    applySuccess.params["disruption_name"] = "HURRICANE_1";
    applySuccess.params["decision"] = "apply";
    ActionResult applySuccessRes = dispatch(room, applySuccess, "apply_disruption_success");
    expectTrue(applySuccessRes.ok(), "disruption apply path works via GameUtility");
    expectTrue(!room.getState().activeDisruption.has_value(), "apply path clears active disruption");

    // --- 5) trade branches ---
    forceAdaptState(room, {TokenEffect::TURN_WILD});
    Action tradeMissingSource = makeAction(room, "trade");
    ActionResult tradeMissingSourceRes = dispatch(room, tradeMissingSource, "trade_missing_source");
    expectTrue(!tradeMissingSourceRes.ok(), "trade missing source fails");

    forceAdaptState(room, {TokenEffect::TURN_WILD});
    Action tradeWrongSource = makeAction(room, "trade");
    tradeWrongSource.params["source"] = "manual";
    ActionResult tradeWrongSourceRes = dispatch(room, tradeWrongSource, "trade_wrong_source");
    expectTrue(!tradeWrongSourceRes.ok(), "trade wrong source fails");

    forceAdaptState(room, {TokenEffect::TURN_WILD});
    Action tradeMissingParams = makeAction(room, "trade");
    tradeMissingParams.params["source"] = "disruption_cancel";
    ActionResult tradeMissingParamsRes = dispatch(room, tradeMissingParams, "trade_missing_params");
    expectTrue(!tradeMissingParamsRes.ok(), "trade missing params fails");

    forceAdaptState(room, {TokenEffect::TURN_WILD});
    Action tradeBadAmount = makeAction(room, "trade");
    tradeBadAmount.params["source"] = "disruption_cancel";
    tradeBadAmount.params["give_param"] = "CY";
    tradeBadAmount.params["receive_param"] = "TECH";
    tradeBadAmount.params["give_amount"] = "x";
    ActionResult tradeBadAmountRes = dispatch(room, tradeBadAmount, "trade_bad_amount");
    expectTrue(!tradeBadAmountRes.ok(), "trade bad give_amount fails");

    forceAdaptState(room, {TokenEffect::TURN_WILD});
    Action tradeNonPositive = makeAction(room, "trade");
    tradeNonPositive.params["source"] = "disruption_cancel";
    tradeNonPositive.params["give_param"] = "CY";
    tradeNonPositive.params["receive_param"] = "TECH";
    tradeNonPositive.params["give_amount"] = "0";
    ActionResult tradeNonPositiveRes = dispatch(room, tradeNonPositive, "trade_non_positive");
    expectTrue(!tradeNonPositiveRes.ok(), "trade non-positive amount fails");

    forceAdaptState(room, {TokenEffect::TURN_WILD});
    Action tradeUnknownParam = makeAction(room, "trade");
    tradeUnknownParam.params["source"] = "disruption_cancel";
    tradeUnknownParam.params["give_param"] = "BAD_PARAM";
    tradeUnknownParam.params["receive_param"] = "TECH";
    ActionResult tradeUnknownParamRes = dispatch(room, tradeUnknownParam, "trade_unknown_param");
    expectTrue(!tradeUnknownParamRes.ok(), "trade unknown param fails");

    forceAdaptState(room, {TokenEffect::TURN_WILD});
    Action tradeSameParam = makeAction(room, "trade");
    tradeSameParam.params["source"] = "disruption_cancel";
    tradeSameParam.params["give_param"] = "CY";
    tradeSameParam.params["receive_param"] = "CY";
    ActionResult tradeSameParamRes = dispatch(room, tradeSameParam, "trade_same_param");
    expectTrue(!tradeSameParamRes.ok(), "trade same param fails");

    forceAdaptState(room, {TokenEffect::TURN_WILD});
    Action tradeCohesion = makeAction(room, "trade");
    tradeCohesion.params["source"] = "disruption_cancel";
    tradeCohesion.params["give_param"] = "CO";
    tradeCohesion.params["receive_param"] = "TECH";
    ActionResult tradeCohesionRes = dispatch(room, tradeCohesion, "trade_cohesion_blocked");
    expectTrue(!tradeCohesionRes.ok(), "trade cohesion blocked");

    forceAdaptState(room, {TokenEffect::TURN_WILD});
    room.getState().params.setCybernationLevel(0);
    Action tradeInsufficient = makeAction(room, "trade");
    tradeInsufficient.params["source"] = "disruption_cancel";
    tradeInsufficient.params["give_param"] = "CY";
    tradeInsufficient.params["receive_param"] = "TECH";
    tradeInsufficient.params["give_amount"] = "1";
    ActionResult tradeInsufficientRes = dispatch(room, tradeInsufficient, "trade_insufficient");
    expectTrue(!tradeInsufficientRes.ok(), "trade insufficient resource fails");

    forceAdaptState(room, {TokenEffect::TURN_WILD});
    room.getState().params.setCybernationLevel(2);
    int techBefore = room.getState().params.getTechnology();
    Action tradeSuccess = makeAction(room, "trade");
    tradeSuccess.params["source"] = "disruption_cancel";
    tradeSuccess.params["give_param"] = "CY";
    tradeSuccess.params["receive_param"] = "TECH";
    tradeSuccess.params["give_amount"] = "1";
    tradeSuccess.params["receive_amount"] = "1";
    ActionResult tradeSuccessRes = dispatch(room, tradeSuccess, "trade_success");
    expectTrue(tradeSuccessRes.ok(), "trade success branch works");
    expectTrue(room.getState().params.getTechnology() == techBefore + 1, "trade increases receive resource");

    // --- 6) commit branches ---
    forceAdaptState(room, {TokenEffect::TURN_WILD});
    Action commitEarly = makeAction(room, "commit");
    ActionResult commitEarlyRes = dispatch(room, commitEarly, "commit_before_complete");
    expectTrue(!commitEarlyRes.ok(), "commit before completion fails");

    forceAdaptState(room, {TokenEffect::TURN_WILD});
    Action finishOne = makeAction(room, "resolve_feedback");
    finishOne.params["decision"] = "allow";
    finishOne.params["target_tile"] = "0";
    ActionResult finishOneRes = dispatch(room, finishOne, "commit_setup_resolve");
    expectTrue(finishOneRes.ok(), "commit setup resolve succeeds");
    Action commitOk = makeAction(room, "commit");
    ActionResult commitOkRes = dispatch(room, commitOk, "commit_success");
    expectTrue(commitOkRes.ok(), "commit success branch works");

    std::cout << "=== Adapt Branch Coverage Finished ===" << std::endl;
    std::cout << "Total failures: " << gFailures << std::endl;
    return gFailures == 0 ? 0 : 1;
}
