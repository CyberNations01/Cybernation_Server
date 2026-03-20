#include "core/DisruptionCard.hpp"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <random>

DisruptionCard::DisruptionCard()
    : type(DisruptionType::DISRUPT), hasCondition(false), cancellable(false) {}

bool DisruptionCard::hasTileChangeEffect() const {
    for (const auto& eff : effects) {
        if (eff.first == DisruptionEffect::TURN_WASTE ||
            eff.first == DisruptionEffect::TURN_WILD  ||
            eff.first == DisruptionEffect::TURN_DEV_A ||
            eff.first == DisruptionEffect::TURN_DEV_B) {
            return true;
        }
    }
    return false;
}

DisruptionEffect DisruptionCard::parseEffectString(const std::string& str) {
    if (str == "TurnWaste")  return DisruptionEffect::TURN_WASTE;
    if (str == "TurnWild")   return DisruptionEffect::TURN_WILD;
    if (str == "TurnDevA")   return DisruptionEffect::TURN_DEV_A;
    if (str == "TurnDevB")   return DisruptionEffect::TURN_DEV_B;
    if (str == "Co")         return DisruptionEffect::COHESION;
    if (str == "HR")         return DisruptionEffect::HUMAN_RELATION;
    if (str == "Cy")         return DisruptionEffect::CYBERNATION;
    if (str == "Tech")       return DisruptionEffect::TECHNOLOGY;
    if (str == "Env")        return DisruptionEffect::ENVIRONMENT;
    if (str == "Resources")  return DisruptionEffect::RESOURCES;
    if (str == "Token")      return DisruptionEffect::TOKEN;
    if (str == "Trade")      return DisruptionEffect::TRADE;
    if (str == "CapEnv")     return DisruptionEffect::CAP_ENV;
    if (str == "IgnoreCohesionEffect") return DisruptionEffect::IGNORE_COHESION_EFFECT;
    if (str == "SwapGoal")   return DisruptionEffect::SWAP_GOAL;
    if (str == "DrawGoal")   return DisruptionEffect::DRAW_GOAL;
    if (str == "MovPpl")     return DisruptionEffect::MOVE_PEOPLE;
    return DisruptionEffect::COHESION; // fallback
}

