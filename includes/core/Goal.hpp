#ifndef GOAL_HPP
#define GOAL_HPP

#include <string>
#include <vector>
#include <map>
#include "Types.hpp"
#include <optional>

enum class comparator {
    GT, GE, EQ, LE, LT, NE
};

class victory_condition {
    private:
        std::string type;
        comparator op;
        int num;
        std::optional<std::string> position;
};

class Goal {
    private:
        int id;
        int reverseGoalId;
        std::string name;
        std::vector<victory_condition> conditions;
        std::map<StackType, std::vector<int>> stackEffect;

    public:
        const int& getId() const;
        const int& getReverseGoalId() const;
        const int& getName() const;
        const std::vector<victory_condition>& getConditions() const;
        const std::map<StackType, std::vector<int>>& getStackEffect() const;
};

#endif
