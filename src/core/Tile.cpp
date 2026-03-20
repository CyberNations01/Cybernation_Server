#include "core/Tile.hpp"
#include "Tile.hpp"

const StackType &Tile::getEffectiveType() const
{
    if (hasOverlay()) return overlay->getType();
    return base.getType();
}

const int &Tile::getNeighbourTile(const int &side) const
{
    return neighbours[side].first;
}

const int &Tile::getNeighbourSide(int side) const
{
    return neighbours[side].first;
}

const std::vector<std::string> &Tile::getSideResources(int side) const
{
    std::vector<std::string> all_resources = base.getSides()[side];
    if (hasOverlay()) {
        all_resources.insert(all_resources.end(), 
                             overlay->getSides()[side].begin(),
                             overlay->getSides()[side].end());
    }
    return all_resources;
}

std::optional<Stack> Tile::removeOverlay()
{
    if (!overlay.has_value()) return std::nullopt;
    Stack removed = overlay.value();
    overlay.reset();
    return removed;
}