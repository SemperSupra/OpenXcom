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
#include <cstdlib>
#include <exception>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
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
inline const char *eventKindName(CompileEventKind kind) noexcept
{
	switch (kind)
	{
	case CompileEventKind::PhaseBegin: return "phase-begin";
	case CompileEventKind::PhaseEnd: return "phase-end";
	case CompileEventKind::RuleOperation: return "rule-operation";
	case CompileEventKind::LinkResult: return "link-result";
	case CompileEventKind::ResourceResolution: return "resource-resolution";
	case CompileEventKind::FilesystemWriteIntent: return "filesystem-write-intent";
	case CompileEventKind::Snapshot: return "snapshot";
	}
	return "unknown";
}

class EnvironmentJsonlObserver final : public CompileObserver
{
public:
	explicit EnvironmentJsonlObserver(const std::string &path) :
		_out(path, std::ios::out | std::ios::trunc)
	{
		if (!_out)
		{
			throw std::runtime_error("could not open compile observer trace: " + path);
		}
	}

	void onCompileEvent(const CompileEvent &event) override
	{
		_out << "{\"schema\":" << event.schemaVersion << ",\"kind\":";
		writeJsonString(eventKindName(event.kind));
		_out << ",\"phase\":";
		writeJsonString(event.phase);
		_out << ",\"category\":";
		writeJsonString(event.category);
		_out << ",\"operation\":";
		writeJsonString(event.operation);
		_out << ",\"identity\":";
		writeJsonString(event.identity);
		_out << ",\"source\":";
		writeJsonString(event.source);
		_out << ",\"outcome\":";
		writeJsonString(event.outcome);
		_out << "}\n";
		_out.flush();
		if (!_out)
		{
			throw std::runtime_error("failed writing compile observer trace");
		}
	}

private:
	void writeJsonString(std::string_view value)
	{
		_out.put('"');
		for (const char raw : value)
		{
			const unsigned char ch = static_cast<unsigned char>(raw);
			switch (ch)
			{
			case '"': _out << "\\\""; break;
			case '\\': _out << "\\\\"; break;
			case '\b': _out << "\\b"; break;
			case '\f': _out << "\\f"; break;
			case '\n': _out << "\\n"; break;
			case '\r': _out << "\\r"; break;
			case '\t': _out << "\\t"; break;
			default:
				if (ch < 0x20)
				{
					static const char hex[] = "0123456789abcdef";
					_out << "\\u00" << hex[(ch >> 4) & 0x0f] << hex[ch & 0x0f];
				}
				else
				{
					_out.put(static_cast<char>(ch));
				}
			}
		}
		_out.put('"');
	}

	std::ofstream _out;
};

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

inline bool &environmentCheckedSlot() noexcept
{
	static bool checked = false;
	return checked;
}

inline std::unique_ptr<EnvironmentJsonlObserver> &environmentObserverSlot() noexcept
{
	static std::unique_ptr<EnvironmentJsonlObserver> observer;
	return observer;
}

inline void tryInstallEnvironmentObserver() noexcept
{
	if (environmentCheckedSlot() || observerSlot())
	{
		return;
	}
	environmentCheckedSlot() = true;

	const char *path = std::getenv("OXCE_COMPILE_TRACE");
	if (!path || !*path)
	{
		return;
	}

	try
	{
		environmentObserverSlot() = std::make_unique<EnvironmentJsonlObserver>(path);
		observerSlot() = environmentObserverSlot().get();
	}
	catch (...)
	{
		failureSlot() = true;
	}
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
	CompileObserverDetail::environmentCheckedSlot() = true;
	CompileObserverDetail::observerSlot() = observer;
	CompileObserverDetail::failureSlot() = false;
	return previous;
}

/**
 * Returns the currently installed observer, or nullptr when disabled.
 *
 * When no observer was installed explicitly, the first call checks the
 * OXCE_COMPILE_TRACE environment variable once. If it names a file, a minimal
 * JSON Lines sink is installed for the lifetime of the process.
 */
inline CompileObserver *getCompileObserver() noexcept
{
	CompileObserverDetail::tryInstallEnvironmentObserver();
	return CompileObserverDetail::observerSlot();
}

/**
 * Reports whether the installed observer threw while handling an event or the
 * environment-requested trace sink could not be created.
 *
 * The failure is deliberately contained to preserve authoritative engine
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
