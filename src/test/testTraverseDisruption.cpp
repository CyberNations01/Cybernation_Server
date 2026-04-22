#include <iostream>
#include <string>
#include "phase/TraversePhaseHandler.hpp"
#include "game/GameState.hpp"
#include "core/Action.hpp"

namespace {
int gFailures = 0;

void expectTrue(bool cond, const std::string& label) {
    std::cout << (cond ? "[PASS] " : "[FAIL] ") << label << std::endl;
    if (!cond) ++gFailures;
}

Action mk(const std::string& type) {
    Action a;
    a.playerId = 0;
    a.type = type;
    return a;
}

DisruptionCard mustFindCard(const GameState& state, const std::string& name) {
    for (const auto& card : state.disruptionManager.getDeck()) {
        if (card.getName() == name) return card;
    }
    for (const auto& card : state.disruptionManager.getDiscard()) {
        if (card.getName() == name) return card;
    }
    std::cerr << "[FATAL] missing card: " << name << std::endl;
    std::exit(2);
}
}

int main() {
    std::cout << "=== Traverse Disruption Flow Test ===" << std::endl;
    TraversePhaseHandler handler;

    {
        GameState s;
        s.params.setCohesion(25);
        s.params.setCybernationLevel(20);
        s.params.setHumanRelation(20);
        s.params.setTechnology(20);
        s.params.setEnvironment(20);

        Action resolveBeforeDraw = mk("resolve_disruption");
        ActionResult r1 = handler.handle(resolveBeforeDraw, s);
        expectTrue(!r1.ok(), "Cannot resolve before draw");

        Action draw = mk("draw_disruption");
        ActionResult r2 = handler.handle(draw, s);
        expectTrue(r2.ok(), "Draw disruption succeeds");
        expectTrue(s.traverseDisruptionDrawn, "Traverse marks disruption drawn");

        Action drawAgain = mk("draw_disruption");
        ActionResult r3 = handler.handle(drawAgain, s);
        expectTrue(!r3.ok(), "Second draw in same Traverse rejected");

        Action resolve = mk("resolve_disruption");
        resolve.params["canceltiles"] = "";
        ActionResult r4 = handler.handle(resolve, s);
        expectTrue(r4.ok(), "Resolve disruption succeeds after draw");
        expectTrue(s.traverseDisruptionHandled, "Traverse marks disruption handled");
        expectTrue(!s.activeDisruption.has_value(), "Resolve clears active disruption");

        Action walk = mk("walkPath");
        ActionResult r5 = handler.handle(walk, s);
        expectTrue(r5.ok(), "walkPath succeeds after disruption handled");
        expectTrue(s.traverseWalkCompleted, "Traverse marks walk completed");

        Action drawAfterWalk = mk("draw_disruption");
        ActionResult r6 = handler.handle(drawAfterWalk, s);
        expectTrue(!r6.ok(), "Cannot draw after walkPath");
    }

    {
        GameState s;
        s.params.setCybernationLevel(20);
        Action cancelBeforeDraw = mk("cancel_disruption");
        ActionResult r1 = handler.handle(cancelBeforeDraw, s);
        expectTrue(!r1.ok(), "Cannot cancel before draw");

        s.activeDisruption = mustFindCard(s, "HURRICANE_1");
        s.traverseDisruptionDrawn = true;

        Action cancel = mk("cancel_disruption");
        ActionResult r3 = handler.handle(cancel, s);
        expectTrue(r3.ok(), "Cancel disruption succeeds");
        expectTrue(s.traverseDisruptionHandled, "Cancel marks disruption handled");
        expectTrue(!s.activeDisruption.has_value(), "Cancel clears active disruption");
    }

    std::cout << "=== Traverse Disruption Finished ===" << std::endl;
    std::cout << "Total failures: " << gFailures << std::endl;
    return gFailures == 0 ? 0 : 1;
}
