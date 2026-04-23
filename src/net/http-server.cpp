#include "http/httplib.h"
#include "nlohmann/json.hpp"
#include "game/GameRoom.hpp"
#include "phase/EnvisionPhaseHandler.hpp"
#include "phase/TraversePhaseHandler.hpp"
#include "phase/AdoptPhaseHandler.hpp"
#include "core/Action.hpp"
#include "core/ActionResult.hpp"
#include "core/Types.hpp"
 
using json = nlohmann::json;
 
static json actionResultToJson(const ActionResult& result)
{
    json payload;
    payload = json::parse(result.message.payload, nullptr, false);
    if (payload.is_discarded())
        payload = result.message.payload;

    return {
        {"status",  static_cast<int>(result.status)},
        {"message", {
            {"type",    result.message.type},
            {"payload", payload}
        }}
    };
}
 
int main()
{
    httplib::Server server;
    GameRoom room;
 
    EnvisionPhaseHandler envisionHandler;
    TraversePhaseHandler traverseHandler;
    AdoptPhaseHandler    adoptHandler;
 
    /* GET /state — full game state snapshot */
    server.Get("/state", [&](const httplib::Request&, httplib::Response& res) {
        res.set_content(room.getSnapshot(), "application/json");
    });
 
    /* POST /test/action — bypass RoundController, invoke PhaseHandler directly
     *
     * Response merges ActionResult with the same gameState + controller as GET /state
     * so clients see updated board/params in one round-trip.
     */
    server.Post("/test/action", [&](const httplib::Request& req, httplib::Response& res) {
        /* Parse request body */
        json body = json::parse(req.body, nullptr, false);
        if (body.is_discarded()) {
            res.status = 400;
            res.set_content(R"({"error":"invalid JSON"})", "application/json");
            return;
        }
 
        /* Extract phase */
        if (!body.contains("phase") || !body["phase"].is_string()) {
            res.status = 400;
            res.set_content(R"({"error":"missing or invalid 'phase' field"})", "application/json");
            return;
        }
        GamePhase phase = strToGamePhase(body["phase"].get<std::string>());
 
        /* Build Action */
        Action action;
        action.playerId = body.value("playerId", 0);
        action.type = body.value("type", "");
 
        if (body.contains("params") && body["params"].is_object()) {
            for (auto& [k, v] : body["params"].items()) {
                action.params[k] = v.is_string() ? v.get<std::string>() : v.dump();
            }
        }
 
        /* Select handler and execute */
        PhaseHandler* handler = nullptr;
        switch (phase) {
            case GamePhase::ENVISION: handler = &envisionHandler; break;
            case GamePhase::TRAVERSE: handler = &traverseHandler; break;
            case GamePhase::ADOPT:    handler = &adoptHandler;    break;
        }
 
        if (!handler) {
            res.status = 400;
            res.set_content(R"({"error":"unknown phase"})", "application/json");
            return;
        }
 
        ActionResult result = handler->handle(action, room.getState());
        json response = actionResultToJson(result);
        response["gameState"]  = room.getState().toJson();
        response["controller"] = room.getController().toJson();
        res.set_content(response.dump(2), "application/json");
    });
 
    std::cout << "Cybernation server listening on 0.0.0.0:8080\n";
    server.listen("0.0.0.0", 8080);
}