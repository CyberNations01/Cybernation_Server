#include "net/Room.hpp"

void Room::joinPlayer(int conn_id)
{
    if (conn_map.find(conn_id) != conn_map.end())
        send(conn_id,
            serialize(ActionResult::invalid({"Room", "Error: Player has joined"})));
    conn_map.insert({conn_id, nextPlayerId});
    return send(conn_id,
            serialize(ActionResult::success({"Room", 
                                             "PlayerId: " + nextPlayerId++})));
}

void Room::onAction(int conn_id, Action action)
{
    if (roomState != ROOM_STATE::PLAYING)
        send(conn_id, serialize(ActionResult::invalid({"Room", "Game not started"}))); 

    auto it = conn_map.find(conn_id);
    if (it == conn_map.end())
        return send(conn_id, serialize(ActionResult::invalid({"Room", "Unknown connection"})));
    
    auto result = gameRoom.receiveAction(action);
    if (result.ok())
        broadcast(gameRoom.getSnapshot());

    send(conn_id, serialize(result));
}


// TODO: handle graceful player disconnect 
// (auto-pass, switch first player, and also update next player etc.)
void Room::removePlayer(int conn_id)
{
    if (conn_map.find(conn_id) == conn_map.end())
        std::cout << "Room: " << "Invalid connection ID" << std::endl;
    
    conn_map.erase(conn_id);
}

void Room::broadcast(const std::string &msg)
{
    for (const auto& e: conn_map)
        sendFunc(e.first, msg);
}

void Room::send(int conn_id, const std::string &msg)
{
    sendFunc(conn_id, msg);
}

std::string Room::serialize(ActionResult result)
{
    nlohmann::json j;
    j["status"] = actionStatusToStr(result.status);
    j["type"] = result.message.type;
    j["payload"] = result.message.payload;
    return j.dump(2);
}