#include "core/Params.hpp"
#include <algorithm>

Params::Params(int cyberLevel, int humanRel, int env, int tech, int coh)
    : cybernationLevel(cyberLevel), humanRelation(humanRel),
      environment(env), technology(tech), cohesion(coh) {}

void Params::setCohesion(const int& val) {
    if (val < 0 || val > 20) return;
    cohesion = val;
    // Cohesion caps all other resources
    humanRelation = std::min(humanRelation, cohesion);
    environment   = std::min(environment, cohesion);
    technology    = std::min(technology, cohesion);
}

void Params::setCybernationLevel(const int& val) {
    if (val < 0 || val > 20) return;
    cybernationLevel = val;
}

void Params::setHumanRelation(const int& val) {
    if (val < 0 || val > 20 || val > cohesion) return;
    humanRelation = val;
}

void Params::setEnvironment(const int& val) {
    if (val < 0 || val > 20 || val > cohesion) return;
    environment = val;
}

void Params::setTechnology(const int& val) {
    if (val < 0 || val > 20 || val > cohesion) return;
    technology = val;
}

void Params::adjustParam(CyberParameter param, int delta) {
    switch (param) {
        case CyberParameter::COHESION:
            setCohesion(cohesion + delta); break;
        case CyberParameter::CYBERNATION_LEVEL:
            setCybernationLevel(cybernationLevel + delta); break;
        case CyberParameter::HUMAN_RELATION:
            setHumanRelation(humanRelation + delta); break;
        case CyberParameter::ENVIRONMENT:
            setEnvironment(environment + delta); break;
        case CyberParameter::TECHNOLOGY:
            setTechnology(technology + delta); break;
    }
}


const int &Params::getParamAmount(CyberParameter param)
{
    switch (param) {
        case CyberParameter::COHESION:
            return this->cohesion;
        case CyberParameter::CYBERNATION_LEVEL:
            return this->cybernationLevel;
        case CyberParameter::HUMAN_RELATION:
            return this->humanRelation;
        case CyberParameter::ENVIRONMENT:
            return this->environment;
        case CyberParameter::TECHNOLOGY:
            return this->cohesion;
    }
    return -1;
}