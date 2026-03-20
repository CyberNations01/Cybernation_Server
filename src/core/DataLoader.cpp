#include "core/DataLoader.hpp"

template<>
Goal DataLoader::parseJson<Goal>(const nlohmann::json& data)
{
    Goal card;
    std::string stackStr[] = {"Wild", "Waste", "DevA", "DevB"};

    card.setId(data["id"].get<int>());
    card.setName(data.value("name", ""));
    card.setReverseGoalId(data["reverseGoalId"].get<int>());

    /* Parse start_effect */
    if (data.contains("start_effect")) {
        const auto & start_effect = data["start_effect"];
        std::map<StackType, std::vector<int>> effect;

        for (const auto& e: stackStr) {
            if (start_effect.contains(e) && start_effect[e].is_array()) {
                std::vector<int> tiles;
                for (const auto& t: start_effect[e])
                    tiles.push_back(t.get<int>());
                effect[strtoStackType(e)] = tiles;
            }
        }

        card.setStackEffect(effect);
    }

    /* Parse victory_condition */
    if (data.contains("victory_condition")) {
        const auto& cond = data["victory_condition"];
        std::vector<victory_condition> vc_vector;
        std::string vic_type[] = {"HR", "Co", "Env", "Tech", "Cy"};
        
        // Resource condition
        for (const auto& type : vic_type) {
            
            if (cond.contains(type)) {
                victory_condition vc; 
                const auto& type_field = cond[type];
                
                vc.type = type;
                vc.num = type_field.value("num", 0);
                std::string op = type_field.value("compare", "EQ");
                vc.op = strToComparator(op);
                vc_vector.push_back(vc);
            }

        }

        // Tile condition
        if (cond.contains("stack")) {
            const auto& stackField = cond["stack"];
            for (const auto &e: stackStr) {
                if (stackField.contains(e)) {
                    victory_condition vc; 
                    const auto& stk = stackField[e];  

                    vc.type = e;
                    vc.num = stk.value("num", 0);
                    
                    std::string op = stk.value("compare", "EQ");
                    vc.op = strToComparator(op);
                    if (stk.contains("position"))
                        vc.position = 
                            std::optional<std::string>(stk["position"].get<std::string>());
                    vc_vector.push_back(vc);
                }
                
            }
        }
        card.setCondition(vc_vector);
    }
    return card;
}

template <>
DisruptionCard DataLoader::parseJson<DisruptionCard>(const nlohmann::json &data)
{
    DisruptionCard card;

    card.setName(data.value("name", ""));
    card.setDescription(data.value("description", ""));
    card.setType(strtoDisruptionType(data.value("type", "disrupt")));
    card.setCancellable(data.value("cancel", true));
    card.setHasCondition(data.value("cond", "") != "");

    std::vector<int> stackTarget;

    if (data.contains("stackTarget") && data["stackTarget"].is_array()) {
        for (const auto& t : data["stackTarget"])
            if (t.is_number()) stackTarget.push_back(t.get<int>());
    }
    card.setStackTargets(stackTarget);

    // Parse effects
    std::vector<std::pair<DisruptionEffect, int>> effects;

    if (data.contains("effect") && data["effect"].is_array()) {
        for (const auto& e : data["effect"]) {
            if (e.is_string()) {
                std::string s = e.get<std::string>();
                auto colon = s.find(':');
                if (colon != std::string::npos) {
                    std::string name = s.substr(0, colon);
                    int val = std::stoi(s.substr(colon + 1));
                    effects.push_back({strtoDisruptionEffect(s), val});
                } else
                    effects.push_back({strtoDisruptionEffect(s), 0});
            }
        }
    }
    card.setEffects(effects);

    // Parse cancel costs
    std::vector<std::pair<DisruptionEffect, int>> cancelCost;

    if (data.contains("cost") && data["cost"].is_array()) {
        for (const auto& c : data["cost"]) {
            if (c.is_string()) {
                std::string s = c.get<std::string>();
                auto colon = s.find(':');
                if (colon != std::string::npos) {
                    std::string name = s.substr(0, colon);
                    int val = std::stoi(s.substr(colon + 1));
                    cancelCost.push_back({strtoDisruptionEffect(name), val});
                } else
                    cancelCost.push_back({strtoDisruptionEffect(s), 0});
            }
        }
    }

    card.setCancelCosts(cancelCost);
    return card;
}

template <>
Stack DataLoader::parseJson<Stack>(const nlohmann::json &data)
{
    Stack stack;
    stack.setId(data.value("id", 0));
    stack.setType(strtoStackType(data.value("type", "")));
    
    std::vector<std::vector<std::string>> sides;
    if (data.contains("sides") && data["sides"].is_array()) {
        if (data["sides"].size() != 6)
            std::cerr << "Stack should have 6 sides: " << sides.size() << std::endl;
        for (const auto & side: data["sides"]) {
            std::vector<std::string> resources;
            if (side.is_array()) {
                for (const auto & e: side) {
                    resources.push_back(e.get<std::string>());
                }
            }
            sides.push_back(resources);
        }
    }
    stack.setSides(sides);
    
    std::map<int, int> paths;
    if (data.contains("paths") && data["paths"].is_array()){
        for (const auto & path : data["paths"]) {
            if (path.is_array() && path.size() == 2) {
                int from = path[0].get<int>();
                int to = path[1].get<int>();
                paths.insert({from, to});
                paths.insert({to, from});
            }
        }
    }else {
        std::cerr << "Stack should contains path attribute" << std::endl;
    }

    stack.setPaths(paths);
    return stack;
}

template<>
Tile DataLoader::parseJson<Tile>(const nlohmann::json& data)
{
    Tile tile;
    std::vector<std::pair<int,int>> neighbours(Tile::TILE_SIDES, {-1,-1});
    tile.setPosition(data.value("position", 0));
    tile.setRotation(data.value("rotation", 0));

    if (data.contains("neighbours") && data["neighbours"].is_array()) {
        int side = 0;
        for (const auto & n: data["neighbours"]) {
            if (n.is_array() && n.size() == 2) {
                int Neighposition  = n[0].get<int>();
                int Neighside = n[1].get<int>();
                neighbours[side] = {Neighposition, Neighside};
            }
            side++;
        }
        tile.setNeighbour(neighbours);
    }else
        std::cerr << "Tile should contains neighbours attribute" << std::endl;

    return tile;
}

std::vector<DisruptionCard> DataLoader::loadDisrupt(const std::string &filename)
{
    std::vector<DisruptionCard> deck;
    try {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Cannot open: " << filename << std::endl;
            return deck;
        }

        nlohmann::json jsonData;
        file >> jsonData;

        if (!jsonData.is_array()) {
            std::cerr << "JSON must be an array" << std::endl;
            return deck;
        }

        for (const auto& cardData : jsonData) {
            try {
                deck.push_back(parseJson<DisruptionCard>(cardData));
            } catch (const std::exception& e) {
                std::cerr << "Error loading card: " << e.what() << std::endl;
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "Failed to load cards: " << e.what() << std::endl;
        return std::vector<DisruptionCard>();
    }
    return deck;
}

std::vector<Stack> DataLoader::loadStack(const std::string &filename)
{
    std::vector<Stack> stackSet;
    try {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Cannot open: " << filename << std::endl;
            return stackSet;
        }

        nlohmann::json jsonData;
        file >> jsonData;

        if (!jsonData.is_array()) {
            std::cerr << "Json must be an array" << std::endl;
            return stackSet;
        }

        for (const auto & stackData: jsonData) {
            try {
                stackSet.push_back(parseJson<Stack>(stackData));
            } catch (const std::exception& e) {
                std::cerr << "Error loading stack: " << e.what() << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error loading stack: " << e.what() << std::endl;
    }

    return stackSet;
}

std::vector<Tile> DataLoader::loadTile(const std::string &filename)
{

    std::vector<Tile> tileSet;
    try {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Cannot open: " << filename << std::endl;
            return tileSet;
        }

        nlohmann::json jsonData;
        file >> jsonData;

        if (!jsonData.is_array()) {
            std::cerr << "JSON must be an array" << std::endl;
            return tileSet;
        }

        for (const auto& cardData : jsonData) {
            try {
                tileSet.push_back(parseJson<Tile>(cardData));
            } catch (const std::exception& e) {
                std::cerr << "Error loading card: " << e.what() << std::endl;
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "Failed to load cards: " << e.what() << std::endl;
    }
    return tileSet;
}

std::vector<Goal> DataLoader::loadGoal(const std::string& filename)
{
    std::vector<Goal> goalSet;
    try {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Cannot open: " << filename << std::endl;
            return goalSet;
        }

        nlohmann::json jsonData;
        file >> jsonData;

        if (!jsonData.is_array()) {
            std::cerr << "JSON must be an array" << std::endl;
            return goalSet;
        }

        for (const auto& cardData: jsonData) {
            try {
                goalSet.push_back(parseJson<Goal>(cardData));
            } catch (const std::exception& e) {
                std::cerr << "Error loading card: " << e.what() << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Failed to load cards: " << e.what() << std::endl;
    }

    return goalSet;
}

template <>
CardManager<DisruptionCard> DataLoader::loadDeck<DisruptionCard>(const std::string& filename)
{
    return CardManager<DisruptionCard>(loadDisrupt(filename));
}

template <>
CardManager<Stack> DataLoader::loadDeck<Stack>(const std::string& filename)
{
    return CardManager<Stack>(loadStack(filename));
}

template <>
CardManager<Tile> DataLoader::loadDeck<Tile>(const std::string& filename)
{
    return CardManager<Tile>(loadTile(filename));
}

template <>
CardManager<Goal> DataLoader::loadDeck<Goal>(const std::string& filename)
{
    return CardManager<Goal>(loadGoal(filename));
}