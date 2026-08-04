#include "ayu/ui/settings/settings_echo.h"

#include "ayu/ayu_settings.h"
#include "ayu/ui/settings/ayu_builder.h"
#include "ayu/ui/settings/settings_ayu_utils.h"
#include "ayu/ui/settings/settings_main.h"
#include "settings/settings_builder.h"
#include "settings/settings_common.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"

#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"

namespace Settings {

using namespace Builder;
using namespace AyuBuilder;

namespace {

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
