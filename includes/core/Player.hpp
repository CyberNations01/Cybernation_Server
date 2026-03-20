#ifndef PLAYER_HPP
#define PLAYER_HPP
#include <vector>
#include "FeedbackPool.hpp"

class Player {
private:
    int id;
    bool hasFirstPlayerToken = false;

    // Players can hold drawn feedback tokens in hand
    std::vector<TokenEffect> hand;

public:
    Player() : id(-1) {}
    Player(int id) : id(id) {}
    ~Player() = default;

    int  getId() const { return id; }
    
    // First player token
    bool isFirstPlayer()         const { return hasFirstPlayerToken; }
    void setFirstPlayer(bool val)      { hasFirstPlayerToken = val; }

    // Hand management
    const std::vector<TokenEffect>& getHand() const { return hand; }
    void addToHand(TokenEffect token)    { hand.push_back(token); }
    bool removeFromHand(TokenEffect token);
    void clearHand()                     { hand.clear(); }
};

#endif
