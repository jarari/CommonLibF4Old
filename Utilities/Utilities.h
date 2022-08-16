#pragma once
#include "RE/Bethesda/TESDataHandler.h"
#include <Windows.h>
using namespace RE;
#pragma region Utilities

char tempbuf[8192] = { 0 };
char* _MESSAGE(const char* fmt, ...) {
	va_list args;

	va_start(args, fmt);
	vsnprintf(tempbuf, sizeof(tempbuf), fmt, args);
	va_end(args);
	spdlog::log(spdlog::level::warn, tempbuf);

	return tempbuf;
}

void Dump(const void* mem, unsigned int size) {
	const char* p = static_cast<const char*>(mem);
	unsigned char* up = (unsigned char*)p;
	std::stringstream stream;
	int row = 0;
	for (unsigned int i = 0; i < size; i++) {
		stream << std::setfill('0') << std::setw(2) << std::hex << (int)up[i] << " ";
		if (i % 8 == 7) {
			stream << "\t0x"
				<< std::setw(2) << std::hex << (int)up[i]
				<< std::setw(2) << (int)up[i - 1]
				<< std::setw(2) << (int)up[i - 2]
				<< std::setw(2) << (int)up[i - 3]
				<< std::setw(2) << (int)up[i - 4]
				<< std::setw(2) << (int)up[i - 5]
				<< std::setw(2) << (int)up[i - 6]
				<< std::setw(2) << (int)up[i - 7] << std::setfill('0');
			stream << "\t0x" << std::setw(2) << std::hex << row * 8 << std::setfill('0');
			_MESSAGE("%s", stream.str().c_str());
			stream.str(std::string());
			row++;
		}
	}
}

template<class Ty>
Ty SafeWrite64Function(uintptr_t addr, Ty data) {
	DWORD oldProtect;
	void* _d[2];
	memcpy(_d, &data, sizeof(data));
	size_t len = sizeof(_d[0]);

	VirtualProtect((void*)addr, len, PAGE_EXECUTE_READWRITE, &oldProtect);
	Ty olddata;
	memset(&olddata, 0, sizeof(Ty));
	memcpy(&olddata, (void*)addr, len);
	memcpy((void*)addr, &_d[0], len);
	VirtualProtect((void*)addr, len, oldProtect, &oldProtect);
	return olddata;
}

ActorValueInfo* GetAVIFByEditorID(std::string editorID) {
	TESDataHandler* dh = TESDataHandler::GetSingleton();
	BSTArray<ActorValueInfo*> avifs = dh->GetFormArray<ActorValueInfo>();
	for (auto it = avifs.begin(); it != avifs.end(); ++it) {
		if (strcmp((*it)->formEditorID.c_str(), editorID.c_str()) == 0) {
			return (*it);
		}
	}
	return nullptr;
}

BGSExplosion* GetExplosionByFullName(std::string explosionname) {
	TESDataHandler* dh = TESDataHandler::GetSingleton();
	BSTArray<BGSExplosion*> explosions = dh->GetFormArray<BGSExplosion>();
	for (auto it = explosions.begin(); it != explosions.end(); ++it) {
		if (strcmp((*it)->GetFullName(), explosionname.c_str()) == 0) {
			return (*it);
		}
	}
	return nullptr;
}

SpellItem* GetSpellByFullName(std::string spellname) {
	TESDataHandler* dh = TESDataHandler::GetSingleton();
	BSTArray<SpellItem*> spells = dh->GetFormArray<SpellItem>();
	for (auto it = spells.begin(); it != spells.end(); ++it) {
		if (strcmp((*it)->GetFullName(), spellname.c_str()) == 0) {
			return (*it);
		}
	}
	return nullptr;
}

TESForm* GetFormFromMod(std::string modname, uint32_t formid) {
	if (!modname.length() || !formid)
		return nullptr;
	TESDataHandler* dh = TESDataHandler::GetSingleton();
	TESFile* modFile = nullptr;
	for (auto it = dh->files.begin(); it != dh->files.end(); ++it) {
		TESFile* f = *it;
		if (strcmp(f->filename, modname.c_str()) == 0) {
			modFile = f;
			break;
		}
	}
	if (!modFile)
		return nullptr;
	uint8_t modIndex = modFile->compileIndex;
	uint32_t id = formid;
	if (modIndex < 0xFE) {
		id |= ((uint32_t)modIndex) << 24;
	}
	else {
		uint16_t lightModIndex = modFile->smallFileCompileIndex;
		if (lightModIndex != 0xFFFF) {
			id |= 0xFE000000 | (uint32_t(lightModIndex) << 12);
		}
	}
	return TESForm::GetFormByID(id);
}

bool CheckPA(Actor* a) {
	if (!a->extraList) {
		return false;
	}
	return a->extraList->HasType(EXTRA_DATA_TYPE::kPowerArmor);;
}

float GetActorScale(Actor* a) {
	typedef float (*_GetPlayerScale)(Actor*);
	REL::Relocation<_GetPlayerScale> func{ REL::ID(911188) };
	return func(a);
}

void ReloadWeaponGraph(BSExtraData* animGraphPreload, Actor& a_actor) {
	typedef void (*_ReloadWeaponGraph)(BSExtraData*, Actor&);
	REL::Relocation<_ReloadWeaponGraph> func{ REL::ID(393711) };
	return func(animGraphPreload, a_actor);
}

namespace RE {
	class BSAnimationGraphManager {
	public:
		bool Activate() {
			using func_t = decltype(&RE::BSAnimationGraphManager::Activate);
			REL::Relocation<func_t> func{ REL::ID(950096) };
			return func(this);
		}
		bool Deactivate() {
			using func_t = decltype(&RE::BSAnimationGraphManager::Deactivate);
			REL::Relocation<func_t> func{ REL::ID(591084) };
			return func(this);
		}
	};

	struct ActorEquipManagerEvent::Event {
		uint32_t unk00;				//00
		uint8_t pad04[0x7 - 0x4];	//04
		bool isUnequip;				//07
		void* unk08;				//08	
		Actor* a;					//10	equip target
	};

	struct TESObjectLoadedEvent {
		uint32_t formId;			//00
		uint8_t loaded;				//08
	};

	struct TESEquipEvent {
		Actor* a;					//00
		uint32_t formId;			//0C
		uint32_t unk08;				//08
		uint64_t flag;				//10 0x00000000ff000000 for unequip
	};

	class HitEventSource : public BSTEventSource<TESHitEvent> {
	public:
		[[nodiscard]] static HitEventSource* GetSingleton() {
			REL::Relocation<HitEventSource*> singleton{ REL::ID(989868) };
			return singleton.get();
		}
	};

	class ObjectLoadedEventSource : public BSTEventSource<TESObjectLoadedEvent> {
	public:
		[[nodiscard]] static ObjectLoadedEventSource* GetSingleton() {
			REL::Relocation<ObjectLoadedEventSource*> singleton{ REL::ID(416662) };
			return singleton.get();
		}
	};

	class EquipEventSource : public BSTEventSource<TESEquipEvent> {
	public:
		[[nodiscard]] static EquipEventSource* GetSingleton() {
			REL::Relocation<EquipEventSource*> singleton{ REL::ID(485633) };
			return singleton.get();
		}
	};

	class MGEFApplyEventSource : public BSTEventSource<TESMagicEffectApplyEvent> {
	public:
		[[nodiscard]] static MGEFApplyEventSource* GetSingleton() {
			REL::Relocation<MGEFApplyEventSource*> singleton{ REL::ID(1481228) };
			return singleton.get();
		}
	};
}

namespace F4 {
	struct Unk {
		uint32_t unk00 = 0xFFFFFFFF;
		uint32_t unk04 = 0x0;
		uint32_t unk08 = 1;
	};

	class TaskQueueInterface {
	public:
		void __fastcall QueueRebuildBendableSpline(TESObjectREFR* ref, bool rebuildCollision, NiAVObject* target) {
			using func_t = decltype(&F4::TaskQueueInterface::QueueRebuildBendableSpline);
			REL::Relocation<func_t> func{ REL::ID(198419) };
			return func(this, ref, rebuildCollision, target);
		}
		void QueueShow1stPerson(bool shouldShow) {
			using func_t = decltype(&F4::TaskQueueInterface::QueueShow1stPerson);
			REL::Relocation<func_t> func{ REL::ID(994377) };
			return func(this, shouldShow);
		}
	};

	REL::Relocation<TaskQueueInterface**> ptr_TaskQueueInterface{ REL::ID(7491) };

	REL::Relocation<DWORD*> ptr_hkMemoryRouterTlsIndex{ REL::ID(878080) };

	bool PlaySound(BGSSoundDescriptorForm* sndr, NiPoint3 pos, NiAVObject* node) {
		typedef bool* func_t(Unk, BGSSoundDescriptorForm*, NiPoint3, NiAVObject*);
		REL::Relocation<func_t> func{ REL::ID(376497) };
		Unk u;
		return func(u, sndr, pos, node);
	}

	void ShakeCamera(float mul, NiPoint3 origin, float duration, float strength) {
		using func_t = decltype(&F4::ShakeCamera);
		REL::Relocation<func_t> func{ REL::ID(758209) };
		return func(mul, origin, duration, strength);
	}

	void ApplyImageSpaceModifier(TESImageSpaceModifier* imod, float strength, NiAVObject* target) {
		using func_t = decltype(&F4::ApplyImageSpaceModifier);
		REL::Relocation<func_t> func{ REL::ID(179769) };
		return func(imod, strength, target);
	}

	TESObjectREFR* PlaceAtMe_Native(BSScript::IVirtualMachine* vm, uint32_t stackId, TESObjectREFR** target, TESForm* form, int32_t count, bool bForcePersist, bool bInitiallyDisabled, bool bDeleteWhenAble) {
		using func_t = decltype(&F4::PlaceAtMe_Native);
		REL::Relocation<func_t> func{ REL::ID(984532) };
		return func(vm, stackId, target, form, count, bForcePersist, bInitiallyDisabled, bDeleteWhenAble);
	}

	void MoveRefrToPosition(TESObjectREFR* source, uint32_t* pTargetHandle, TESObjectCELL* parentCell, TESWorldSpace* worldSpace, NiPoint3* position, NiPoint3* rotation) {
		using func_t = decltype(&F4::MoveRefrToPosition);
		REL::Relocation<func_t> func{ REL::ID(1332434) };
		return func(source, pTargetHandle, parentCell, worldSpace, position, rotation);
	}
}

#pragma endregion
