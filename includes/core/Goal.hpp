#ifndef GOAL_HPP
#define GOAL_HPP

#include <string>
#include <vector>
#include <map>
#include "core/Types.hpp"
#include <optional>

enum class comparator {
    GT, GE, EQ, LE, LT, NE
};

inline comparator strToComparator(const std::string& op) {
    if (op == "GT") return comparator::GT;
    if (op == "GE") return comparator::GE;
    if (op == "EQ") return comparator::EQ;
    if (op == "LE") return comparator::LE;
    if (op == "LT") return comparator::LT;
    if (op == "NE") return comparator::NE;
    return comparator::EQ;
}

inline std::string comparatorToStr(comparator op) {
    switch (op) {
        case comparator::GT: return "GT";
        case comparator::GE: return "GE";
        case comparator::EQ: return "EQ";
        case comparator::LE: return "LE";
        case comparator::LT: return "LT";
        case comparator::NE: return "NE";
        default: return "EQ";
    }
}

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
