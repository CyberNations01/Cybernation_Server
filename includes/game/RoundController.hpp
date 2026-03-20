#ifndef ROUND_CONTROLLER_HPP
#define ROUND_CONTROLLER_HPP

#include "game/GameState.hpp"
#include "game/PhaseHandler.hpp"
#include "core/Action.hpp"
#include "core/ActionResult.hpp"
#include <set>
#include <memory>
#include <array>

/*
 * RoundController (Layer 1) — Turn & Phase Gatekeeper
 * 
 * Responsibilities:
 *   1. Validate that the action comes from the CURRENT player
 *   2. Check that the player has NOT already passed this phase
 *   3. Delegate to the appropriate PhaseHandler (Layer 2)
 *   4. Handle "pass" bookkeeping (add to passedPlayers set)
 *   5. After each action, check if phase is complete (all passed)
 *   6. Advance phase or round when complete
 *   7. Manage clockwise turn order starting from firstPlayer
 * 
 * NOT responsible for:
 *   - Knowing what actions are legal (PhaseHandler decides)
 *   - Mutating game resources (PhaseHandler does)
 *   - Networking / serialization
 */

class RoundController {
public:
    RoundController();
    ~RoundController() = default;

    /*
     * Main entry point — receives ANY player action.
     * 
     * Flow:
     *   1. Is it this player's turn? No → silently ignore
     *   2. Has player already passed? Yes → ignore
     *   3. Is it a "pass"? → record it, advance turn
     *   4. Otherwise → delegate to PhaseHandler
     *   5. After action: check phase completion, advance if needed
     */
    ActionResult processAction(const Action& action, GameState& state);

    // Query current state
    int  getCurrentPlayerId() const { return turnOrder[turnIndex]; }
    bool hasPlayerPassed(int playerId) const;
    bool isPhaseComplete() const;

    // Get a snapshot of controller state (for debugging / frontend)
    nlohmann::json toJson() const;

private:
    // --- Phase Handlers ---
    std::array<std::unique_ptr<PhaseHandler>, NUM_PHASES> handlers;

    // --- Turn tracking ---
    std::array<int, GameState::NUM_PLAYERS> turnOrder;  // Clockwise from firstPlayer
    int  turnIndex = 0;       // Index into turnOrder for current player
    bool initialized = false; // Has turn order been built yet?
    
    // --- Pass tracking (per phase) ---
    std::set<int> passedPlayers;

    // --- Internal methods ---
    void buildTurnOrder(int firstPlayerId);
    void advanceTurn(GameState& state);
    void advancePhase(GameState& state);
    void advanceRound(GameState& state);
    void resetPhase(GameState& state);
    
    PhaseHandler* getHandlerForPhase(GamePhase phase);

    // Find next non-passed player. Returns false if everyone passed.
    bool advanceToNextActivePlayer();
};

#endif
