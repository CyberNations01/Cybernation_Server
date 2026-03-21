#include "core/Goal.hpp"

const int& Goal::getId() const {
    return id;
}

const int& Goal::getReverseGoalId() const {
    return reverseGoalId;
}

const std::string& Goal::getName() const {
    return name;
}

const std::vector<victory_condition>& Goal::getConditions() const {
    return conditions;
}

const std::map<StackType, std::vector<int>>& Goal::getStackEffect() const {
    return stackEffect;
}

void Goal::setId(int value) {
    id = value;
}

void Goal::setReverseGoalId(int value) {
    reverseGoalId = value;
}

void Goal::setName(const std::string& value) {
    name = value;
}

void Goal::setCondition(const std::vector<victory_condition>& value) {
    conditions = value;
}

void Goal::setStackEffect(const std::map<StackType, std::vector<int>>& value) {
    stackEffect = value;
}