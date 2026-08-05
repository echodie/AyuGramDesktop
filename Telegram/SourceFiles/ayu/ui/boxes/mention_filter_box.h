#pragma once

#include "base/basic_types.h"

namespace Ui {
class GenericBox;
} // namespace Ui

void AddMentionFilterBox(
	not_null<Ui::GenericBox*> box,
	Fn<void()> onDone);

void EditMentionFilterBox(
	not_null<Ui::GenericBox*> box,
	int index,
	Fn<void()> onDone);
