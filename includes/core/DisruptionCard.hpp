#ifndef DISRUPTION_CARD_HPP
#define DISRUPTION_CARD_HPP

#include <string>
#include <vector>
#include "nlohmann/json.hpp"
#include "Stack.hpp"
#include "Params.hpp"
#include "Types.hpp"

class DisruptionCard {
private:
    std::string     name;
    std::string     description;
    DisruptionType  type;

    std::vector<int>                              stackTargets;
    std::vector<std::pair<DisruptionEffect, int>> effects;
    std::vector<std::pair<DisruptionEffect, int>> cancelCosts;

    std::vector<StackType>      stackConditions;
    std::vector<CyberParameter> relationConditions;
    nlohmann::json optionalData;

    bool hasCondition;
    bool cancellable;

public:
    DisruptionCard();
    ~DisruptionCard() = default;

    
    const std::string&                                   getName()               const { return name; }
    const std::string&                                   getDescription()        const { return description; }
    DisruptionType                                       getType()               const { return type; }
    const std::vector<int>&                              getStackTargets()       const { return stackTargets; }
    const std::vector<std::pair<DisruptionEffect, int>>& getEffects()            const { return effects; }
    const std::vector<std::pair<DisruptionEffect, int>>& getCancelCosts()        const { return cancelCosts; }
    const std::vector<StackType>&                        getStackConditions()    const { return stackConditions; }
    const std::vector<CyberParameter>&                   getRelationConditions() const { return relationConditions; }
    const nlohmann::json&                                getOptionalData() const { return optionalData; }

    void setName(const std::string& n)                                          { name = n; }
    void setDescription(const std::string& d)                                   { description = d; }
    void setType(DisruptionType t)                                              { type = t; }
    void setStackTargets(const std::vector<int>& t)                             { stackTargets = t; }
    void setEffects(const std::vector<std::pair<DisruptionEffect, int>>& e)     { effects = e; }
    void setCancelCosts(const std::vector<std::pair<DisruptionEffect, int>>& c) { cancelCosts = c; }
    void setStackConditions(const std::vector<StackType>& s)                    { stackConditions = s; }
    void setRelationConditions(const std::vector<CyberParameter>& r)            { relationConditions = r; }
    void setCancellable(bool c)                                                 { cancellable = c; }
    void setHasCondition(bool c)                                                { hasCondition = c; }
    void setOptionalData(const nlohmann::json& o)                               { optionalData = o; }


    bool isCancellable() const { return cancellable; }

    // Helpers
    bool hasTileChangeEffect() const;
    
    // Parsing
    static DisruptionEffect parseEffectString(const std::string& str);
};


#endif
