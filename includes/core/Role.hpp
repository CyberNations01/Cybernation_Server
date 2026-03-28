#ifndef ROLE_HPP
#define ROLE_HPP

#include <string>
#include <vector>

class Role {
private:
    int id = -1;
    std::string name;
    std::string agendaRaw;
    std::vector<std::string> agendaCondition;

public:
    Role() = default;
    ~Role() = default;

    int getId() const { return id; }
    const std::string& getName() const { return name; }
    const std::string& getAgendaRaw() const { return agendaRaw; }
    const std::vector<std::string>& getAgendaCondition() const { return agendaCondition; }

    void setId(int nextId) { id = nextId; }
    void setName(const std::string& nextName) { name = nextName; }
    void setAgendaRaw(const std::string& nextAgendaRaw) { agendaRaw = nextAgendaRaw; }
    void setAgendaCondition(const std::vector<std::string>& nextCondition) { agendaCondition = nextCondition; }
};

#endif
