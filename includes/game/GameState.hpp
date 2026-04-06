#ifndef GAME_STATE_HPP
#define GAME_STATE_HPP

#include "core/Params.hpp"
#include "core/FeedbackPool.hpp"
#include "core/FeedbackTokenManager.hpp"
#include "core/Stack.hpp"
#include "core/Tile.hpp"
#include "core/Player.hpp"
#include "core/DisruptionCard.hpp"
#include "core/GamePhase.hpp"
#include "core/CardManager.hpp"
#include "core/DataLoader.hpp"
#include "nlohmann/json.hpp"
#include <vector>
#include <string>
#include <array>
#include <optional>
#include <set>


/*
 * GameState is a pure data container for the entire game state.
 * 
 * It does NOT contain game logic or validation — that's the job of
 * the RoundController and PhaseHandlers.
 * 
 * Both layers read and write GameState:
 *   - RoundController manages turn/phase/round progression
 *   - PhaseHandlers mutate board, params, tokens, etc.
 */

class GameState {
public:
    static constexpr int NUM_PLAYERS = 5;
    static constexpr int NUM_TILE  = 11;
    static constexpr int FEEDBACK_TRACK_SIZE = 11;

    // --- Board ---
    std::vector<Tile>        board;
    std::vector<TokenEffect> tokenBag;  // Tokens currently in the draw bag
    std::array<TokenEffect, FEEDBACK_TRACK_SIZE> feedbackTrack;

    // --- Resources ---
    Params       params;
    FeedbackPool pool;
    FeedbackTokenManager tokenManager;

    // ---- Card Resource ----
    CardManager<DisruptionCard> disruptionManager;
    CardManager<Tile> tileManager;
    CardManager<Goal> goalManager;
    
    CardManager<Stack> wildStackManager;
    CardManager<Stack> wasteStackManager;
    CardManager<Stack> devAStackManager;
    CardManager<Stack> devBStackManager;

    // --- Players ---
    Player players[NUM_PLAYERS];

    // {Tile, side} : {4,4} -> board[4] (t4) -> t4.getNeighborSide(4) -> -1, 0-5 
    std::pair<int, int> peopleToken;

    // --- Round/Phase tracking (written by RoundController) ---
    int       currentRound      = 1;
    int       maxRounds         = 50;
    GamePhase currentPhase      = GamePhase::ENVISION;
    int       firstPlayerId     = 0;  // Player who goes first this round
    int       currentPlayerId   = 0;  // Whose turn it is right now
    bool      gameOver          = false;
    bool      ignoreCohesionLossThisRound = false; // Set by IgnoreCohesionEffect
    Goal      currentGoal;
    
    std::optional<DisruptionCard> activeDisruption = std::nullopt;

    // --- Adapt phase runtime state ---
    std::vector<TokenEffect> adaptTrack;       // feedback tokens in order
    int                      adaptCursor = 0;  // next token index to resolve

    // --- Traverse phase runtime state ---
    bool traverseDisruptionDrawn = false;
    bool traverseDisruptionHandled = false;
    bool traverseWalkCompleted = false;

    // --- Constructor ---
    GameState();

    // --- Serialization ---
    nlohmann::json toJson() const;
    std::string    snapshot() const;

    // --- Board helpers ---
    const std::pair<int, int> & getPeopleToken() {return this->peopleToken;};
    std::vector<Tile> getBoard(){return board;};
    Tile*        getTile(int position);
    Player*      getPlayer(int id);

    void setPeopleToken(const std::pair<int,int> & pos);
    // Find who currently holds the first player token
    int findFirstPlayer() const;

    // Re-derive the token bag from current board state
    void rebuildTokenBag();
    void fillFeedbackTrackFromBag();

    // ! BUG ? Why previous in private
    void syncTokenBagFromManager();
    void setTokenBag(const std::vector<TokenEffect>& nextBag);
    bool isActiveGoalMet() const;

private:
    TokenEffect mapStackTypeToFeedbackToken(StackType type) const;
    

};

#endif
