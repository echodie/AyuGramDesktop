#pragma once

namespace Main {
class Session;
} // namespace Main

namespace Dialogs {
class Entry;
} // namespace Dialogs

namespace Ayu::LocalPins {

void Setup(not_null<Main::Session*> session);
[[nodiscard]] bool Toggle(not_null<Dialogs::Entry*> entry, FilterId filterId);

} // namespace Ayu::LocalPins
