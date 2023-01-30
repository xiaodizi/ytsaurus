#pragma once

#include <yt/yt/core/misc/public.h>

namespace NYT::NCoreDump {

////////////////////////////////////////////////////////////////////////////////

DECLARE_REFCOUNTED_STRUCT(ICoreDumper)

DECLARE_REFCOUNTED_CLASS(TCoreDumperConfig)

////////////////////////////////////////////////////////////////////////////////

} // namespace NYT::NCoreDump