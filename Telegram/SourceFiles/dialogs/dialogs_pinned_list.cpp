/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "dialogs/dialogs_pinned_list.h"

#include "data/data_saved_messages.h"
#include "dialogs/dialogs_key.h"
#include "dialogs/dialogs_entry.h"
#include "history/history.h"
#include "data/data_channel.h"
#include "data/data_session.h"
#include "data/data_forum.h"

namespace Dialogs {

PinnedList::PinnedList(FilterId filterId, int limit)
: _filterId(filterId)
, _limit(limit) {
	Expects(limit > 0);
}

void PinnedList::setLimit(int limit) {
	Expects(limit > 0);

	if (_limit == limit) {
		return;
	}
	_limit = limit;
	applyLimit(_limit);
}

void PinnedList::addPinned(Key key) {
	Expects(key.entry()->folderKnown());

	addPinnedGetPosition(key);
}

int PinnedList::addPinnedGetPosition(Key key) {
	const auto already = ranges::find(_data, key);
	if (already - begin(_data) < _server) {
		return already - begin(_data);
	} else if (already != end(_data)) {
		_data.erase(already);
	} else {
		applyLimit(_limit - 1);
	}
	insertAfterServer(key);
	return _server++;
}

void PinnedList::setPinnedLocally(Key key) {
	Expects(key.entry()->folderKnown() || _filterId != 0);

	if (!ranges::contains(_data, key) && int(_data.size()) < _limit) {
		insertAfterServer(key);
	}
}

bool PinnedList::isLocal(Key key) const {
	const auto i = ranges::find(_data, key);
	return (i != end(_data)) && (i - begin(_data) >= _server);
}

void PinnedList::insertAfterServer(Key key) {
	_data.insert(begin(_data) + _server, key);
	for (auto i = _server; i < int(size(_data)); ++i) {
		_data[i].entry()->cachePinnedIndex(_filterId, i + 1);
	}
}

void PinnedList::setPinned(Key key, bool pinned) {
	Expects(key.entry()->folderKnown() || _filterId != 0);

	if (pinned) {
		const int position = addPinnedGetPosition(key);
		if (position) {
			const auto begin = _data.begin();
			std::rotate(begin, begin + position, begin + position + 1);
			for (auto i = 0; i != position + 1; ++i) {
				_data[i].entry()->cachePinnedIndex(_filterId, i + 1);
			}
		}
	} else if (const auto it = ranges::find(_data, key); it != end(_data)) {
		const auto index = int(it - begin(_data));
		if (index < _server) {
			--_server;
		}
		_data.erase(it);
		key.entry()->cachePinnedIndex(_filterId, 0);
		// cachePinnedIndex() may re-enter and erase from _data (for example
		// while a filter is being removed), so re-check the size each
		// iteration instead of caching it to avoid an out-of-bounds access.
		for (auto i = index; i < int(size(_data)); ++i) {
			_data[i].entry()->cachePinnedIndex(_filterId, i + 1);
		}
	}
}

void PinnedList::applyLimit(int limit) {
	Expects(limit >= 0);

	while (_data.size() > limit) {
		setPinned(_data.back(), false);
	}
}

void PinnedList::clear() {
	applyLimit(0);
}

void PinnedList::clearServer() {
	while (_server) {
		setPinned(_data[_server - 1], false);
	}
}

void PinnedList::applyList(
		not_null<Data::Session*> owner,
		const QVector<MTPDialogPeer> &list) {
	Expects(this != owner->savedMessages().chatsList()->pinned());

	clearServer();
	for (const auto &peer : list) {
		peer.match([&](const MTPDdialogPeer &data) {
			if (const auto peerId = peerFromMTP(data.vpeer())) {
				addPinned(owner->history(peerId));
			}
		}, [&](const MTPDdialogPeerFolder &data) {
			addPinned(owner->folder(data.vfolder_id().v));
		}, [&](const MTPDdialogPeerCommunity &data) {
			const auto channelId = ChannelId(data.vcommunity_id().v);
			if (const auto channel = owner->channelLoaded(channelId)) {
				const auto history = owner->history(channel);
				if (!history->folderKnown()) {
					history->clearFolder();
				}
				addPinned(history);
			}
		});
	}
}

void PinnedList::applyList(
		not_null<Data::SavedMessages*> sublistsOwner,
		const QVector<MTPDialogPeer> &list) {
	Expects(this == sublistsOwner->chatsList()->pinned());

	clear();
	for (const auto &peer : list) {
		peer.match([&](const MTPDdialogPeer &data) {
			if (const auto peerId = peerFromMTP(data.vpeer())) {
				const auto peer = sublistsOwner->owner().peer(peerId);
				addPinned(sublistsOwner->sublist(peer));
			}
		}, [](const MTPDdialogPeerFolder &data) {
		}, [](const MTPDdialogPeerCommunity &data) {
		});
	}
}

void PinnedList::applyList(
		not_null<Data::Forum*> forum,
		const QVector<MTPint> &list) {
	Expects(this == forum->topicsList()->pinned());

	clear();
	for (const auto &topicId : list) {
		addPinned(forum->topicFor(topicId.v));
	}
}

void PinnedList::applyList(const std::vector<not_null<History*>> &list) {
	Expects(_filterId != 0);

	const auto old = base::take(_data);
	const auto oldServer = std::exchange(_server, int(list.size()));

	_data.assign(list.begin(), list.end());
	for (const auto &key : old | ranges::views::drop(oldServer)) {
		if (!ranges::contains(_data, key)) {
			_data.push_back(key);
		}
	}
	for (auto i = 0; i != int(_data.size()); ++i) {
		_data[i].entry()->cachePinnedIndex(_filterId, i + 1);
	}

	for (const auto &key : old) {
		if (!ranges::contains(_data, key)) {
			key.entry()->cachePinnedIndex(_filterId, 0);
		}
	}
}

void PinnedList::reorder(Key key1, Key key2) {
	const auto index1 = ranges::find(_data, key1) - begin(_data);
	const auto index2 = ranges::find(_data, key2) - begin(_data);
	Assert(index1 >= 0 && index1 < _data.size());
	Assert(index2 >= 0 && index2 < _data.size());
	Assert(index1 != index2);
	std::swap(_data[index1], _data[index2]);
	key1.entry()->cachePinnedIndex(_filterId, index2 + 1);
	key2.entry()->cachePinnedIndex(_filterId, index1 + 1);
}

} // namespace Dialogs
