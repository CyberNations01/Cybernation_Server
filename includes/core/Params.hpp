#ifndef PARAMS_HPP
#define PARAMS_HPP

enum class CyberParameter {
    COHESION,
    CYBERNATION_LEVEL,
    HUMAN_RELATION,
    ENVIRONMENT,
    TECHNOLOGY
};

class Params {
private:
    int cohesion         = 10;
    int cybernationLevel = 2;
    int humanRelation    = 7;
    int environment      = 7;
    int technology       = 7;

public:
    Params() = default;
    Params(int cyberLevel, int humanRel, int env, int tech, int coh);
    ~Params() = default;

    // Getters
    int getCohesion()          const { return cohesion; }
    int getCybernationLevel()  const { return cybernationLevel; }
    int getHumanRelation()     const { return humanRelation; }
    int getEnvironment()       const { return environment; }
    int getTechnology()        const { return technology; }

    // Setters (with validation — cohesion caps the others)
    void setCohesion(int val);
    void setCybernationLevel(int val);
    void setHumanRelation(int val);
    void setEnvironment(int val);
    void setTechnology(int val);

    // Convenience: adjust by delta instead of absolute set
    void adjustParam(CyberParameter param, int delta);
};

#endif
