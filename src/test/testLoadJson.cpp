#include "core/DataLoader.hpp"
#include "core/Stack.hpp"
#include "core/Types.hpp"
#include "game/GameState.hpp"
#include "core/ActionResult.hpp"
#include "phase/TraversePhaseHandler.hpp"
#include <unordered_map>
#include <iostream>

void testLoadStack()
{
    std::vector<Stack> stackSet;
    stackSet = DataLoader::loadStack("data/stack.json");

    for (const auto& stack : stackSet) {
        std::cout << "=== Stack " << stack.getId() << " ===" << std::endl;
        std::cout << "Type: " << stackTypeToStr(stack.getType()) << std::endl;

        std::cout << "Sides:" << std::endl;
        const auto& sides = stack.getSides();
        for (int i = 0; i < (int)sides.size(); i++) {
            std::cout << "  Side " << i << ": [";
            for (int j = 0; j < (int)sides[i].size(); j++) {
                if (j > 0) std::cout << ", ";
                std::cout << sides[i][j];
            }
            std::cout << "]" << std::endl;
        }

        std::cout << "Paths:" << std::endl;
        const auto& paths = stack.getPaths();
        for (const auto& [from, to] : paths) {
            std::cout << "  " << from << " -> " << to << std::endl;
        }

        std::cout << std::endl;
    }
}

void testLoadTile()
{
    std::vector<Tile> board;
    board = DataLoader::loadTile("data/layout.json");

    for (const auto& tile : board) {
        std::cout << "=== Tile Position " << tile.getPosition() << " ===" << std::endl;
        std::cout << "Rotation: " << tile.getRotation() << std::endl;

        std::cout << "Neighbours:" << std::endl;
        const auto& neighbours = tile.getNeighbours();
        for (int i = 0; i < Tile::TILE_SIDES; i++) {
            std::cout << "  Side " << i << " -> ";
            if (neighbours[i].first == -1)
                std::cout << "EDGE";
            else
                std::cout << "Tile " << neighbours[i].first
                          << " (side " << neighbours[i].second << ")";
            std::cout << std::endl;
        }

        std::cout << std::endl;
    }
}

void testBoard()
{
    GameState state;
    const auto& board = state.getBoard();

    for (int i = 0; i < (int)board.size(); i++) {
        std::cout << "=== Tile " << board[i].getPosition() << " ===" << std::endl;

        std::cout << "Base: " << stackTypeToStr(board[i].getBase().getType())
                  << " (Stack " << board[i].getBase().getId() << ")" << std::endl;

        if (board[i].hasOverlay()) {
            std::cout << "Overlay: " << stackTypeToStr(board[i].getOverlay().getType())
                      << " (Stack " << board[i].getOverlay().getId() << ")" << std::endl;
        } else {
            std::cout << "Overlay: none" << std::endl;
        }

        std::cout << "Effective Type: "
                  << stackTypeToStr(board[i].getEffectiveType()) << std::endl;

        std::cout << "Base Sides:" << std::endl;
        const auto& sides = board[i].getBase().getSides();
        for (int s = 0; s < (int)sides.size(); s++) {
            std::cout << "  Side " << s << ": [";
            for (int j = 0; j < (int)sides[s].size(); j++) {
                if (j > 0) std::cout << ", ";
                std::cout << sides[s][j];
            }
            std::cout << "]" << std::endl;
        }

        if (board[i].hasOverlay()) {
            std::cout << "Overlay Sides:" << std::endl;
            const auto& oSides = board[i].getOverlay().getSides();
            for (int s = 0; s < (int)oSides.size(); s++) {
                std::cout << "  Side " << s << ": [";
                for (int j = 0; j < (int)oSides[s].size(); j++) {
                    if (j > 0) std::cout << ", ";
                    std::cout << oSides[s][j];
                }
                std::cout << "]" << std::endl;
            }
        }

        std::cout << "Neighbours:" << std::endl;
        const auto& neighbours = board[i].getNeighbours();
        for (int s = 0; s < Tile::TILE_SIDES; s++) {
            std::cout << "  Side " << s << ": ";
            if (neighbours[s].first == -1)
                std::cout << "EDGE";
            else
                std::cout << "Tile " << neighbours[s].first
                          << " (side " << neighbours[s].second << ")";
            std::cout << std::endl;
        }

        std::cout << "Active Paths:" << std::endl;
        if (board[i].hasOverlay()) {
            const auto& paths = board[i].getOverlay().getPaths();
            for (const auto& [from, to] : paths) {
                if (from < to)
                    std::cout << "  " << from << " <-> " << to << std::endl;
            }
        } else {
            const auto& paths = board[i].getBase().getPaths();
            for (const auto& [from, to] : paths) {
                if (from < to)
                    std::cout << "  " << from << " <-> " << to << std::endl;
            }
        }

        std::cout << std::endl;

    }
}

void testWalkPath()
{
    GameState state;
    TraversePhaseHandler handler;
    Action clientReq = {
        .playerId = 1,
        .type = "walkPath",
        .params = std::unordered_map<std::string, std::string>(),
    };

    ActionResult res = handler.handle(clientReq, state);
    std::cout << "Result Type: " << res.message.type << std::endl;
    std::cout << res.message.payload << std::endl;

}

void testLoadGoal()
{
    std::ifstream file("data/goal.json");
    if (!file.is_open()) {
        std::cout << "Cannot open data/goal.json" << std::endl;
        return;
    }

    nlohmann::json goals;
    file >> goals;
    if (!goals.is_array()) {
        std::cout << "goal.json is not an array" << std::endl;
        return;
    }

    for (const auto& goal : goals) {
        std::cout << "=== Goal " << goal.value("id", -1) << " ===" << std::endl;
        std::cout << "Name: " << goal.value("name", std::string("Unknown")) << std::endl;
        std::cout << "Reverse Goal ID: " << goal.value("reverseGoalId", -1) << std::endl;
        if (goal.contains("victory_condition")) {
            std::cout << "Victory Conditions: " << goal["victory_condition"].dump() << std::endl;
        }
        if (goal.contains("start_effect")) {
            std::cout << "Stack Effects: " << goal["start_effect"].dump() << std::endl;
        }
        std::cout << std::endl;
    }
}


int main(void)
{
    // testLoadStack();
    // testLoadTile();
    // testBoard();
    // testWalkPath();
    testLoadGoal();
    return 0;
}