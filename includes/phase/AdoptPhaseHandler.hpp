#ifndef ADOPT_PHASE_HANDLER_HPP
#define ADOPT_PHASE_HANDLER_HPP

#include "game/PhaseHandler.hpp"

/*
 * AdoptPhaseHandler
 * 
 * Adapt-phase responsibilities:
 *   - Resolve feedback tokens in strict slot order (inner -> middle -> outer)
 *   - Allow cancel/allow decisions with cybernation payment
 *   - Apply token effects (including DevA/DevB development transitions)
 *   - Support disruption-effect cancellation via card-defined cost
 *   - Expose Adapt status for UI synchronization
 *   - Commit/finalize Adapt when all feedback tokens are resolved
 * 
 * This phase assumes first-player order has already been determined
 * by previous phases; it does not reassign first player.
 */

class AdoptPhaseHandler : public PhaseHandler {
public:
    ActionResult handle(const Action& action, GameState& state) override;
    GamePhase    getPhase() const override { return GamePhase::ADOPT; }

private:
    ActionResult handleGetAdaptStatus(GameState& state);
    ActionResult handleResolveFeedback(const Action& action, GameState& state);
    ActionResult handleCancelDisruptionEffect(const Action& action, GameState& state);
    ActionResult handleTrade(const Action& action, GameState& state);
    ActionResult handleCommit(const Action& action, GameState& state);

    bool isValidTargetForCursor(int cursor, int tilePos) const;
    ActionResult applyFeedbackEffect(TokenEffect effect, int tilePos, const Action& action, GameState& state);
};

#endif
