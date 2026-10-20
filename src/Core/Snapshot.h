#pragma once

#include <cstddef>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <utility>

// A table measured from the load order that other threads read while it is
// measured again, at every save load, new game and coc from the main menu.
// The new table is built aside and replaces the old one whole, so a reader
// gets one or the other and never half of each. A reader's copy keeps its
// table alive. The lock is held only to move pointers and never while the
// game is called, so a reader may take it under any game lock, an
// inventory's included.
template <class T>
class Snapshot
{
public:
	// The table last published, or nothing before the first. Taken once a
	// call, so every read in the call goes by the same table.
	[[nodiscard]] std::shared_ptr<const T> Get() const
	{
		const std::shared_lock l{ _lock };
		return _table;
	}

	// Makes a_table the one readers get. The table it replaces is kept until
	// the next call and freed here then, after the lock. A reader keeps a
	// table for 1 call, far less than the time between 2 measures, so a
	// loader thread never frees one under an inventory lock. Publishing
	// nothing, as Unload does, lets go of both.
	void Publish(std::shared_ptr<const T> a_table)
	{
		std::shared_ptr<const T> gone[2];
		{
			const std::unique_lock l{ _lock };
			gone[0] = std::move(_retired);
			_retired = std::exchange(_table, std::move(a_table));
			if (!_table) {
				gone[1] = std::move(_retired);
			}
		}
	}

private:
	mutable std::shared_mutex _lock;
	std::shared_ptr<const T>  _table;
	// The table _table replaced, see Publish.
	std::shared_ptr<const T> _retired;
};

// How many keys 2 readings of a map disagree on: in 1 and not the other, or
// with another value. For the line NEC.log writes when a measure moved.
template <class Map>
[[nodiscard]] std::size_t ChangedKeys(const Map& a_was, const Map& a_now)
{
	std::size_t changed = 0;
	for (const auto& [key, value] : a_now) {
		const auto was = a_was.find(key);
		changed += was == a_was.end() || was->second != value ? 1 : 0;
	}
	for (const auto& [key, value] : a_was) {
		changed += a_now.contains(key) ? 0 : 1;
	}
	return changed;
}
