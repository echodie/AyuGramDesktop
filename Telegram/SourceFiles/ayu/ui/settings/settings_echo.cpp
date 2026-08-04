#include "ayu/ui/settings/settings_echo.h"

#include "ayu/ui/settings/settings_main.h"
#include "settings/settings_builder.h"
#include "settings/settings_common.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"

#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"

namespace Settings {

using namespace Builder;

namespace {

const auto kMeta = BuildHelper({
	.id = Echo::Id(),
	.parentId = AyuMain::Id(),
	.title = u"echo"_q,
	.icon = &st::menuIconCustomize,
}, [](SectionBuilder &builder) {
	builder.addSkip();
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
