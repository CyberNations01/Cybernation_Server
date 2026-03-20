#include "core/Player.hpp"
#include <algorithm>

bool Player::removeFromHand(TokenEffect token) {
    auto it = std::find(hand.begin(), hand.end(), token);
    if (it != hand.end()) {
        hand.erase(it);
        return true;
    }
    return false;
}
