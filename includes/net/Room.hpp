#include "game/GameRoom.hpp"
#include "core/ActionResult.hpp"
#include <mutex>
enum ROOM_STATE {
    WAITING,
    PLAYING,
    FINISHED
};

class Room {
public:
    Room(std::function<void(int conn_id, std::string msg)> sendFunc)
    : sendFunc(sendFunc){};
    ~Room() = default;

    void joinPlayer(int conn_id);
    void onAction(int conn_id, Action action);
    void removePlayer(int conn_id);
    
private:
    void broadcast(const std::string& msg);
    void send(int conn_id, const std::string& msg);

    std::string roomId;
    ROOM_STATE  roomState;
    GameRoom    gameRoom;
    int nextPlayerId = 0;

    std::mutex room_mutex;
    std::map<int,int> conn_map;
    std::function<void(int conn_id, std::string msg)> sendFunc;
    std::string serialize(ActionResult result);
};