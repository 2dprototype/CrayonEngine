#pragma once

// The engine's side of the plugin ABI: builds the CrayonHostAPI table handed to plugins.
#include "crayon_plugin.h"

namespace crayon {

// Returns a pointer to a static, fully populated CrayonHostAPI (valid for the whole process).
const CrayonHostAPI* plugin_host_api();

} // namespace crayon
