#include "ayu/ui/boxes/mention_filter_box.h"

#include "ayu/ayu_settings.h"
#include "lang/lang_keys.h"
#include "ui/layers/generic_box.h"
#include "ui/toast/toast.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/checkbox.h"
#include "ui/wrap/vertical_layout.h"

#include "styles/style_layers.h"
#include "styles/style_settings.h"

#include <QtCore/QRegularExpression>

namespace {

void ShowFilterBox(
		not_null<Ui::GenericBox*> box,
		int index,
		Fn<void()> onDone) {
	const auto &filters = AyuSettings::getInstance().mentionFilters();
	const auto isEdit = (index >= 0) && (index < int(filters.size()));
	const auto initial = isEdit ? filters[index] : MentionFilter();

	box->setTitle(rpl::single(isEdit
		? u"edit filter"_q
		: u"add filter"_q));
	box->setWidth(st::boxWideWidth);

	const auto container = box->verticalLayout();
	const auto addRow = [&](auto widget, int topSkip) {
		return container->add(
			std::move(widget),
			QMargins(
				st::boxPadding.left(),
				topSkip,
				st::boxPadding.right(),
				0));
	};

	const auto pattern = addRow(
		object_ptr<Ui::InputField>(
			container,
			st::defaultInputField,
			rpl::single(u"regex pattern"_q),
			initial.pattern),
		0);
	const auto description = addRow(
		object_ptr<Ui::InputField>(
			container,
			st::defaultInputField,
			rpl::single(u"description (optional)"_q),
			initial.description),
		st::boxPadding.top());
	const auto caseSensitive = addRow(
		object_ptr<Ui::Checkbox>(
			container,
			u"Case Sensitive"_q,
			initial.caseSensitive,
			st::defaultBoxCheckbox),
		st::boxPadding.top() * 2);
	const auto reversed = addRow(
		object_ptr<Ui::Checkbox>(
			container,
			u"Reversed"_q,
			initial.reversed,
			st::defaultBoxCheckbox),
		st::boxPadding.top());

	box->setFocusCallback([=] { pattern->setFocusFast(); });

	box->addButton(rpl::single(isEdit ? u"save"_q : u"add"_q), [=] {
		const auto text = pattern->getLastText().trimmed();
		if (text.isEmpty()) {
			pattern->showError();
			return;
		}
		const auto expression = QRegularExpression(text);
		if (!expression.isValid()) {
			Ui::Toast::Show(
				box,
				u"invalid regex: "_q + expression.errorString());
			pattern->showError();
			return;
		}
		auto &settings = AyuSettings::getInstance();
		auto list = settings.mentionFilters();
		auto filter = MentionFilter{
			.pattern = text,
			.description = description->getLastText().trimmed(),
			.enabled = initial.enabled,
			.caseSensitive = caseSensitive->checked(),
			.reversed = reversed->checked(),
		};
		if (isEdit && index < int(list.size())) {
			list[index] = std::move(filter);
		} else {
			list.push_back(std::move(filter));
		}
		settings.setMentionFilters(std::move(list));
		if (onDone) {
			onDone();
		}
		box->closeBox();
	});
	box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
}

} // namespace

void AddMentionFilterBox(
		not_null<Ui::GenericBox*> box,
		Fn<void()> onDone) {
	ShowFilterBox(box, -1, std::move(onDone));
}

void EditMentionFilterBox(
		not_null<Ui::GenericBox*> box,
		int index,
		Fn<void()> onDone) {
	ShowFilterBox(box, index, std::move(onDone));
}
