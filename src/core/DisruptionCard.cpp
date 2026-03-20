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


std::ostream &operator<<(std::ostream &os, DisruptionEffect eff)
{
    if (eff == DisruptionEffect::TURN_WASTE )  return  os << "TurnWaste";
    if (eff == DisruptionEffect::TURN_WILD)   return  os << "TurnWild";
    if (eff == DisruptionEffect::TURN_DEV_A)   return  os << "TurnDevA";
    if (eff == DisruptionEffect::TURN_DEV_B)   return os << "TurnDevB";
    if (eff == DisruptionEffect::COHESION)         return  os << "Co";
    if (eff == DisruptionEffect::HUMAN_RELATION)         return  os << "HR";
    if (eff == DisruptionEffect::CYBERNATION)         return  os << "Cy";
    if (eff == DisruptionEffect::TECHNOLOGY)       return os << "Tech";
    if (eff == DisruptionEffect::ENVIRONMENT)        return os << "Env";
    if (eff == DisruptionEffect::RESOURCES)  return os << "Resources";
    if (eff == DisruptionEffect::TOKEN)      return os << "Token";
    if (eff == DisruptionEffect::TRADE)      return os << "Trade";
    if (eff == DisruptionEffect::CAP_ENV)     return os << "CapEnv";
    if (eff == DisruptionEffect::IGNORE_COHESION_EFFECT) return os << "IgnoreCohesionEffect";
    if (eff == DisruptionEffect::SWAP_GOAL)   return os << "SwapGoal";
    if (eff == DisruptionEffect::DRAW_GOAL)   return os << "DrawGoal";
    if (eff == DisruptionEffect::MOVE_PEOPLE) return os << "MovPpl";
    return os << "Co"; 
}