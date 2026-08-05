#include "ayu/features/auto_download.h"

#include "core/application.h"
#include "core/core_settings.h"
#include "data/data_session.h"
#include "main/main_account.h"
#include "main/main_domain.h"
#include "main/main_session.h"
#include "main/main_session_settings.h"

#include <rpl/event_stream.h>

namespace Ayu::AutoDownload {
namespace {

rpl::event_stream<> ChangesStream;

} // namespace

std::shared_ptr<Data::AutoDownload::Full> Load(
		const Data::AutoDownload::Full &fallback) {
	auto &global = Core::App().settings();
	auto result = std::make_shared<Data::AutoDownload::Full>(fallback);
	const auto saved = global.readPref<QByteArray>(kSettingsKey);
	if (!result->setFromSerialized(saved)) {
		global.writePref<QByteArray>(kSettingsKey, result->serialize());
	}
	return result;
}

void Save(not_null<Main::Session*> from, Effects effects) {
	auto &app = Core::App();
	auto &global = app.settings();
	const auto saved = from->settings().autoDownload().serialize();
	if (global.readPref<QByteArray>(kSettingsKey) == saved) {
		return;
	}
	global.writePref<QByteArray>(kSettingsKey, saved);
	for (const auto &[index, account] : app.domain().accounts()) {
		const auto session = account->maybeSession();
		if (!session) {
			continue;
		}
		session->saveSettingsDelayed();
		if (effects.photos) {
			session->data().photoLoadSettingsChanged();
		}
		if (effects.documents) {
			session->data().documentLoadSettingsChanged();
		}
		if (effects.animations) {
			session->data().checkPlayingAnimations();
		}
	}
	ChangesStream.fire({});
}

rpl::producer<> Changes() {
	return ChangesStream.events();
}

} // namespace Ayu::AutoDownload
