#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#include "core/Action.hpp"
#include "core/DisruptionCard.hpp"
#include "core/Types.hpp"
#include "game/GameState.hpp"
#include "phase/AdoptPhaseHandler.hpp"

namespace {
int gFailures = 0;

void expectTrue(bool cond, const std::string& label) {
    std::cout << (cond ? "[PASS] " : "[FAIL] ") << label << std::endl;
    if (!cond) ++gFailures;
}

Action makeAction(const std::string& type) {
    Action a;
    a.playerId = 0;
    a.type = type;
    return a;
}

ActionResult dispatch(AdoptPhaseHandler& handler, GameState& state, Action action, const std::string& label) {
    ActionResult result = handler.handle(action, state);
    std::cout << "[ACTION " << label << "] "
              << (result.ok() ? "OK" : "FAIL")
              << " status=" << static_cast<int>(result.status)
              << " type=" << result.message.type
              << " payload=" << result.message.payload << std::endl;
    return result;
}

void forceAdaptState(GameState& state, const std::vector<TokenEffect>& track) {
    state.currentPhase = GamePhase::ADOPT;
    state.gameOver = false;
    state.activeDisruption = std::nullopt;
    state.adaptTrack = track;
    state.adaptCursor = 0;
    state.tokenManager.clearTrack();
    state.setTokenBag(track);
}

DisruptionCard mustFindDisruptionCard(const GameState& state, const std::string& name) {
    for (const auto& card : state.disruptionManager.getDeck()) {
        if (card.getName() == name) return card;
    }
    for (const auto& card : state.disruptionManager.getDiscard()) {
        if (card.getName() == name) return card;
    }
    std::cerr << "[FATAL] disruption card not found: " << name << std::endl;
    std::exit(2);
}
}

int main() {
    GameState state;
    AdoptPhaseHandler handler;
    std::cout << "=== Adapt Branch Coverage Simulation ===" << std::endl;

    // --- 1) resolve_feedback validation branches ---
    forceAdaptState(state, {TokenEffect::TURN_WILD});
    Action missingParam = makeAction("resolve_feedback");
    ActionResult missingParamRes = dispatch(handler, state, missingParam, "resolve_missing_params");
    expectTrue(!missingParamRes.ok(), "resolve_feedback missing params fails");

    forceAdaptState(state, {TokenEffect::TURN_WILD});
    Action badTarget = makeAction("resolve_feedback");
    badTarget.params["decision"] = "allow";
    badTarget.params["target_tile"] = "abc";
    ActionResult badTargetRes = dispatch(handler, state, badTarget, "resolve_bad_target_type");
    expectTrue(!badTargetRes.ok(), "resolve_feedback invalid target type fails");

    forceAdaptState(state, {TokenEffect::TURN_WILD});
    Action wrongRing = makeAction("resolve_feedback");
    wrongRing.params["decision"] = "allow";
    wrongRing.params["target_tile"] = "1";
    ActionResult wrongRingRes = dispatch(handler, state, wrongRing, "resolve_wrong_ring");
    expectTrue(!wrongRingRes.ok(), "resolve_feedback wrong ring fails");

    forceAdaptState(state, {TokenEffect::TURN_WILD, TokenEffect::TURN_WILD, TokenEffect::TURN_WILD});
    Action occupyInner = makeAction("resolve_feedback");
    occupyInner.params["decision"] = "allow";
    occupyInner.params["target_tile"] = "0";
    ActionResult occupyInnerRes = dispatch(handler, state, occupyInner, "resolve_occupied_tile_inner");
    expectTrue(occupyInnerRes.ok(), "inner resolve succeeds before occupied check");
    Action occupyMiddleFirst = makeAction("resolve_feedback");
    occupyMiddleFirst.params["decision"] = "allow";
    occupyMiddleFirst.params["target_tile"] = "1";
    ActionResult occupyMiddleFirstRes = dispatch(handler, state, occupyMiddleFirst, "resolve_occupied_tile_middle_first");
    expectTrue(occupyMiddleFirstRes.ok(), "first middle resolve succeeds");
    Action occupied = makeAction("resolve_feedback");
    occupied.params["decision"] = "allow";
    occupied.params["target_tile"] = "1";
    ActionResult occupiedRes = dispatch(handler, state, occupied, "resolve_occupied_tile_middle_second");
    expectTrue(!occupiedRes.ok(), "resolve_feedback occupied tile fails");

    forceAdaptState(state, {TokenEffect::TURN_WILD});
    state.params.setCybernationLevel(0);
    Action cancelNoCy = makeAction("resolve_feedback");
    cancelNoCy.params["decision"] = "cancel";
    cancelNoCy.params["target_tile"] = "0";
    ActionResult cancelNoCyRes = dispatch(handler, state, cancelNoCy, "resolve_cancel_insufficient_cy");
    expectTrue(!cancelNoCyRes.ok(), "cancel fails when cybernation is 0");

    forceAdaptState(state, {TokenEffect::TURN_WILD});
    state.params.setCybernationLevel(2);
    Action cancelOk = makeAction("resolve_feedback");
    cancelOk.params["decision"] = "cancel";
    cancelOk.params["target_tile"] = "0";
    ActionResult cancelOkRes = dispatch(handler, state, cancelOk, "resolve_cancel_ok");
    expectTrue(cancelOkRes.ok(), "cancel succeeds with enough cybernation");
    expectTrue(state.params.getCybernationLevel() == 1, "cancel consumes 1 cybernation");

    // --- 2) Effect branches: DEVELOP / TRANSFORM / UNKNOWN / SOLVE_DISRUPTION ---
    forceAdaptState(state, {TokenEffect::DEVELOP_STACK});
    Tile* inner = state.getTile(0);
    if (inner) inner->removeOverlay();
    Action develop = makeAction("resolve_feedback");
    develop.params["decision"] = "allow";
    develop.params["target_tile"] = "0";
    develop.params["develop_to"] = "DEV_B";
    ActionResult developRes = dispatch(handler, state, develop, "resolve_develop_dev_b");
    expectTrue(developRes.ok(), "DEVELOP_STACK allow succeeds");
    expectTrue(inner && inner->hasOverlay(), "DEVELOP_STACK creates overlay");
    expectTrue(inner && inner->hasOverlay() && inner->getOverlay().getType() == StackType::DEV_B,
               "DEVELOP_STACK chooses DEV_B");

    forceAdaptState(state, {TokenEffect::TRANSFORM_STACK});
    inner = state.getTile(0);
    if (inner) {
        Stack overlay;
        overlay.setType(StackType::DEV_A);
        inner->setOverlay(overlay);
    }
    Action transform = makeAction("resolve_feedback");
    transform.params["decision"] = "allow";
    transform.params["target_tile"] = "0";
    ActionResult transformRes = dispatch(handler, state, transform, "resolve_transform");
    expectTrue(transformRes.ok(), "TRANSFORM_STACK allow succeeds");
    expectTrue(inner && inner->hasOverlay(), "TRANSFORM keeps overlay");
    expectTrue(inner && inner->hasOverlay() && inner->getOverlay().getType() == StackType::DEV_B,
               "TRANSFORM toggles DEV_A -> DEV_B");

    forceAdaptState(state, {TokenEffect::UNKNOWN});
    Action unknown = makeAction("resolve_feedback");
    unknown.params["decision"] = "allow";
    unknown.params["target_tile"] = "0";
    ActionResult unknownRes = dispatch(handler, state, unknown, "resolve_unknown_effect");
    expectTrue(unknownRes.ok(), "UNKNOWN effect branch returns success");

    forceAdaptState(state, {TokenEffect::SOLVE_DISRUPTION});
    state.activeDisruption = std::nullopt;
    Action solveDisruption = makeAction("resolve_feedback");
    solveDisruption.params["decision"] = "allow";
    solveDisruption.params["target_tile"] = "0";
    ActionResult solveDisruptionRes = dispatch(handler, state, solveDisruption, "resolve_solve_disruption");
    expectTrue(solveDisruptionRes.ok(), "SOLVE_DISRUPTION draws disruption via GameUtility");
    expectTrue(state.activeDisruption.has_value(), "SOLVE_DISRUPTION creates active disruption");

    // --- 3) cancel_disruption_effect branches ---
    forceAdaptState(state, {TokenEffect::TURN_WILD});
    Action cancelMissingName = makeAction("cancel_disruption_effect");
    ActionResult cancelMissingRes = dispatch(handler, state, cancelMissingName, "cancel_disruption_missing_name");
    expectTrue(!cancelMissingRes.ok(), "cancel_disruption_effect requires disruption_name");

    forceAdaptState(state, {TokenEffect::TURN_WILD});
    Action cancelUnknown = makeAction("cancel_disruption_effect");
    cancelUnknown.params["disruption_name"] = "NO_SUCH_CARD";
    ActionResult cancelUnknownRes = dispatch(handler, state, cancelUnknown, "cancel_disruption_unknown_name");
    expectTrue(!cancelUnknownRes.ok(), "cancel_disruption_effect unknown name fails");

    forceAdaptState(state, {TokenEffect::TURN_WILD});
    Action cancelNoCost = makeAction("cancel_disruption_effect");
    cancelNoCost.params["disruption_name"] = "HURRICANE_1";
    ActionResult cancelNoCostRes = dispatch(handler, state, cancelNoCost, "cancel_disruption_no_cost");
    expectTrue(cancelNoCostRes.ok(), "cancel_disruption_effect uses GameUtility cancel path");
    expectTrue(cancelNoCostRes.message.type == "disruption_effect_cancelled",
               "cancel_disruption_effect returns restored message type");

    forceAdaptState(state, {TokenEffect::TURN_WILD});
    Action applySuccess = makeAction("cancel_disruption_effect");
    applySuccess.params["disruption_name"] = "HURRICANE_1";
    applySuccess.params["decision"] = "apply";
    ActionResult applySuccessRes = dispatch(handler, state, applySuccess, "apply_disruption_success");
    expectTrue(applySuccessRes.ok(), "disruption apply path works via GameUtility");
    expectTrue(!state.activeDisruption.has_value(), "apply path clears active disruption");

    // --- 4) disruption routes in ADOPT ---
    forceAdaptState(state, {TokenEffect::TURN_WILD});
    Action adoptDraw = makeAction("draw_disruption");
    ActionResult adoptDrawRes = dispatch(handler, state, adoptDraw, "adopt_draw_disruption");
    expectTrue(adoptDrawRes.ok(), "ADOPT draw_disruption route works");
    expectTrue(state.activeDisruption.has_value(), "ADOPT draw creates active disruption");

    forceAdaptState(state, {TokenEffect::TURN_WILD});
    state.activeDisruption = mustFindDisruptionCard(state, "HURRICANE_1");
    Action adoptCancel = makeAction("cancel_disruption");
    ActionResult adoptCancelRes = dispatch(handler, state, adoptCancel, "adopt_cancel_disruption");
    expectTrue(adoptCancelRes.ok(), "ADOPT cancel_disruption route works");
    expectTrue(!state.activeDisruption.has_value(), "ADOPT cancel clears active disruption");

    forceAdaptState(state, {TokenEffect::TURN_WILD});
    state.params.setCybernationLevel(10);
    state.activeDisruption = mustFindDisruptionCard(state, "HURRICANE_1");
    Action adoptResolve = makeAction("resolve_disruption");
    ActionResult adoptResolveRes = dispatch(handler, state, adoptResolve, "adopt_resolve_disruption");
    expectTrue(adoptResolveRes.ok(), "ADOPT resolve_disruption route works");
    expectTrue(!state.activeDisruption.has_value(), "ADOPT resolve clears active disruption");

    // --- 5) trade branches ---
    forceAdaptState(state, {TokenEffect::TURN_WILD});
    Action tradeMissingSource = makeAction("trade");
    ActionResult tradeMissingSourceRes = dispatch(handler, state, tradeMissingSource, "trade_missing_source");
    expectTrue(!tradeMissingSourceRes.ok(), "trade missing source fails");

    forceAdaptState(state, {TokenEffect::TURN_WILD});
    Action tradeBadAmount = makeAction("trade");
    tradeBadAmount.params["src"] = "CY";
    tradeBadAmount.params["dst"] = "TECH";
    tradeBadAmount.params["amount"] = "x";
    ActionResult tradeBadAmountRes = dispatch(handler, state, tradeBadAmount, "trade_bad_amount");
    expectTrue(!tradeBadAmountRes.ok(), "trade invalid amount fails");

    forceAdaptState(state, {TokenEffect::TURN_WILD});
    Action tradeNonPositive = makeAction("trade");
    tradeNonPositive.params["src"] = "CY";
    tradeNonPositive.params["dst"] = "TECH";
    tradeNonPositive.params["amount"] = "0";
    ActionResult tradeNonPositiveRes = dispatch(handler, state, tradeNonPositive, "trade_non_positive");
    expectTrue(!tradeNonPositiveRes.ok(), "trade non-positive amount fails");

    forceAdaptState(state, {TokenEffect::TURN_WILD});
    Action tradeUnknownParam = makeAction("trade");
    tradeUnknownParam.params["src"] = "BAD_PARAM";
    tradeUnknownParam.params["dst"] = "TECH";
    ActionResult tradeUnknownParamRes = dispatch(handler, state, tradeUnknownParam, "trade_unknown_param");
    expectTrue(!tradeUnknownParamRes.ok(), "trade unknown param fails");

    forceAdaptState(state, {TokenEffect::TURN_WILD});
    state.params.setCybernationLevel(0);
    Action tradeInsufficient = makeAction("trade");
    tradeInsufficient.params["src"] = "CY";
    tradeInsufficient.params["dst"] = "TECH";
    tradeInsufficient.params["amount"] = "1";
    ActionResult tradeInsufficientRes = dispatch(handler, state, tradeInsufficient, "trade_insufficient");
    expectTrue(!tradeInsufficientRes.ok(), "trade insufficient resource fails");

    forceAdaptState(state, {TokenEffect::TURN_WILD});
    state.params.setCybernationLevel(2);
    int techBefore = state.params.getTechnology();
    Action tradeSuccess = makeAction("trade");
    tradeSuccess.params["src"] = "CY";
    tradeSuccess.params["dst"] = "TECH";
    tradeSuccess.params["amount"] = "1";
    ActionResult tradeSuccessRes = dispatch(handler, state, tradeSuccess, "trade_success");
    expectTrue(tradeSuccessRes.ok(), "trade success branch works");
    expectTrue(state.params.getTechnology() == techBefore + 1, "trade increases receive resource");

    // --- 6) commit branches ---
    forceAdaptState(state, {TokenEffect::TURN_WILD});
    Action commitEarly = makeAction("commit");
    ActionResult commitEarlyRes = dispatch(handler, state, commitEarly, "commit_before_complete");
    expectTrue(!commitEarlyRes.ok(), "commit before completion fails");

    forceAdaptState(state, {TokenEffect::TURN_WILD});
    Action finishOne = makeAction("resolve_feedback");
    finishOne.params["decision"] = "allow";
    finishOne.params["target_tile"] = "0";
    ActionResult finishOneRes = dispatch(handler, state, finishOne, "commit_setup_resolve");
    expectTrue(finishOneRes.ok(), "commit setup resolve succeeds");
    Action commitOk = makeAction("commit");
    ActionResult commitOkRes = dispatch(handler, state, commitOk, "commit_success");
    expectTrue(commitOkRes.ok(), "commit success branch works");

    std::cout << "=== Adapt Branch Coverage Finished ===" << std::endl;
    std::cout << "Total failures: " << gFailures << std::endl;
    std::fflush(stdout);
    std::_Exit(gFailures == 0 ? 0 : 1);
}
