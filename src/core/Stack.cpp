#include "core/Stack.hpp"

int Stack::getConnectedSide(int side)
{
    if (paths.find(side) == paths.end())
        return -1;
    return paths[side];
}

/**
 * rotate: Rotate a stack
 * 
 * @degree: Rotation degree
 */
void Stack::rotate(int steps)
{
    if (!steps)
        return;
        
    const int numSide = this->sides.size();
    steps %= numSide;

    /* Adjust for anti-clockwise */
    if (steps < 0)
        steps += numSide;

    /* Path map adjustment */
    std::map<int,int> newPath;
    for (const auto & [from, to]: this->paths) {
        int newFrom = (from + steps) % numSide;
        int newTo = (to + steps) % numSide;
        newPath[newFrom] = newTo;
    }
    paths = std::move(newPath);
    std::rotate(sides.begin(), sides.begin() + steps, sides.end());
}