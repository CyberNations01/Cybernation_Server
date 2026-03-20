#ifndef ACTION_RESULT_HPP
#define ACTION_RESULT_HPP
#include <string>
#include "Types.hpp"

struct ActionMessage {
    std::string type;
    std::string payload;

    ActionMessage(const std::string& t, const std::string& p)
    : type(t), payload(p) {}
};

struct ActionResult {
    ActionStatus status;
    std::vector<ActionMessage> messages;

    ActionResult(ActionStatus s):  status(s) {}

    bool ok() const { return status == ActionStatus::SUCCESS;}

    void addMessage(const std::string& type, const std::string& payload) {
        messages.emplace_back(type, payload);
    }
    static ActionResult success(const std::string& msg = "OK") {
        return {ActionStatus::SUCCESS};
    }
    static ActionResult ignored() {
        return {ActionStatus::NOT_YOUR_TURN};
    }
    static ActionResult alreadyPassed() {
        return {ActionStatus::PLAYER_ALREADY_PASSED};
    }
};

#endif
