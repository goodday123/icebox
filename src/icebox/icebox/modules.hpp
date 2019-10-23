#pragma once

#include "enums.hpp"
#include "types.hpp"

#include <functional>

namespace core { struct Core; }

namespace modules
{
    using bpid_t      = uint64_t;
    using on_mod_fn   = std::function<walk_e(mod_t)>;
    using on_event_fn = std::function<void(proc_t, mod_t)>;

    bool                list            (core::Core&, proc_t proc, on_mod_fn on_mod);
    opt<std::string>    name            (core::Core&, proc_t proc, mod_t mod);
    opt<span_t>         span            (core::Core&, proc_t proc, mod_t mod);
    opt<mod_t>          find            (core::Core&, proc_t proc, uint64_t addr);
    opt<bpid_t>         listen_create   (core::Core&, const on_event_fn& on_load);
} // namespace modules
