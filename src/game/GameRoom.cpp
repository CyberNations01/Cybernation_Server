#include "game/GameRoom.hpp"

GameRoom::GameRoom()
    : state(), controller(state) {}

ActionResult GameRoom::receiveAction(const Action& action)
{
    return controller.processAction(action, state);
}

std::string GameRoom::getSnapshot() const
{
    nlohmann::json combined;
    combined["gameState"]  = getGameStateSnapshot();
    combined["controller"] = getControllerSnapshot();
    return combined.dump(2);
}

std::string GameRoom::getControllerSnapshot() const
{
    return controller.toJson().dump(2);
}

std::string GameRoom::getGameStateSnapshot() const
{
    return state.toJson().dump(2);
}
