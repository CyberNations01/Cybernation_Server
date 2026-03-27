#include "game/GameRoom.hpp"

GameRoom::GameRoom()
    : state(), controller() {}

ActionResult GameRoom::receiveAction(const Action& action) {
    // TODO: We need to add an interactive, parallel role-setup handling.
    // Keep GameRoom unchanged for now per team constraint.
    return controller.processAction(action, state);
}

std::string GameRoom::getSnapshot() const {
    nlohmann::json combined;
    combined["gameState"]  = state.toJson();
    combined["controller"] = controller.toJson();
    return combined.dump(2);
}

std::string GameRoom::getControllerSnapshot() const {
    return controller.toJson().dump(2);
}
