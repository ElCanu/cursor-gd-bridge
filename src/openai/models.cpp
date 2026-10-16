#include "openai/models.hpp"

#include "json.hpp"

namespace bridge::openai {

std::string ModelsListJson() {
    const nlohmann::json doc = {
        {"object", "list"},
        {"data", nlohmann::json::array()},
    };
    return doc.dump();
}

}  // namespace bridge::openai
