#include "game/GameUtility.hpp"

namespace {
}

nlohmann::json 
GameUtility::pathResultToJson(const int& tile, const int& side, 
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

ActionResult GameUtility::walkPath(GameState &state)
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
        resJson.push_back(pathResultToJson(tokenLocation.first, tokenLocation.second, resources, "base"));

        resources = baseStack.getSides()[connectedSide];
        resJson.push_back(pathResultToJson(tokenLocation.first, connectedSide, resources, "base"));

        if (currentTile.hasOverlay()) {
            resources = overlayStack.getSides()[tokenLocation.second];
            resJson.push_back(pathResultToJson(tokenLocation.first, tokenLocation.second, resources, "overlay"));

            resources = overlayStack.getSides()[connectedSide];
            resJson.push_back(pathResultToJson(tokenLocation.first, connectedSide, resources, "overlay"));
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
    return ActionResult::success(ActionMessage("walkPath", resJson.dump())); 
}

ActionResult GameUtility::drawDisruption(GameState& state)
{
    if (state.activeDisruption.has_value())
        return {ActionStatus::INVALID_ACTION, 
               {"draw_disruption", 
                "Disruption Card has already been drawn"}};
    state.activeDisruption = state.disruptionManager.draw();
    return {ActionStatus::SUCCESS, {
        "draw_disruption",
        // ! TODO: Format DisruptionCard Detail back to Client
        "DisruptionCard"
    }};
}

bool GameUtility::checkResourceCondition(GameState& state, ResourceCondition cond)
{
    int LHS = state.params.getParamAmount(cond.lhs);
    int RHS = state.params.getParamAmount(cond.rhs);

    switch (cond.compare) {
        case comparator::EQ:
            return LHS == RHS;
        case comparator::GE:
            return LHS >= RHS;
        case comparator::GT:
            return LHS > RHS;
        case comparator::LE:
            return LHS <= RHS;
        case comparator::LT:
            return LHS < RHS;
        case comparator::NE:
            return LHS != RHS;
    }
    return false;
}

bool GameUtility::isStackEffect(const DisruptionEffect &e) 
{
    return (e == DisruptionEffect::TURN_WILD  ||
            e == DisruptionEffect::TURN_WASTE ||
            e == DisruptionEffect::TURN_DEV_A ||
            e == DisruptionEffect::TURN_DEV_B);
}

void GameUtility::resolveParamEffect(GameState & state,
                                        const std::pair<DisruptionEffect, int>& effect)
{
    DisruptionEffect type = effect.first;
    int delta = effect.second; 
    switch (effect.first) {
        case DisruptionEffect::COHESION:
            state.params.adjustParam(CyberParameter::COHESION, delta);
            break;
        case DisruptionEffect::HUMAN_RELATION:
            state.params.adjustParam(CyberParameter::HUMAN_RELATION, delta);
            break;
        case DisruptionEffect::CYBERNATION:
            state.params.adjustParam(CyberParameter::CYBERNATION_LEVEL, delta);
            break;
        case DisruptionEffect::TECHNOLOGY:
            state.params.adjustParam(CyberParameter::TECHNOLOGY, delta);
            break;
        case DisruptionEffect::ENVIRONMENT:
            state.params.adjustParam(CyberParameter::ENVIRONMENT, delta);
            break;
        default: 
            state.params.adjustParam(CyberParameter::COHESION, delta);
            break;
    }
}

void GameUtility::changeTileStack(GameState& state, 
                                int tilePos,
                                StackType targetType)
{
    CardManager<Stack>* StackManager;
    switch (targetType) {
        case StackType::WILD:
             StackManager = &state.wildStackManager;

            if (state.board[tilePos].hasOverlay())
                state.board[tilePos].removeOverlay();

            if (StackManager->isDeckEmpty())
                StackManager->reshuffle();
            
            if (state.board[tilePos].getBase().getType() != StackType::WILD)
                state.board[tilePos].setBase(StackManager->draw());

            break;
        
        case StackType::WASTE:
            StackManager = &state.wasteStackManager;

            if (StackManager->isDeckEmpty())
                StackManager->reshuffle();
            
            if (state.board[tilePos].hasOverlay())
                state.board[tilePos].removeOverlay();
            
            if (state.board[tilePos].getBase().getType() != StackType::WASTE)
                state.board[tilePos].setBase(StackManager->draw());
            
            break;
        
        case StackType::DEV_A:
            StackManager = &state.devAStackManager;
            
            if (StackManager->isDeckEmpty())
                StackManager->reshuffle();
            
            if (!state.board[tilePos].hasOverlay())
                state.board[tilePos].setOverlay(StackManager->draw());

            else if (state.board[tilePos].getEffectiveType() != StackType::DEV_A)
                state.board[tilePos].setOverlay(StackManager->draw());

            break;
        case StackType::DEV_B:
            StackManager = &state.devBStackManager;
            
            if (StackManager->isDeckEmpty())
                StackManager->reshuffle();
            
            if (!state.board[tilePos].hasOverlay())
                state.board[tilePos].setOverlay(StackManager->draw());

            else if (state.board[tilePos].getEffectiveType() != StackType::DEV_B)
                state.board[tilePos].setOverlay(StackManager->draw());


            break;

    }
}


std::set<int> parseIntSet(const std::string& s) {
    std::set<int> result;
    std::istringstream ss(s);
    std::string token;
    while (std::getline(ss, token, ',')) {
        result.insert(std::stoi(token));
    }
    return result;
}


ActionResult GameUtility::applyDisruptionEffect(GameState& state, const Action& action)
{
    if (!state.activeDisruption.has_value()) {
        return ActionResult::invalid("Disruption card is not drawn");
    }

    // ! Disruption Card  
    DisruptionCard &card = state.activeDisruption.value();
    std::string category = card.getCategory();
    const auto& tiles = card.getStackTargets();
    std::vector<std::pair<DisruptionEffect, int>> cancelCosts = card.getCosts();

    std::vector<int> effectiveStackTarget;

    const auto& clientReq = action.params;


    auto tryCancelTiles = [&](const DisruptionCard& card,
                             const std::unordered_map<std::string, std::string> clientReq,
                             const std::vector<int>& stackTarget,
                             const std::vector<std::pair<DisruptionEffect, int>>& cancelCosts,
                             std::vector<int>& effectiveStackTarget) -> std::optional<ActionResult>
    {
        if (clientReq.find("canceltiles") == clientReq.end()) {
            for (auto t : stackTarget)
                effectiveStackTarget.push_back(t);
            return std::nullopt;
        }

        if (!card.isCancellable())
            return ActionResult::invalid("Card is not cancellable\n");

        auto cancelTiles = parseIntSet(clientReq.at("canceltiles"));
        for (auto t : stackTarget) {
            if (cancelTiles.find(t) == cancelTiles.end())
                effectiveStackTarget.push_back(t);
        }

        std::pair<DisruptionEffect, int> costPair = cancelCosts[0];
        int costOnCancel = (stackTarget.size() - effectiveStackTarget.size()) * costPair.second;
        CyberParameter costParam = disruptionEffectToCyberParameter(costPair.first);

        if (state.params.getParamAmount(costParam) < std::abs(costOnCancel))
            return ActionResult::invalid("Not enough resources to cancel");

        // ! Apply cancel cost to GameState
        resolveParamEffect(state, {costPair.first, costOnCancel});

        return std::nullopt;
    }; 

    if (category == "CatA") {
        auto err = tryCancelTiles(card, clientReq, tiles, cancelCosts, effectiveStackTarget);
        if (err.has_value())
            return err.value();
    }
    
    else if (category == "CatB") {
        if (card.getConditionType() == ConditionType::NONE) {
            for (auto e : card.getEffects())
                resolveParamEffect(state, e);
        } else {
            auto err = tryCancelTiles(card, clientReq, tiles, cancelCosts, effectiveStackTarget);
            if (err.has_value())
                return err.value();

            if (card.getConditionType() == ConditionType::STACK) {
                auto err = tryCancelTiles(card, clientReq, tiles, cancelCosts, effectiveStackTarget);
                if (err.has_value())
                    return err.value();
            } else {
                if (card.getResourceCondition().has_value()) {
                    ResourceCondition cond = card.getResourceCondition().value();
                    if (checkResourceCondition(state, cond)) {
                        for (auto e : card.getEffects())
                            resolveParamEffect(state, e);
                    }

                }
            }
        } 
    }

    else if (category == "CatC" || category == "CatD") {
        auto err = tryCancelTiles(card, clientReq, tiles, cancelCosts, effectiveStackTarget);
        if (err.has_value())
            return err.value();

        if (card.getConditionType() == ConditionType::STACK && 
            card.getStackCondition().has_value()) {
            std::set<StackType> stackTypeAffected(
                                            card.getStackCondition().value().stackTypes.begin(),
                                            card.getStackCondition().value().stackTypes.end()
                                            );
            std::vector<int> filteredTarget;
            for (auto e : effectiveStackTarget) {
                if (stackTypeAffected.count(state.board[e].getEffectiveType()))
                    filteredTarget.push_back(e);
            }
            effectiveStackTarget = filteredTarget;
        }
    }

    else if (category == "CatE") {
        if (clientReq.find("cancel") != clientReq.end()) {
            state.currentGoal = state.goalManager.draw();
        } else {

            // ! One Cost only, therefore hard code for now
            std::pair<DisruptionEffect, int> costPair = cancelCosts[0];
            CyberParameter costParam = disruptionEffectToCyberParameter(costPair.first);

            if (state.params.getParamAmount(costParam) < std::abs(costPair.second))
                return ActionResult::invalid("Not enough resources to cancel");
            resolveParamEffect(state, costPair);
        }
    }

    // ! Boost: Gain Resource  
    else if (category == "CatF") {
        for (auto e : card.getEffects())
            resolveParamEffect(state, e);
    }

    // ! Apply stack effect
    if (!effectiveStackTarget.empty()) {

        for (auto t : effectiveStackTarget) {
            for (auto& e : card.getEffects()) {
                switch (e.first) {
                    case DisruptionEffect::TURN_WASTE:
                        changeTileStack(state, t, StackType::WASTE);
                        break;
                    case DisruptionEffect::TURN_WILD:
                        changeTileStack(state, t, StackType::WILD);
                        break;
                    // Param effects: apply once per matching tile
                    case DisruptionEffect::COHESION:
                    case DisruptionEffect::HUMAN_RELATION:
                    case DisruptionEffect::CYBERNATION:
                    case DisruptionEffect::TECHNOLOGY:
                    case DisruptionEffect::ENVIRONMENT:
                        resolveParamEffect(state, e);
                        break;
                    default:
                        break;
                }
            }
        }

    }
    return ActionResult::success(ActionMessage("resolve_disruption", "Disruption is resolved"));
}


ActionResult GameUtility::cancelDisruptionEffect(GameState& state)
{
    if (!state.activeDisruption.has_value()) {
        return {ActionStatus::INVALID_ACTION, {
            "cancel_disruption",
            "Disruption Card is not drawn"
        }};
    }

    DisruptionCard card = state.activeDisruption.value();

    if (!card.isCancellable()) {
        return {ActionStatus::INVALID_ACTION, {
            "cancel_disruption",
            "This card cannot be cancelled"
        }};
    }

    /* Apply Resource Effect to GameState */
    const auto& cancelCosts = card.getCosts();
    for (const auto& cost: cancelCosts)
        resolveParamEffect(state, cost);
    
    state.activeDisruption = std::nullopt;
    return {ActionStatus::SUCCESS, {
        "cancel_disruption",
        "Disruption Card is cancelled"
    }};
}

ActionResult GameUtility::tradeForDisruption(GameState& state, const Action& action)
{
    auto sourceIt = action.params.find("source");
    if (sourceIt == action.params.end()) {
        return {ActionStatus::INVALID_ACTION, {"error", "trade requires source=disruption_cancel (or disruption_effect_cancel)"}};
    }
    const std::string& source = sourceIt->second;
    if (source != "disruption_cancel" && source != "disruption_effect_cancel") {
        return {ActionStatus::INVALID_ACTION, {"error", "trade is restricted to disruption cancel flows"}};
    }

    auto giveParamIt = action.params.find("give_param");
    auto recvParamIt = action.params.find("receive_param");
    if (giveParamIt == action.params.end() || recvParamIt == action.params.end()) {
        return {ActionStatus::INVALID_ACTION, {"error", "trade requires params: give_param, receive_param"}};
    }

    int giveAmount = 1;
    int recvAmount = 1;
    auto giveAmtIt = action.params.find("give_amount");
    auto recvAmtIt = action.params.find("receive_amount");
    if (giveAmtIt != action.params.end()) {
        try { giveAmount = std::stoi(giveAmtIt->second); } catch (...) { return {ActionStatus::INVALID_ACTION, {"error", "give_amount must be integer"}}; }
    }
    if (recvAmtIt != action.params.end()) {
        try { recvAmount = std::stoi(recvAmtIt->second); } catch (...) { return {ActionStatus::INVALID_ACTION, {"error", "receive_amount must be integer"}}; }
    }
    if (giveAmount <= 0 || recvAmount <= 0) {
        return {ActionStatus::INVALID_ACTION, {"error", "trade amounts must be > 0"}};
    }

    CyberParameter giveParam;
    CyberParameter recvParam;
    if (!parseCyberParameter(giveParamIt->second, giveParam) ||
        !parseCyberParameter(recvParamIt->second, recvParam)) {
        return {ActionStatus::INVALID_ACTION, {"error", "Unknown trade parameter"}};
    }
    if (giveParam == recvParam) {
        return {ActionStatus::INVALID_ACTION, {"error", "give_param and receive_param must differ"}};
    }

    if (giveParam == CyberParameter::COHESION || recvParam == CyberParameter::COHESION) {
        return {ActionStatus::INVALID_ACTION, {"error", "Cohesion cannot be used in trade"}};
    }

    const int beforeGive = state.params.getParamAmount(giveParam);
    const int beforeRecv = state.params.getParamAmount(recvParam);
    if (beforeGive < giveAmount) {
        return {ActionStatus::INSUFFICIENT_RESOURCE, {"error", "Not enough resource to trade"}};
    }

    state.params.adjustParam(giveParam, -giveAmount);
    state.params.adjustParam(recvParam, recvAmount);

    const int afterGive = state.params.getParamAmount(giveParam);
    const int afterRecv = state.params.getParamAmount(recvParam);

    nlohmann::json payload = {
        {"giveParam", cyberParameterToLabel(giveParam)},
        {"giveAmount", giveAmount},
        {"receiveParam", cyberParameterToLabel(recvParam)},
        {"receiveAmount", recvAmount},
        {"before", {
            {"give", beforeGive},
            {"receive", beforeRecv}
        }},
        {"after", {
            {"give", afterGive},
            {"receive", afterRecv}
        }}
    };

    return ActionResult::success(ActionMessage("adapt_disruption_trade", payload.dump()));
}