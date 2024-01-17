#ifndef JSONHELPERS_H
#define JSONHELPERS_H

#include <glm/glm.hpp>
#include <json/json.h>

namespace JsonHelpers
{
    static inline glm::vec3 GetJsonVec3f(Json::Value vectorValue)
    {
        return glm::vec3(vectorValue[0].asFloat(), vectorValue[1].asFloat(), vectorValue[2].asFloat());
    }
    static inline glm::ivec2 GetJsonVec2i(Json::Value vectorValue)
    {
        return glm::ivec2(vectorValue[0].asInt(), vectorValue[1].asInt());
    }

    static inline glm::uvec3 GetJsonVec3u(Json::Value vectorValue)
    {
        return glm::uvec3(vectorValue[0].asUInt(), vectorValue[1].asUInt(), vectorValue[2].asUInt());
    }

}



#endif