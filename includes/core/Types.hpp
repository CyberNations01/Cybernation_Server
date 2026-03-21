#ifndef TYPES_HPP
#define TYPES_HPP
#include <string>

enum class StackType {
    WILD,
    WASTE,
    DEV_A,
    DEV_B,
    UNKNOWN
};

inline StackType strtoStackType(std::string str)
{
    if (str == "Wild")
        return StackType::WILD;
    else if (str == "Waste")
        return StackType::WASTE;
    else if (str == "DevA")
        return StackType::DEV_A;
    else if (str == "DevB")
        return StackType::DEV_B;
    return StackType::UNKNOWN;
}

inline std::string stackTypeToStr(StackType t) {
    switch (t) {
        case StackType::WILD:    return "Wild";
        case StackType::WASTE:   return "Waste";
        case StackType::DEV_A:   return "DevA";
        case StackType::DEV_B:   return "DevB";
        default:                 return "Unknown";
    }
}


enum class TokenEffect {
    TURN_WILD,
    LOSE_COHESION,
    TURN_WASTE,
    SOLVE_DISRUPTION,
    DEVELOP_STACK,
    TRANSFORM_STACK,
    UNKNOWN
};

enum class GamePhase {
    ENVISION,
    TRAVERSE,
    ADOPT,
};

enum class DisruptionType {
    DISRUPT,
    BOOST
};

inline DisruptionType strtoDisruptionType(std::string str)
{
    if (str == "disrupt")
        return DisruptionType::DISRUPT;
    return DisruptionType::BOOST;
}

enum class DisruptionEffect {
    // Stack changes
    TURN_WASTE,
    TURN_WILD,
    TURN_DEV_A,
    TURN_DEV_B,

    // Resource changes
    COHESION,
    HUMAN_RELATION,
    CYBERNATION,
    TECHNOLOGY,
    ENVIRONMENT,
    RESOURCES,
    TOKEN,
    TRADE,

    // Rule modifiers
    CAP_ENV,
    IGNORE_COHESION_EFFECT,

    // Meta
    SWAP_GOAL,
    DRAW_GOAL,
    MOVE_PEOPLE
};

inline DisruptionEffect strtoDisruptionEffect(std::string str)
{
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


enum class ActionStatus {
    SUCCESS,            // Action executed successfully
    INVALID_ACTION,     // Action not allowed in this phase
    INVALID_TARGET,     // Target (stack, player, etc.) is invalid
    INSUFFICIENT_RESOURCE, // Not enough resources to perform action
    NOT_YOUR_TURN,      // Silently ignored by Round Controller
    PLAYER_ALREADY_PASSED, // Player has already passed this phase
    GAME_OVER,          // Game has ended
    UNKNOWN_ERROR
};

enum class comparator {
    GT, GE, EQ, LE, LT, NE
};

inline comparator strToComparator(const std::string& str)
{
    if (str == "GT") return comparator::GT;
    if (str == "GE") return comparator::GE;
    if (str == "EQ") return comparator::EQ;
    if (str == "LE") return comparator::LE;
    if (str == "LT") return comparator::LT;
    if (str == "NE") return comparator::NE;
    return comparator::EQ;
}

inline std::string comparatorToStr(const comparator& op)
{
    switch (op) {
        case comparator::GT: return "GT";
        case comparator::GE: return "GE";
        case comparator::EQ: return "EQ";
        case comparator::LE: return "LE";
        case comparator::LT: return "LT";
        case comparator::NE: return "NE";
        default:             return "EQ";
    }
}

#endif