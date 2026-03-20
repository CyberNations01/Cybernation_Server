#include "core/FeedbackPool.hpp"

FeedbackPool::FeedbackPool(int each)
    : turnWildTokens(each), loseCohesionTokens(each),
      turnWasteTokens(each), solveDisruptTokens(each) {}

FeedbackPool::FeedbackPool(int wild, int loseCoh, int waste, int solve)
    : turnWildTokens(wild), loseCohesionTokens(loseCoh),
      turnWasteTokens(waste), solveDisruptTokens(solve) {}

int FeedbackPool::getPoolSize() const {
    return turnWildTokens + loseCohesionTokens + turnWasteTokens + solveDisruptTokens;
}

int FeedbackPool::getTotalCapacity() const {
    // This could be tracked separately if needed
    return getPoolSize();
}

bool FeedbackPool::draw(TokenEffect token) {
    switch (token) {
        case TokenEffect::TURN_WILD:
            if (turnWildTokens > 0) { --turnWildTokens; return true; }
            break;
        case TokenEffect::LOSE_COHESION:
            if (loseCohesionTokens > 0) { --loseCohesionTokens; return true; }
            break;
        case TokenEffect::TURN_WASTE:
            if (turnWasteTokens > 0) { --turnWasteTokens; return true; }
            break;
        case TokenEffect::SOLVE_DISRUPTION:
            if (solveDisruptTokens > 0) { --solveDisruptTokens; return true; }
            break;
        default: break;
    }
    return false;
}

void FeedbackPool::putBack(TokenEffect token) {
    switch (token) {
        case TokenEffect::TURN_WILD:       ++turnWildTokens; break;
        case TokenEffect::LOSE_COHESION:   ++loseCohesionTokens; break;
        case TokenEffect::TURN_WASTE:      ++turnWasteTokens; break;
        case TokenEffect::SOLVE_DISRUPTION: ++solveDisruptTokens; break;
        default: break;
    }
}
