#include "ayu/ui/settings/settings_echo.h"

#include "lang_auto.h"
#include "ayu/ayu_settings.h"
#include "ayu/ui/boxes/mention_filter_box.h"
#include "ayu/ui/settings/ayu_builder.h"
#include "ayu/ui/settings/settings_ayu_utils.h"
#include "ayu/ui/settings/settings_main.h"
#include "base/unique_qptr.h"
#include "boxes/auto_download_box.h"
#include "core/application.h"
#include "data/data_auto_download.h"
#include "data/data_session.h"
#include "main/main_account.h"
#include "main/main_domain.h"
#include "main/main_session.h"
#include "main/main_session_settings.h"
#include "settings/settings_builder.h"
#include "settings/settings_common.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/popup_menu.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_controller.h"
#include "window/window_session_controller.h"

#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"

namespace Settings {

using namespace Builder;
using namespace AyuBuilder;

namespace {

struct FiltersState {
	base::unique_qptr<Ui::PopupMenu> menu;
	Fn<void()> rebuild;
};

struct SaveToggle {
	QString id;
	BoolGetter getter;
	BoolSetter setter;
};

void AddSeparator(SectionBuilder &builder) {
	builder.addSkip();
	builder.addDivider();
	builder.addSkip();
}

void UpdateFilters(int index, Fn<void(std::vector<MentionFilter>&, int)> apply) {
	auto &settings = AyuSettings::getInstance();
	auto list = settings.mentionFilters();
	if (index >= int(list.size())) {
		return;
	}
	apply(list, index);
	settings.setMentionFilters(std::move(list));
}

void ShowFilterMenu(
		not_null<Ui::VerticalLayout*> list,
		not_null<Window::SessionController*> controller,
		not_null<FiltersState*> state,
		int index,
		bool enabled) {
	state->menu = base::make_unique_q<Ui::PopupMenu>(
		list,
		st::popupMenuWithIcons);
	state->menu->addAction(u"Edit"_q, [=] {
		controller->show(Box(EditMentionFilterBox, index, [=] {
			state->rebuild();
		}));
	}, &st::menuIconEdit);
	state->menu->addAction(
		enabled ? u"Disable"_q : u"Enable"_q,
		[=] {
			UpdateFilters(index, [=](auto &filters, int i) {
				filters[i].enabled = !enabled;
			});
			state->rebuild();
		},
		enabled ? &st::menuIconMute : &st::menuIconUnmute);
	state->menu->addSeparator();
	state->menu->addAction(u"Delete"_q, [=] {
		UpdateFilters(index, [](auto &filters, int i) {
			filters.erase(begin(filters) + i);
		});
		state->rebuild();
	}, &st::menuIconDelete);
	state->menu->popup(QCursor::pos());
}

void FillFilters(
		not_null<Ui::VerticalLayout*> list,
		not_null<Window::SessionController*> controller,
		not_null<FiltersState*> state) {
	list->clear();
	const auto &filters = AyuSettings::getInstance().mentionFilters();
	for (auto index = 0, count = int(filters.size()); index != count; ++index) {
		const auto &filter = filters[index];
		auto label = filter.pattern;
		if (!filter.description.isEmpty()) {
			label += u" — "_q + filter.description;
		}
		if (!filter.enabled) {
			label = u"[off] "_q + label;
		}
		const auto row = list->add(
			object_ptr<Ui::SettingsButton>(
				list,
				rpl::single(label),
				st::settingsButtonNoIcon));
		const auto enabled = filter.enabled;
		row->setClickedCallback([=] {
			ShowFilterMenu(list, controller, state, index, enabled);
		});
	}
	list->resizeToWidth(list->width());
}

void BuildNotifications(SectionBuilder &builder, AyuSectionBuilder &ayu) {
	builder.addSubsectionTitle(rpl::single(u"notifications"_q));

	ayu.addSettingToggle({
		.id = u"echo/disableGroupMentionNotifications"_q,
		.title = rpl::single(u"mute group mentions"_q),
		.getter = &AyuSettings::disableGroupMentionNotifications,
		.setter = &AyuSettings::setDisableGroupMentionNotifications,
		.icon = { &st::menuIconMute },
	});

	builder.addDivider();
	builder.addSkip();

	builder.add([](const WidgetContext &ctx) -> SectionBuilder::WidgetToAdd {
		auto wrap = object_ptr<Ui::VerticalLayout>(ctx.container);
		const auto outer = wrap.data();
		const auto controller = ctx.controller;
		const auto list = outer->add(object_ptr<Ui::VerticalLayout>(outer));
		const auto state = outer->lifetime().make_state<FiltersState>();

		state->rebuild = [=] {
			FillFilters(list, controller, state);
		};
		state->rebuild();

		const auto add = AddButtonWithIcon(
			outer,
			rpl::single(u"add mention filter"_q),
			st::settingsButton,
			{ &st::menuIconAdd });
		add->setClickedCallback([=] {
			controller->show(Box(AddMentionFilterBox, [=] {
				state->rebuild();
			}));
		});

		return { .widget = std::move(wrap), .align = style::al_top };
	});

	builder.addSkip();
}

void BuildUnreadCounter(SectionBuilder &builder, AyuSectionBuilder &ayu) {
	builder.addSubsectionTitle(rpl::single(u"unread counter"_q));

	ayu.addSettingToggle({
		.id = u"echo/excludeMentionsFromUnreadCount"_q,
		.title = rpl::single(u"exclude mentions from badge"_q),
		.getter = &AyuSettings::excludeMentionsFromUnreadCount,
		.setter = &AyuSettings::setExcludeMentionsFromUnreadCount,
	});
}

void SyncAutoDownloadToAllSessions(not_null<::Main::Session*> from) {
	const auto saved = from->settings().autoDownload().serialize();
	for (const auto &[index, account] : Core::App().domain().accounts()) {
		const auto session = account->maybeSession();
		if (!session || session == from) {
			continue;
		}
		session->settings().autoDownload().setFromSerialized(saved);
		session->saveSettingsDelayed();
		session->data().photoLoadSettingsChanged();
		session->data().documentLoadSettingsChanged();
	}
}

void AddAutoDownloadButton(
		SectionBuilder &builder,
		QString id,
		rpl::producer<QString> title,
		Data::AutoDownload::Source source,
		IconDescriptor &&icon) {
	builder.addButton({
		.id = std::move(id),
		.title = std::move(title),
		.icon = std::move(icon),
		.onClick = [=] {
			const auto window = Core::App().activeWindow();
			const auto controller = window
				? window->sessionController()
				: nullptr;
			if (!controller) {
				return;
			}
			const auto session = &controller->session();
			auto box = Box<AutoDownloadBox>(session, source);
			const auto raw = box.data();
			window->show(std::move(box));
			QObject::connect(raw, &QObject::destroyed, [=] {
				SyncAutoDownloadToAllSessions(session);
			});
		},
	});
}

void BuildAutoDownload(SectionBuilder &builder) {
	builder.addSubsectionTitle(tr::lng_media_auto_settings());

	AddAutoDownloadButton(
		builder,
		u"echo/autoDownloadPrivate"_q,
		tr::lng_media_auto_in_private(),
		Data::AutoDownload::Source::User,
		{ &st::menuIconProfile });
	AddAutoDownloadButton(
		builder,
		u"echo/autoDownloadGroups"_q,
		tr::lng_media_auto_in_groups(),
		Data::AutoDownload::Source::Group,
		{ &st::menuIconGroups });
	AddAutoDownloadButton(
		builder,
		u"echo/autoDownloadChannels"_q,
		tr::lng_media_auto_in_channels(),
		Data::AutoDownload::Source::Channel,
		{ &st::menuIconChannel });
}

void BuildAccountList(SectionBuilder &builder, AyuSectionBuilder &ayu) {
	builder.addSubsectionTitle(rpl::single(u"account list"_q));

	ayu.addSettingToggle({
		.id = u"echo/showPhoneInAccountList"_q,
		.title = rpl::single(u"show phone number"_q),
		.getter = &AyuSettings::showPhoneInAccountList,
		.setter = &AyuSettings::setShowPhoneInAccountList,
	});
}

void BuildSaveSection(
		SectionBuilder &builder,
		AyuSectionBuilder &ayu,
		const QString &title,
		rpl::producer<bool> shown,
		SaveToggle inPrivate,
		SaveToggle inGroups) {
	builder.addSubsectionTitle(rpl::single(title));

	ayu.addSettingToggle({
		.id = std::move(inPrivate.id),
		.title = rpl::single(u"in private chats"_q),
		.getter = inPrivate.getter,
		.setter = inPrivate.setter,
		.shown = rpl::duplicate(shown),
	});
	ayu.addSettingToggle({
		.id = std::move(inGroups.id),
		.title = rpl::single(u"in groups"_q),
		.getter = inGroups.getter,
		.setter = inGroups.setter,
		.shown = std::move(shown),
	});
}

const auto kMeta = BuildHelper({
	.id = Echo::Id(),
	.parentId = AyuMain::Id(),
	.title = u"echo"_q,
	.icon = &st::menuIconCustomize,
}, [](SectionBuilder &builder) {
	auto ayu = AyuSectionBuilder(builder);
	auto *settings = &AyuSettings::getInstance();

	builder.addSkip();
	BuildNotifications(builder, ayu);
	AddSeparator(builder);
	BuildUnreadCounter(builder, ayu);
	AddSeparator(builder);
	BuildAutoDownload(builder);
	AddSeparator(builder);
	BuildAccountList(builder, ayu);
	AddSeparator(builder);
	BuildSaveSection(
		builder,
		ayu,
		u"save deleted messages"_q,
		settings->saveDeletedMessagesValue(),
		{
			u"echo/saveDeletedPrivate"_q,
			&AyuSettings::saveDeletedPrivate,
			&AyuSettings::setSaveDeletedPrivate,
		},
		{
			u"echo/saveDeletedGroups"_q,
			&AyuSettings::saveDeletedGroups,
			&AyuSettings::setSaveDeletedGroups,
		});
	AddSeparator(builder);
	BuildSaveSection(
		builder,
		ayu,
		u"save edit history"_q,
		settings->saveMessagesHistoryValue(),
		{
			u"echo/saveEditedPrivate"_q,
			&AyuSettings::saveEditedPrivate,
			&AyuSettings::setSaveEditedPrivate,
		},
		{
			u"echo/saveEditedGroups"_q,
			&AyuSettings::saveEditedGroups,
			&AyuSettings::setSaveEditedGroups,
		});
	builder.addSkip();
});

} // namespace

rpl::producer<QString> Echo::title() {
	return rpl::single(u"echo"_q);
}

Echo::Echo(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
: Section(parent, controller) {
	setupContent();
}

void Echo::setupContent() {
	const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);
	build(content, kMeta.build);
	Ui::ResizeFitChild(this, content);
}

Type EchoId() {
	return Echo::Id();
}

} // namespace Settings
