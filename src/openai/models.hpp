#pragma once

#include <string>

// Desktop stores model ids itself. This list is empty so the bridge does not invent one.

namespace bridge::openai {

[[nodiscard]] std::string ModelsListJson();

}  // namespace bridge::openai
