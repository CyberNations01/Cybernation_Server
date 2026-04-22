#ifndef ROUND_CONTROLLER_HPP
#define ROUND_CONTROLLER_HPP

#include "game/GameState.hpp"
#include "phase/AdoptPhaseHandler.hpp"
#include "phase/EnvisionPhaseHandler.hpp"
#include "phase/TraversePhaseHandler.hpp"
#include "core/Action.hpp"
#include "core/ActionResult.hpp"
#include <set>


class RoundController {
public:

    RoundController();
    ActionResult processAction(const Action& action, GameState& state);

    int         getCurrentPlayerId() const;
    GamePhase   getCurrentPhase() const;
    int         getCurrentRound() const;
    bool        isGameOver() const;

    nlohmann::json toJson() const;

private:

    int       currentRound    = 1;
    int       maxRounds       = 50;
    GamePhase currentPhase    = GamePhase::ENVISION;
    bool      gameOver        = false;

    int firstPlayerId = 0;
    int currentPlayerId = 0;
    std::array<int, GameState::NUM_PLAYERS> turnOrder;
    int turnIndex = 0;
    std::set<int> passedPlayers;

    EnvisionPhaseHandler envisionHandler;
    TraversePhaseHandler traverseHandler;
    AdoptPhaseHandler    adoptHandler;


    void buildTurnOrder(int firstPlayerId);
    void advanceTurn(GameState& state);
    void advancePhase(GameState& state);
    void advanceRound(GameState& state);
    void resetPhase();
    PhaseHandler* getHandlerForPhase(GamePhase phase);
    bool advanceToNextActivePlayer();
};

#endif
