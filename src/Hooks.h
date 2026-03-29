#pragma once

namespace sosr::hooks {
void Install();
[[nodiscard]] bool IsWindowShutdownObserved();
}
