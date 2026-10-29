// SPDX-License-Identifier: GPL-3.0-only
// ObjectId has no out-of-line members; this translation unit exists so that the
// scene library always contributes at least one object file on every generator.
#include "finaleffect/scene/ObjectId.hpp"

namespace fe::scene {

static_assert(sizeof(ObjectId) == sizeof(std::uint64_t), "ObjectId must stay a single 64-bit value");

}  // namespace fe::scene
