/*
 * Private development smoke test for the OXCE compile observer contract.
 * No game data or engine runtime is required.
 */

#include "../src/Engine/CompileObserver.h"

#include <cassert>
#include <stdexcept>
#include <string>

bool compileObserverVisibleFromOtherTranslationUnit() noexcept;

namespace
{

class CountingObserver final : public OpenXcom::CompileObserver
{
public:
	int count = 0;
	OpenXcom::CompileEventKind lastKind = OpenXcom::CompileEventKind::PhaseBegin;
	std::string lastPhase;
	std::string lastOutcome;

	void onCompileEvent(const OpenXcom::CompileEvent &event) override
	{
		++count;
		lastKind = event.kind;
		lastPhase.assign(event.phase.data(), event.phase.size());
		lastOutcome.assign(event.outcome.data(), event.outcome.size());
	}
};

class ThrowingObserver final : public OpenXcom::CompileObserver
{
public:
	void onCompileEvent(const OpenXcom::CompileEvent &) override
	{
		throw std::runtime_error("intentional observer failure");
	}
};

}

int main()
{
	using namespace OpenXcom;

	assert(COMPILE_OBSERVER_SCHEMA_VERSION == 1);
	assert(getCompileObserver() == nullptr);
	assert(!compileObserverVisibleFromOtherTranslationUnit());
	assert(!compileObserverFailed());

	CompileEvent event;
	event.kind = CompileEventKind::PhaseBegin;
	event.phase = "smoke";

	// Disabled observation is a no-op.
	emitCompileEvent(event);
	assert(!compileObserverFailed());

	CountingObserver counting;
	assert(setCompileObserver(&counting) == nullptr);
	assert(compileObserverVisibleFromOtherTranslationUnit());
	event.kind = CompileEventKind::RuleOperation;
	emitCompileEvent(event);
	assert(counting.count == 1);
	assert(counting.lastKind == CompileEventKind::RuleOperation);
	assert(!compileObserverFailed());

	// A phase scope emits begin/end without changing normal control flow.
	const int beforeSuccess = counting.count;
	{
		CompilePhaseScope scope("smoke.success");
	}
	assert(counting.count == beforeSuccess + 2);
	assert(counting.lastKind == CompileEventKind::PhaseEnd);
	assert(counting.lastPhase == "smoke.success");
	assert(counting.lastOutcome == "success");

	// Stack unwinding is observed, not caught or translated by the phase scope.
	const int beforeException = counting.count;
	bool sawExpectedException = false;
	try
	{
		CompilePhaseScope scope("smoke.exception");
		throw std::runtime_error("authoritative failure");
	}
	catch (const std::runtime_error &e)
	{
		sawExpectedException = std::string(e.what()) == "authoritative failure";
	}
	assert(sawExpectedException);
	assert(counting.count == beforeException + 2);
	assert(counting.lastKind == CompileEventKind::PhaseEnd);
	assert(counting.lastPhase == "smoke.exception");
	assert(counting.lastOutcome == "exception");
	assert(!compileObserverFailed());

	// A broken evidence sink must not escape into authoritative engine behavior.
	ThrowingObserver throwing;
	assert(setCompileObserver(&throwing) == &counting);
	assert(compileObserverVisibleFromOtherTranslationUnit());
	emitCompileEvent(event);
	assert(compileObserverFailed());

	// Replacing/disabling the observer starts a fresh evidence session.
	assert(setCompileObserver(nullptr) == &throwing);
	assert(getCompileObserver() == nullptr);
	assert(!compileObserverVisibleFromOtherTranslationUnit());
	assert(!compileObserverFailed());

	return 0;
}
