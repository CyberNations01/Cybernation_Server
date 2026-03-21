#ifndef DISRUPTION_RESOLVER_HPP
#define DISRUPTION_RESOLVER_HPP
#include "core/Types.hpp"
#include "core/ActionResult.hpp"
#include "core/Action.hpp"
#include "core/DisruptionCard.hpp"
#include "game/GameState.hpp"

class GameUtility {
    public:
        static ActionResult walkPath(GameState& state);    

        static ActionResult drawDisruption(GameState& state);

        static ActionResult applyDisruptionEffect(GameState& state, const Action& action);
        static ActionResult cancelDisruptionEffect(GameState& state);
        static void changeTileStack(GameState& state, int tilePos, StackType targetType);


    private:
        // ! WalkPath Internal
        static nlohmann::json pathResultToJson(
                    const int& tile, const int& side, 
                    const std::vector<std::string> & resources, 
                    const std::string& layer);

        // ! Disruption Card Internal
        static bool checkResourceCondition(GameState& state, ResourceCondition cond);
        static bool isStackEffect(const DisruptionEffect& e);
        static void resolveParamEffect(GameState & state, const std::pair<DisruptionEffect, int>& effect);
        static bool resolveResourceEffect(GameState& state, 
                                          const std::vector<std::pair<DisruptionEffect, int>>& effect,
                                          int limit);

};

#endif