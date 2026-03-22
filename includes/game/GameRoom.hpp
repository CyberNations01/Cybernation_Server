#ifndef GAME_ROOM_HPP
#define GAME_ROOM_HPP

#include "game/GameState.hpp"
#include "game/RoundController.hpp"
#include "core/Action.hpp"
#include "core/ActionResult.hpp"
#include <string>

/*
 * GameRoom — Top-level game session
 * 
 * Owns the GameState and the RoundController.
 * Provides a simple interface for the server layer:
 *   - receiveAction()  : Process a player action
 *   - getSnapshot()    : Get full game state as JSON string
 * 
 * The server layer (REST/WebSocket) just needs to:
 *   1. Parse the client request into an Action
 *   2. Call room.receiveAction(action)
 *   3. Send back room.getSnapshot() to all clients
 */

class GameRoom {
private:
    GameState       state;
    RoundController controller;
    nlohmann::json  buildNextPrompt() const;

public:
    GameRoom();
    ~GameRoom() = default;

    // --- Main interface for server layer ---
    
    // Process a player action. Returns what happened.
    ActionResult receiveAction(const Action& action);

    // Full game state as JSON string (broadcast to all clients after each action)
    std::string getSnapshot() const;

    // Controller state for debugging
    std::string getControllerSnapshot() const;

    // --- Direct state access (for testing / setup) ---
    GameState&       getState()       { return state; }
    const GameState& getState() const { return state; }

    RoundController&       getController()       { return controller; }
    const RoundController& getController() const { return controller; }
};

#endif
