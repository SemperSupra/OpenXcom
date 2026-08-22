#pragma once
/*
 * Copyright 2010-2026 OpenXcom Developers.
 *
 * This file is part of OpenXcom.
 *
 * OpenXcom is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OpenXcom is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenXcom.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <cstdint>
#include <exception>
#include <string_view>

namespace OpenXcom
{

/**
 * Schema version for structured compile/load observation events.
 *
 * This version describes the observer contract, not OXCE ruleset syntax.
 */
static constexpr std::uint32_t COMPILE_OBSERVER_SCHEMA_VERSION = 1;

/**
 * Stable, high-level event classes emitted by the compile/load observer.
 *
 * Event producers remain the existing OXCE loader/linker/resource paths;
 * this enum does not define or duplicate their semantics.
 */
enum class CompileEventKind : std::uint8_t
{
	PhaseBegin,
	PhaseEnd,
	RuleOperation,
	LinkResult,
	ResourceResolution,
	FilesystemWriteIntent,
	Snapshot
};

/**
 * Structured view of one compile/load observation.
 *
 * Text members are non-owning and are only guaranteed to remain valid for
 * the duration of CompileObserver::onCompileEvent(). A sink that retains an
 * event must copy the text it needs.
 *
 * Empty fields mean "not applicable/not supplied" for that event kind.
 */
struct CompileEvent
{
	std::uint32_t schemaVersion = COMPILE_OBSERVER_SCHEMA_VERSION;
	CompileEventKind kind = CompileEventKind::PhaseBegin;
	std::string_view phase;
	std::string_view category;
	std::string_view operation;
	std::string_view identity;
	std::string_view source;
	std::string_view outcome;
};

/**
 * Passive observer for authoritative OXCE compile/load behavior.
 *
 * The observer is optional. With no observer installed, emitCompileEvent()
 * is a no-op. Observer failures are contained so instrumentation cannot alter
 * OXCE loader pass/fail behavior; callers may query compileObserverFailed()
 * and fail evidence generation separately.
 */
class CompileObserver
{
public:
	virtual ~CompileObserver() = default;
	virtual void onCompileEvent(const CompileEvent &event) = 0;
};

namespace CompileObserverDetail
{
inline CompileObserver *&observerSlot() noexcept
{
	static CompileObserver *observer = nullptr;
	return observer;
}

inline bool &failureSlot() noexcept
{
	static bool failed = false;
	return failed;
}
}

/**
 * Installs an observer owned by the caller.
 * @param observer Observer to install, or nullptr to disable observation.
 * @return Previously installed observer.
 */
inline CompileObserver *setCompileObserver(CompileObserver *observer) noexcept
{
	CompileObserver *previous = CompileObserverDetail::observerSlot();
	CompileObserverDetail::observerSlot() = observer;
	CompileObserverDetail::failureSlot() = false;
	return previous;
}

/**
 * Returns the currently installed observer, or nullptr when disabled.
 */
inline CompileObserver *getCompileObserver() noexcept
{
	return CompileObserverDetail::observerSlot();
}

/**
 * Reports whether the installed observer threw while handling an event.
 *
 * The exception is deliberately contained to preserve authoritative engine
 * behavior. Evidence-producing modes can treat this flag as a hard failure.
 */
inline bool compileObserverFailed() noexcept
{
	return CompileObserverDetail::failureSlot();
}

/**
 * Sends one event to the installed observer without changing engine behavior.
 */
inline void emitCompileEvent(const CompileEvent &event) noexcept
{
	CompileObserver *observer = getCompileObserver();
	if (!observer)
	{
		return;
	}

	try
	{
		observer->onCompileEvent(event);
	}
	catch (...)
	{
		CompileObserverDetail::failureSlot() = true;
	}
}

/**
 * No-throw scope helper for observing an existing engine phase.
 *
 * Construction emits PhaseBegin. Destruction emits PhaseEnd with outcome
 * "success" during normal scope exit or "exception" during stack unwinding.
 * It does not catch, translate, or suppress engine exceptions.
 *
 * The phase string is non-owning and must outlive this scope. Intended call
 * sites use string literals so observation introduces no allocation or other
 * failure path into authoritative engine behavior.
 */
class CompilePhaseScope
{
public:
	explicit CompilePhaseScope(std::string_view phase) noexcept :
		_phase(phase), _uncaughtOnEntry(std::uncaught_exceptions())
	{
		CompileEvent event;
		event.kind = CompileEventKind::PhaseBegin;
		event.phase = _phase;
		emitCompileEvent(event);
	}

	~CompilePhaseScope() noexcept
	{
		CompileEvent event;
		event.kind = CompileEventKind::PhaseEnd;
		event.phase = _phase;
		event.outcome = std::uncaught_exceptions() > _uncaughtOnEntry ? "exception" : "success";
		emitCompileEvent(event);
	}

	CompilePhaseScope(const CompilePhaseScope &) = delete;
	CompilePhaseScope &operator=(const CompilePhaseScope &) = delete;

private:
	std::string_view _phase;
	int _uncaughtOnEntry;
};

}
