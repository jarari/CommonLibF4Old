#include "RE/NetImmerse/NiObjectNET.h"

#include "RE/NetImmerse/NiTimeController.h"

namespace RE
{
	NiObjectNET::NiObjectNET() { stl::emplace_vtable(this); }

	NiObjectNET::~NiObjectNET() {}
	NiExtraData* NiObjectNET::GetExtraData(BSFixedString n) const noexcept {
		if (extra) {
			extra->lock.lock();
			for (auto it = extra->begin(); it != extra->end(); ++it) {
				if ((*it)->name == n) {
					return *it;
				}
			}
			extra->lock.unlock();
		}

		return nullptr;
	}
	// NOLINT(modernize-use-equals-default)
}
