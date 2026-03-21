#include "game/GameUtility.hpp"

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

/*
    @type: "cancel", "apply"
*/

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
    }
}

bool GameUtility::resolveResourceEffect(GameState& state, 
                                        const std::vector<std::pair<DisruptionEffect, int>>& effect,
                                        int limit)
{
    int limit_counter = 0;
    for (const auto& e: effect) {
        resolveParamEffect(state, e);
        limit_counter += e.second;
        if (limit_counter > limit)
            return false;
    }
    return true;
}


void GameUtility::changeTileStack(GameState& state, 
                                int tilePos,
                                StackType targetType)
{
    std::cout << "Tile Pos: " << tilePos << "Stack Type: " << stackTypeToStr(targetType) << std::endl;;
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

ActionResult GameUtility::applyDisruptionEffect(GameState& state, const Action& action)
{
    const DisruptionCard& card = state.activeDisruption.value();

    // ─── Step 1: Check preconditions ────────────────────────────────
    // ResourceCondition cards only fire if the param comparison holds
    if (card.getConditionType() == ConditionType::RESOURCE) {
        const auto& condition = card.getResourceCondition().value();
        if (!checkResourceCondition(state, condition)) {
            
            return {ActionStatus::SUCCESS, {
                "resolve_disruption",
                "Disruption applied"
            }};
        }
    }

    // ─── Step 2: Filter target tiles by stack condition ─────────────
    // StackCondition cards only affect tiles whose effective type matches
    std::vector<int> affectedTiles;

    if (card.getConditionType() == ConditionType::STACK) {
        const StackCondition& condition = card.getStackCondition().value();
        for (int tilePos : card.getStackTargets()) {
            StackType tileType = state.board[tilePos].getEffectiveType();
            for (const StackType& requiredType : condition.stackTypes) {
                if (tileType == requiredType) {
                    affectedTiles.push_back(tilePos);
                    break;
                }
            }
        }
    } else {
        affectedTiles = card.getStackTargets();
    }
    std::cout << "  DEBUG affectedTiles: ";
    for (int t : affectedTiles) std::cout << t << " ";
    std::cout << std::endl;


    // ─── Step 3: Determine which effects to apply ───────────────────
    // OR: player picks one effect; AND/NONE: all effects apply in order
    const auto& allEffects = card.getEffects();
    std::vector<std::pair<DisruptionEffect, int>> chosenEffects;

    if (card.getEffectCond() == EffectCondition::OR) {
        int chosenIndex = std::stoi(action.params.at("effectIndex"));
        if (chosenIndex >= 0 && chosenIndex < static_cast<int>(allEffects.size()))
            chosenEffects.push_back(allEffects[chosenIndex]);
    } else {
        chosenEffects = allEffects;
    }

    for (const auto& [effect, value] : chosenEffects)
        std::cout << "  DEBUG effect: " << static_cast<int>(effect) << " value: " << value << std::endl;

    for (const auto& [effect, value] : allEffects)
        std::cout << "  DEBUG all effect: " << static_cast<int>(effect) << " value: " << value << std::endl;

    // ─── Step 4: Apply effects ──────────────────────────────────────
    // Separate into param effects (applied immediately) and
    // tile effects (applied to each affected tile afterwards)
    std::vector<std::pair<DisruptionEffect, int>> tileEffects;

    for (const auto& [effect, value] : chosenEffects) {

        if (isStackEffect(effect)) {
            tileEffects.push_back({effect, value});
            continue;
        }

        switch (effect) {

            // Player distributes N total resources across params
            case DisruptionEffect::RESOURCES: {
                std::vector<std::pair<DisruptionEffect, int>> distribution;
                for (const auto& [paramKey, paramVal] : action.params) {
                    if (paramKey == "effectIndex") continue;
                    try {
                        distribution.push_back({strtoDisruptionEffect(paramKey), std::stoi(paramVal)});
                    } catch (const std::exception&) {
                        std::cerr << "Cannot parse resource param: " << paramKey << std::endl;
                    }
                }
                resolveResourceEffect(state, distribution, value);
                break;
            }

            case DisruptionEffect::DRAW_GOAL:
                state.currentGoal = state.goalManager.draw();
                break;

            case DisruptionEffect::SWAP_GOAL:
                state.currentGoal = state.goalManager.getDeck()[state.currentGoal.getReverseGoalId()];
                break;

            case DisruptionEffect::TOKEN:
                // TODO: draw feedback tokens from bag
                break;

            case DisruptionEffect::MOVE_PEOPLE:
                // TODO: reposition people token
                break;

            case DisruptionEffect::CAP_ENV:
                // TODO: set environment cap for this turn
                break;

            case DisruptionEffect::IGNORE_COHESION_EFFECT:
                // TODO: disable cohesion capping for this turn
                break;

            // Standard param adjustment (Co, HR, Cy, Tech, Env)
            default: {
                int amount = value;
                if (card.getConditionType() == ConditionType::STACK)
                    amount *= static_cast<int>(affectedTiles.size());
                resolveParamEffect(state, {effect, amount});
                break;
            }
        }
    }

    // ─── Step 5: Apply tile-type changes to affected tiles ──────────
    std::cout << "  DEBUG tileEffects size: " << tileEffects.size() << std::endl;
    for (const auto& [effect, value] : tileEffects) {
        for (int tilePos : affectedTiles) {
            switch (effect) {
                case DisruptionEffect::TURN_WILD:
                    changeTileStack(state, tilePos, StackType::WILD);    break;
                case DisruptionEffect::TURN_WASTE:
                    changeTileStack(state, tilePos, StackType::WASTE);   break;
                case DisruptionEffect::TURN_DEV_A:
                    changeTileStack(state, tilePos, StackType::DEV_A);   break;
                case DisruptionEffect::TURN_DEV_B:
                    changeTileStack(state, tilePos, StackType::DEV_B);   break;
                default:
                    std::cerr << "Unknown tile effect: " << static_cast<int>(effect) << std::endl;
                    return {ActionStatus::UNKNOWN_ERROR, {
                        "resolve_disruption",
                        "Cannot Apply Tile Effect"
                    }};
            }
        }
    }

    return ActionResult::success(ActionMessage("resolve_disruption", "Disruption applied"));
}


ActionResult GameUtility::cancelDisruptionEffect(GameState& state)
{
    return {ActionStatus::SUCCESS, {
        "cancel_disruption",
        "Not Implemented"
    }};
}