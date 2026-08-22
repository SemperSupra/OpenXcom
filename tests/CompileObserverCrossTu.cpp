/*
 * Cross-translation-unit probe for the header-only compile observer slot.
 */

#include "../src/Engine/CompileObserver.h"

bool compileObserverVisibleFromOtherTranslationUnit() noexcept
{
	return OpenXcom::getCompileObserver() != nullptr;
}
