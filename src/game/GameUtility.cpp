#include "game/GameUtility.hpp"
#include <sstream>

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
    int delta = effect.second; 
    switch (effect.first) {
        case DisruptionEffect::COHESION:
            if (delta < 0 && state.ignoreCohesionLossThisRound)
                break;
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
        default:
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

    auto applyResourceDistribution = [&](const int limit) -> std::optional<ActionResult>
    {
        if (limit < 0)
            return ActionResult::invalid("Invalid resource distribution limit");

        int assignedTotal = 0;
        std::vector<std::pair<CyberParameter, int>> adjustments;

        for (const auto& [rawParam, rawValue] : clientReq) {
            CyberParameter param;
            if (!parseCyberParameter(rawParam, param))
                continue;

            if (param != CyberParameter::HUMAN_RELATION &&
                param != CyberParameter::TECHNOLOGY &&
                param != CyberParameter::ENVIRONMENT) {
                return ActionResult::invalid("Resource distribution only allows HR, Tech, Env");
            }

            int amount = 0;
            try {
                amount = std::stoi(rawValue);
            } catch (...) {
                return ActionResult::invalid("Resource distribution values must be integers");
            }

            if (amount < 0)
                return ActionResult::invalid("Resource distribution values must be >= 0");

            if (amount == 0)
                continue;

            assignedTotal += amount;
            adjustments.push_back({param, amount});
        }

        if (assignedTotal != limit) {
            return ActionResult::invalid(
                "Resource distribution must sum to " + std::to_string(limit)
            );
        }

        for (const auto& [param, amount] : adjustments)
            state.params.adjustParam(param, amount);

        return std::nullopt;
    };

    auto parseIntParam = [&](const std::string& key, int& out) -> bool {
        auto it = clientReq.find(key);
        if (it == clientReq.end()) return false;
        try {
            out = std::stoi(it->second);
            return true;
        } catch (...) {
            return false;
        }
    };

    auto parseBoolParam = [&](const std::string& key) -> bool {
        auto it = clientReq.find(key);
        if (it == clientReq.end()) return false;
        const std::string& v = it->second;
        return (v == "1" || v == "true" || v == "TRUE" || v == "yes" || v == "YES");
    };

    auto isParamEffect = [&](const DisruptionEffect e) -> bool {
        return (e == DisruptionEffect::COHESION ||
                e == DisruptionEffect::HUMAN_RELATION ||
                e == DisruptionEffect::CYBERNATION ||
                e == DisruptionEffect::TECHNOLOGY ||
                e == DisruptionEffect::ENVIRONMENT);
    };

    auto applyParamEffects = [&](const std::vector<std::pair<DisruptionEffect, int>>& effects, int multiplier = 1) {
        for (const auto& e : effects) {
            if (!isParamEffect(e.first)) continue;
            resolveParamEffect(state, {e.first, e.second * multiplier});
        }
    };

    auto hasEnoughResourceForEffects =
        [&](const std::vector<std::pair<DisruptionEffect, int>>& effects) -> bool {
        for (const auto& e : effects) {
            if (!isParamEffect(e.first)) continue;
            CyberParameter param = disruptionEffectToCyberParameter(e.first);
            if (e.second < 0 && state.params.getParamAmount(param) < std::abs(e.second))
                return false;
        }
        return true;
    };

    auto filterTargetsByStackCondition = [&](const std::vector<int>& candidates,
                                             const DisruptionCard& card) -> std::vector<int> {
        if (card.getConditionType() != ConditionType::STACK || !card.getStackCondition().has_value())
            return candidates;

        const auto& allow = card.getStackCondition().value().stackTypes;
        std::vector<int> filtered;
        for (const int t : candidates) {
            if (t < 0 || t >= static_cast<int>(state.board.size())) continue;
            StackType cur = state.board[t].getEffectiveType();
            for (const auto st : allow) {
                if (cur == st) {
                    filtered.push_back(t);
                    break;
                }
            }
        }
        return filtered;
    };

    auto parseChosenEffect = [&](const DisruptionCard& card) -> std::optional<std::pair<DisruptionEffect, int>> {
        const auto& effects = card.getEffects();
        if (effects.empty())
            return std::nullopt;

        int idx = 0;
        auto it = clientReq.find("effectIndex");
        if (it != clientReq.end()) {
            try {
                idx = std::stoi(it->second);
            } catch (...) {
                return std::nullopt;
            }
        }

        if (idx < 0 || idx >= static_cast<int>(effects.size()))
            return std::nullopt;
        return effects[idx];
    };

    auto findDefaultTargetTile = [&](const DisruptionCard& card) -> int {
        for (const int t : card.getStackTargets()) {
            if (t >= 0 && t < static_cast<int>(state.board.size()))
                return t;
        }
        return -1;
    };

    auto applyEffectToTarget = [&](const std::pair<DisruptionEffect, int>& e, const int t) {
        switch (e.first) {
            case DisruptionEffect::TURN_WASTE:
                changeTileStack(state, t, StackType::WASTE);
                break;
            case DisruptionEffect::TURN_WILD:
                changeTileStack(state, t, StackType::WILD);
                break;
            case DisruptionEffect::TURN_DEV_A:
                changeTileStack(state, t, StackType::DEV_A);
                break;
            case DisruptionEffect::TURN_DEV_B:
                changeTileStack(state, t, StackType::DEV_B);
                break;
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
    };

    if (category == "CatA") {
        auto err = tryCancelTiles(card, clientReq, tiles, cancelCosts, effectiveStackTarget);
        if (err.has_value())
            return err.value();
    }
    
    else if (category == "CatB") {
        if (card.getConditionType() == ConditionType::NONE) {
            applyParamEffects(card.getEffects());
        } else {
            auto err = tryCancelTiles(card, clientReq, tiles, cancelCosts, effectiveStackTarget);
            if (err.has_value())
                return err.value();

            if (card.getConditionType() == ConditionType::STACK) {
                effectiveStackTarget = filterTargetsByStackCondition(effectiveStackTarget, card);
                for (const int t : effectiveStackTarget) {
                    (void)t;
                    applyParamEffects(card.getEffects());
                }
            } else {
                if (card.getResourceCondition().has_value()) {
                    ResourceCondition cond = card.getResourceCondition().value();
                    if (checkResourceCondition(state, cond)) {
                        applyParamEffects(card.getEffects());
                    }

                }
            }
        } 
    }

    else if (category == "CatC" || category == "CatD") {
        auto err = tryCancelTiles(card, clientReq, tiles, cancelCosts, effectiveStackTarget);
        if (err.has_value())
            return err.value();

        effectiveStackTarget = filterTargetsByStackCondition(effectiveStackTarget, card);
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
        for (const auto& e : card.getEffects()) {
            if (isParamEffect(e.first)) {
                resolveParamEffect(state, e);
                continue;
            }
            if (e.first == DisruptionEffect::RESOURCES) {
                auto err = applyResourceDistribution(e.second);
                if (err.has_value())
                    return err.value();
            }
        }
    }

    else if (category == "CatG") {
        auto chosenOpt = parseChosenEffect(card);
        if (!chosenOpt.has_value())
            return ActionResult::invalid("CatG requires valid effectIndex");

        auto chosen = chosenOpt.value();
        int multiplier = static_cast<int>(filterTargetsByStackCondition(card.getStackTargets(), card).size());
        if (multiplier <= 0)
            return ActionResult::success(ActionMessage("resolve_disruption", "Disruption is resolved"));

        if (chosen.first == DisruptionEffect::RESOURCES) {
            auto err = applyResourceDistribution(chosen.second * multiplier);
            if (err.has_value()) return err.value();
        } else if (isParamEffect(chosen.first)) {
            resolveParamEffect(state, {chosen.first, chosen.second * multiplier});
        }
    }

    else if (category == "CatH") {
        auto chosenOpt = parseChosenEffect(card);
        if (!chosenOpt.has_value())
            return ActionResult::invalid("CatH requires valid effectIndex");

        auto chosen = chosenOpt.value();
        const bool hasEffectIndex = (clientReq.find("effectIndex") != clientReq.end());
        if (!hasEffectIndex &&
            (chosen.first == DisruptionEffect::MOVE_PEOPLE ||
             chosen.first == DisruptionEffect::TOKEN ||
             chosen.first == DisruptionEffect::IGNORE_COHESION_EFFECT)) {
            for (const auto& e : card.getEffects()) {
                if (isParamEffect(e.first)) {
                    chosen = e;
                    break;
                }
            }
        }

        switch (chosen.first) {
            case DisruptionEffect::COHESION:
            case DisruptionEffect::HUMAN_RELATION:
            case DisruptionEffect::CYBERNATION:
            case DisruptionEffect::TECHNOLOGY:
            case DisruptionEffect::ENVIRONMENT:
                resolveParamEffect(state, chosen);
                break;
            case DisruptionEffect::MOVE_PEOPLE: {
                int targetTile = -1;
                int targetSide = -1;
                if (!parseIntParam("moveTo_tile", targetTile) ||
                    !parseIntParam("moveTo_side", targetSide)) {
                    return ActionResult::invalid("MovePpl requires moveTo_tile and moveTo_side");
                }
                if (targetTile < 0 || targetTile >= static_cast<int>(state.board.size()) ||
                    targetSide < 0 || targetSide >= 6) {
                    return ActionResult::invalid("Invalid people token destination");
                }
                state.setPeopleToken({targetTile, targetSide});
                break;
            }
            case DisruptionEffect::TOKEN:
                // Token behavior is intentionally deferred in current milestone.
                // Keep non-breaking no-op for now.
                break;
            case DisruptionEffect::IGNORE_COHESION_EFFECT:
                state.ignoreCohesionLossThisRound = true;
                break;
            default:
                return ActionResult::invalid("Unsupported CatH effect");
        }
    }

    else if (category == "CatI") {
        int targetTile = -1;
        if (!parseIntParam("targetTile", targetTile))
            targetTile = findDefaultTargetTile(card);

        if (targetTile < 0 || targetTile >= static_cast<int>(state.board.size()))
            return ActionResult::invalid("Invalid targetTile");

        bool inAllowedTarget = false;
        for (const int t : card.getStackTargets()) {
            if (t == targetTile) {
                inAllowedTarget = true;
                break;
            }
        }
        if (!inAllowedTarget)
            return ActionResult::invalid("targetTile is not in stackTarget");

        if (!hasEnoughResourceForEffects(card.getCosts()))
            return ActionResult::invalid("Not enough resources for CatI cost");
        for (const auto& c : card.getCosts())
            resolveParamEffect(state, c);

        effectiveStackTarget = {targetTile};
    }

    else if (category == "CatJ") {
        effectiveStackTarget = filterTargetsByStackCondition(card.getStackTargets(), card);

        if (parseBoolParam("useOptional") && card.hasOptional()) {
            if (!hasEnoughResourceForEffects(card.getOptionalCosts()))
                return ActionResult::invalid("Not enough resources for optional cost");
            for (const auto& c : card.getOptionalCosts())
                resolveParamEffect(state, c);
            for (const auto& g : card.getOptionalGains())
                resolveParamEffect(state, g);
        }
    }

    else if (category == "CatK") {
        // Intentionally no-op for now (temporary product decision).
    }

    // ! Apply stack effect
    if (!effectiveStackTarget.empty()) {

        for (auto t : effectiveStackTarget) {
            for (auto& e : card.getEffects()) {
                applyEffectToTarget(e, t);
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