#include "phase/EnvisionPhaseHandler.hpp"
#include <unistd.h>

ActionResult EnvisionPhaseHandler::handle(const Action& action, GameState& state) {
    // "pass" is valid in every phase — RoundController handles the pass 
    // mechanics, but we still return SUCCESS so it knows to proceed.
    // if (action.isPass()) {
    //     return ActionResult::success("Player passed");
    // }

    if (action.type == "walkPath") {
        return handleWalkPath(state);
    }
    return {ActionStatus::INVALID_ACTION, 
            "Action '" + action.type + "' is not valid during Envision phase"};
}

nlohmann::json 
restoJson(const int& tile, const int& side, 
                const std::vector<std::string> & resources, 
                const std::string& layer)
{
    nlohmann::json data;
    nlohmann::json res = nlohmann::json::array();

    data["tile"] = tile;
    data["side"] = side;
    data["layer"] = layer;
    for (const auto& r : resources)
        res.push_back(r);
    data["resources"] = res;
    return data;
}



ActionResult EnvisionPhaseHandler::handleWalkPath(GameState &state)
{
    // ! { Tile, side } 
    std::vector<Tile> gameBoard = state.getBoard();
    std::pair<int, int> tokenLocation = state.getPeopleToken();
    Tile currentTile;
    std::vector<std::string> resourceClaimed;
    nlohmann::json resJson = nlohmann::json::array();


    while (true) {

        currentTile = gameBoard[tokenLocation.first];
        Stack baseStack = currentTile.getBase();
        Stack overlayStack;
        Stack travereStack = baseStack;
        int connectedSide;
        std::vector<std::string> resources;

        std::cout << "Ppl token at: " 
                  << "Tile: " << tokenLocation.first << " " 
                  << "Side: " << tokenLocation.second << std::endl;
        
        /* ! Use path on Overlay Stack if exists */
        if (currentTile.hasOverlay()) {
            overlayStack = gameBoard[tokenLocation.first].getOverlay();
            travereStack = overlayStack;
        }


        connectedSide = travereStack.getConnectedSide(tokenLocation.second);
        if (connectedSide == -1) 
            break;
        
        /* 2. Resource Collection 
            - If Overlay Stack exist, collect both stack resources
            - Collect resources for both of the connected sides 
        */ 
        resources = baseStack.getSides()[tokenLocation.second];
        resJson.push_back(restoJson(tokenLocation.first, tokenLocation.second, resources, "base"));

        resources = baseStack.getSides()[connectedSide];
        resJson.push_back(restoJson(tokenLocation.first, connectedSide, resources, "base"));

        if (currentTile.hasOverlay()) {
            resources = overlayStack.getSides()[tokenLocation.second];
            resJson.push_back(restoJson(tokenLocation.first, tokenLocation.second, resources, "overlay"));

            resources = overlayStack.getSides()[connectedSide];
            resJson.push_back(restoJson(tokenLocation.first, connectedSide, resources, "overlay"));
        }

        /*! 3. Update tokenLocation */ 
        std::pair<int,int> next = currentTile.getNeighbours()[connectedSide];
        if (next.first < 0  || next.second < 0 ||
            next.first  >= GameState::NUM_TILE   || 
            next.second >= GameState::NUM_TILE) {
                tokenLocation.second = connectedSide;
                break;
        }

        tokenLocation = next;
    }
    state.setPeopleToken(tokenLocation);
    return ActionResult::success(resJson.dump()); 
   
}