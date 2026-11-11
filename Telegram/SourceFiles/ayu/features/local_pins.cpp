#include "ayu/features/local_pins.h"

#include "data/data_channel.h"
#include "data/data_chat_filters.h"
#include "data/data_folder.h"
#include "data/data_histories.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "history/history.h"
#include "main/main_session.h"
#include "storage/storage_account.h"

#include <QtCore/QDataStream>

namespace Ayu::LocalPins {
namespace {

constexpr auto kPrefKey = "ayu-local-pins"_cs;
constexpr auto kRequestLimit = 100;

struct Saved {
	FilterId filterId = 0;
	FolderId folderId = 0;
	not_null<History*> history;
};

[[nodiscard]] uint64 AccessHash(not_null<PeerData*> peer) {
	if (const auto user = peer->asUser()) {
		return user->accessHash();
	} else if (const auto channel = peer->asChannel()) {
		return channel->accessHash();
	}
	return 0;
}

void RestoreAccessHash(not_null<PeerData*> peer, uint64 hash) {
	if (const auto user = peer->asUser(); user && !user->accessHash()) {
		user->setAccessHash(hash);
	} else if (const auto channel = peer->asChannel()
		; channel && !channel->accessHash()) {
		channel->setAccessHash(hash);
	}
}

[[nodiscard]] Dialogs::PinnedList *ListFor(const Saved &saved) {
	auto &data = saved.history->owner();
	if (saved.filterId) {
		auto &filters = data.chatsFilters();
		const auto &list = filters.list();
		return ranges::contains(list, saved.filterId, &Data::ChatFilter::id)
			? filters.chatsList(saved.filterId)->pinned().get()
			: nullptr;
	}
	const auto folder = saved.history->folder();
	return ((folder ? folder->id() : 0) == saved.folderId)
		? data.chatsListFor(saved.history)->pinned().get()
		: nullptr;
}

void Save(not_null<Main::Session*> session) {
	auto result = QByteArray();
	QDataStream stream(&result, QIODevice::WriteOnly);
	stream.setVersion(QDataStream::Qt_5_1);
	const auto write = [&](
			FilterId filterId,
			FolderId folderId,
			not_null<const Dialogs::PinnedList*> list) {
		const auto local = list->order()
			| ranges::views::drop(list->serverCount());
		for (const auto &key : local) {
			const auto peer = key.history()->peer;
			stream
				<< qint32(filterId)
				<< qint32(folderId)
				<< SerializePeerId(peer->id)
				<< quint64(AccessHash(peer));
		}
	};
	auto &data = session->data();
	write(0, 0, data.chatsList()->pinned());
	if (const auto archive = data.folderLoaded(Data::Folder::kId)) {
		write(0, archive->id(), archive->chatsList()->pinned());
	}
	for (const auto &filter : data.chatsFilters().list()) {
		if (const auto id = filter.id()) {
			write(id, 0, data.chatsFilters().chatsList(id)->pinned());
		}
	}
	session->local().writePref<QByteArray>(kPrefKey, result);
}

void RequestDialogs(
		not_null<Main::Session*> session,
		std::vector<not_null<History*>> histories,
		Fn<void()> done) {
	if (histories.empty()) {
		done();
		return;
	}
	const auto count = std::min(int(histories.size()), kRequestLimit);
	const auto rest = std::vector(histories.begin() + count, histories.end());
	const auto pending = std::make_shared<int>(count);
	for (const auto history : histories | ranges::views::take(count)) {
		session->data().histories().requestDialogEntry(history, [=] {
			if (!--*pending) {
				RequestDialogs(session, rest, done);
			}
		});
	}
}

void Restore(not_null<Main::Session*> session, Fn<void()> done) {
	auto &data = session->data();
	const auto saved = std::make_shared<std::vector<Saved>>();
	const auto serialized = session->local().readPref<QByteArray>(kPrefKey);
	QDataStream stream(serialized);
	stream.setVersion(QDataStream::Qt_5_1);
	while (!stream.atEnd()) {
		auto filterId = qint32();
		auto folderId = qint32();
		auto peerId = quint64();
		auto hash = quint64();
		stream >> filterId >> folderId >> peerId >> hash;
		const auto id = DeserializePeerId(peerId);
		if (stream.status() != QDataStream::Ok || !id) {
			break;
		}
		const auto peer = data.peer(id);
		RestoreAccessHash(peer, hash);
		saved->push_back({ filterId, folderId, data.history(peer) });
	}
	auto unknown = std::vector<not_null<History*>>();
	for (const auto &entry : *saved) {
		const auto channel = entry.history->peer->asChannel();
		if (!entry.history->folderKnown()
			&& !(channel && channel->isCommunity())) {
			unknown.push_back(entry.history);
		}
	}
	RequestDialogs(session, std::move(unknown), [=] {
		for (const auto &entry : *saved | ranges::views::reverse) {
			if (!entry.history->folderKnown()) {
				continue;
			} else if (const auto list = ListFor(entry)) {
				list->setPinnedLocally(entry.history);
			}
		}
		session->data().notifyPinnedDialogsOrderUpdated();
		done();
	});
}

} // namespace

void Setup(not_null<Main::Session*> session) {
	const auto data = &session->data();
	const auto active = std::make_shared<bool>(false);
	data->pinnedDialogsOrderUpdated(
	) | rpl::filter([=] {
		return *active;
	}) | rpl::on_next([=] {
		Save(session);
	}, session->lifetime());
	data->sessionDataAboutToBeCleared(
	) | rpl::on_next([=] {
		*active = false;
	}, session->lifetime());
	rpl::single(
		rpl::empty
	) | rpl::then(
		data->chatsFilters().changed()
	) | rpl::filter([=] {
		return data->chatsFilters().loaded();
	}) | rpl::take(1) | rpl::on_next([=] {
		Restore(session, [=] { *active = true; });
	}, session->lifetime());
}

bool Toggle(not_null<Dialogs::Entry*> entry, FilterId filterId) {
	const auto history = entry->asHistory();
	const auto channel = history ? history->peer->asChannel() : nullptr;
	if (!history
		|| !history->folderKnown()
		|| (channel && channel->isCommunity())) {
		return false;
	}
	auto &data = history->owner();
	const auto list = (filterId
		? data.chatsFilters().chatsList(filterId)
		: data.chatsListFor(history))->pinned();
	if (list->isLocal(history)) {
		data.setChatPinned(history, filterId, false);
		return true;
	} else if (history->isPinnedDialog(filterId)
		|| (filterId
			? data.pinnedCanPin(filterId, history)
			: data.pinnedCanPin(history))
		|| int(list->order().size()) >= list->limit()) {
		return false;
	}
	list->setPinnedLocally(history);
	data.notifyPinnedDialogsOrderUpdated();
	return true;
}

} // namespace Ayu::LocalPins
