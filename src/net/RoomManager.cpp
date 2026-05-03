#include "net/RoomManager.hpp"

std::string
RoomManager::createRoom()
{
    std::lock_guard<std::mutex>lock(room_map_mutex);
    std::string code;
    do {
        code = generateCode(code_size);
    } while (room_map.find(code) != room_map.end());

    room_map[code] = std::make_unique<Room>(sendFunc);
    return code;
}
void RoomManager::joinRoom(std::string code, int conn_id)
{
    std::lock_guard<std::mutex>lock(room_map_mutex);
    if (room_map.find(code) == room_map.end())
        return sendFunc(conn_id, "Game room does not exist");

    room_map.at(code)->joinPlayer(conn_id);
}

void RoomManager::leaveRoom(std::string code, int conn_id)
{
    std::lock_guard<std::mutex>lock(room_map_mutex);
    if (room_map.find(code) == room_map.end())
        return sendFunc(conn_id, "Failed to leave game room");

    room_map.at(code)->removePlayer(conn_id);
    // TODO: if room is FINISHED or empty, erase from rooms map
    return;
}

Room *RoomManager::getRoom(std::string code)
{
    std::lock_guard<std::mutex>lock(room_map_mutex);
    auto it = room_map.find(code);
    if (it == room_map.end())
        return nullptr;
    return it->second.get();
}
std::string RoomManager::generateCode(int length)
{
    static const char chars[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    static std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(0, sizeof(chars) - 2);

    std::string code(length, ' ');
    for (auto& c : code)
            c = chars[dist(rng)];
    return code;
} 
