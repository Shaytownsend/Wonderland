// ir_forge - the factory IR library definition (every shipped IR and its generator parameters).
#pragma once

#include "PostProcess.h"

#include <functional>
#include <string>

namespace irf
{
struct IrDef
{
    std::string id, name, category, description, generator;
    uint64_t seed = 1;
    std::function<Ir4 (uint64_t seed)> render;
    FinishParams finish;
};

/** All factory IRs, in library order. */
const std::vector<IrDef>& factoryLibrary();

/** Category display colour (with a small deterministic per-id variation). */
std::array<int, 3> colourFor (const std::string& category, const std::string& id);

} // namespace irf
