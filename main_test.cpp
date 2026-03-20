#include <iostream>
#include "game/GameRoom.hpp"
#include "core/Action.hpp"

using namespace std;

void printResult(const string& label, const ActionResult& result) {
    cout << "  [" << label << "] " 
         << (result.ok() ? "OK" : "FAIL") 
         << " — " << result.message << endl;
}

int main() {
    cout << "=== Cybernation Game Logic Test ===" << endl << endl;

    // 1. Create a game room
    GameRoom room;
    cout << "Game room created." << endl;
    cout << "Initial snapshot:" << endl;
    cout << room.getSnapshot() << endl << endl;

    // 2. Test: wrong player tries to act (player 1 when player 0 is first)
    cout << "--- Test: Wrong player sends action ---" << endl;
    Action wrongPlayer;
    wrongPlayer.playerId = 1;
    wrongPlayer.type = "trade";
    auto r1 = room.receiveAction(wrongPlayer);
    printResult("Player 1 acts out of turn", r1);
    cout << endl;

    // 3. Test: correct player (0) does a valid action in Envision
    cout << "--- Test: Player 0 claims first player ---" << endl;
    Action claimFP;
    claimFP.playerId = 0;
    claimFP.type = "claim_first_player";
    auto r2 = room.receiveAction(claimFP);
    printResult("Player 0 claims first player", r2);
    cout << endl;

    // 4. Test: now it should be player 1's turn
    cout << "--- Test: Turn advanced to player 1 ---" << endl;
    cout << "Controller state: " << room.getControllerSnapshot() << endl;

    // 5. Test: all players pass → phase should advance
    cout << endl << "--- Test: All players pass (Envision → Traverse) ---" << endl;
    for (int i = 0; i < GameState::NUM_PLAYERS; ++i) {
        int currentId = room.getController().getCurrentPlayerId();
        
        Action pass;
        pass.playerId = currentId;
        pass.type = "pass";
        auto result = room.receiveAction(pass);
        printResult("Player " + to_string(currentId) + " passes", result);
    }

    cout << endl << "After all pass:" << endl;
    cout << room.getSnapshot() << endl << endl;

    // 6. Verify we're now in Traverse phase
    auto& state = room.getState();
    cout << "Current phase: " << static_cast<int>(state.currentPhase) 
         << " (0=Envision, 1=Traverse, 2=Adopt)" << endl;
    cout << "Current player: " << state.currentPlayerId << endl;

    // 7. Test: pass through Traverse and Adopt to trigger round advance
    cout << endl << "--- Test: Pass through Traverse ---" << endl;
    for (int i = 0; i < GameState::NUM_PLAYERS; ++i) {
        int currentId = room.getController().getCurrentPlayerId();
        Action pass;
        pass.playerId = currentId;
        pass.type = "pass";
        room.receiveAction(pass);
    }
    cout << "Phase after Traverse passes: " << static_cast<int>(room.getState().currentPhase) << endl;

    cout << "--- Test: Pass through Adopt ---" << endl;
    for (int i = 0; i < GameState::NUM_PLAYERS; ++i) {
        int currentId = room.getController().getCurrentPlayerId();
        Action pass;
        pass.playerId = currentId;
        pass.type = "pass";
        room.receiveAction(pass);
    }
    cout << "Round after full cycle: " << room.getState().currentRound << endl;
    cout << "Phase after round advance: " << static_cast<int>(room.getState().currentPhase) << endl;

    cout << endl << "=== All tests completed ===" << endl;
    return 0;
}
