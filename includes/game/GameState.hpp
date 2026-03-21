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

    // --- Board ---
    std::vector<Tile>        board;
    std::vector<TokenEffect> tokenBag;  // Tokens currently in the draw bag

    // --- Resources ---
    Params       params;
    FeedbackPool pool;
    FeedbackTokenManager tokenManager;

    // ---- Card Resource ----
    CardManager<DisruptionCard> disruptionManager;
    std::vector<DisruptionCard> disruptionCatalog;
    std::vector<nlohmann::json> goalDeck;
    nlohmann::json              activeGoal;
    CardManager<Stack> stackManager;
    CardManager<Tile> tileManager;
    std::vector<Stack> devATemplates;
    std::vector<Stack> devBTemplates;
    int devATemplateIndex = 0;
    int devBTemplateIndex = 0;

    // --- Players ---
    Player players[NUM_PLAYERS];
    std::pair<int, int> peopleToken;

    // --- Round/Phase tracking (written by RoundController) ---
    int       currentRound      = 1;
    int       maxRounds         = 50;
    GamePhase currentPhase      = GamePhase::ENVISION;
    int       firstPlayerId     = 0;  // Player who goes first this round
    int       currentPlayerId   = 0;  // Whose turn it is right now
    bool      gameOver          = false;

    // --- Adapt phase runtime state ---
    std::vector<TokenEffect> adaptTrack;       // 11 feedback tokens in order
    int                      adaptCursor = 0;  // next token index to resolve
    std::vector<int>         adaptPlacedOn;    // token index -> tile position (-1 if unresolved)
    std::vector<bool>        adaptCancelled;   // token index -> cancelled?
    std::vector<bool>        adaptTileOccupied;// tile position -> already has a feedback token

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
    void syncTokenBagFromManager();
    void setTokenBag(const std::vector<TokenEffect>& nextBag);

    // Adapt-phase helpers
    void resetAdaptState();
    bool initAdaptTrackIfNeeded();
    bool isAdaptComplete() const;
    bool isAdaptTileAvailable(int tilePos) const;
    Stack nextDevelopmentStack(StackType targetType);
    int countStacksByType(StackType type) const;
    const DisruptionCard* findDisruptionCardByName(const std::string& name) const;
    bool isActiveGoalMet() const;
};

#endif
