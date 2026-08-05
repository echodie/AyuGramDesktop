#pragma once

#include <rpl/producer.h>

#include <memory>

namespace Data::AutoDownload {
class Full;
} // namespace Data::AutoDownload

namespace Main {
class Session;
} // namespace Main

namespace Ayu::AutoDownload {

inline constexpr auto kSettingsKey = "ayu-auto-download"_cs;

struct Effects {
	bool photos = false;
	bool documents = false;
	bool animations = false;
};

[[nodiscard]] std::shared_ptr<Data::AutoDownload::Full> Load(
	const Data::AutoDownload::Full &fallback);
void Save(not_null<Main::Session*> from, Effects effects);
[[nodiscard]] rpl::producer<> Changes();

} // namespace Ayu::AutoDownload
