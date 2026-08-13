#include "resources.hpp"

#include <glaze/json.hpp>

namespace game {

resources_t::resources_t(alex::core_t *core, std::filesystem::path manifest)
    : _core{core}, _manifest{manifest} {}

}
