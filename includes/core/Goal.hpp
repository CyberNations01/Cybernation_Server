#ifndef GOAL_HPP
#define GOAL_HPP

#include <string>
#include <vector>
#include <map>
#include "core/Types.hpp"
#include <optional>

struct victory_condition {
    std::string type;
    comparator op = comparator::EQ;
    int num = 0;
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
        Goal() : id(0), reverseGoalId(0), name("") {}

        const int& getId() const;
        const int& getReverseGoalId() const;
        const std::string& getName() const;
        const std::vector<victory_condition>& getConditions() const;
        const std::map<StackType, std::vector<int>>& getStackEffect() const;

        void setId(int value);
        void setReverseGoalId(int value);
        void setName(const std::string& value);
        void setCondition(const std::vector<victory_condition>& value);
        void setStackEffect(const std::map<StackType, std::vector<int>>& value);
};

#endif
