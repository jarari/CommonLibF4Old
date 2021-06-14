#include <Windows.h>
#include <unordered_map>
char tempbuf[8192] = { 0 };
char* _MESSAGE(const char* fmt, ...) {
	va_list args;

	va_start(args, fmt);
	vsprintf_s(tempbuf, sizeof(tempbuf), fmt, args);
	va_end(args);

	return tempbuf;
}

void Dump(void* mem, unsigned int size) {
	char* p = static_cast<char*>(mem);
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
			logger::warn(_MESSAGE("%s", stream.str().c_str()));
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

using std::unordered_map;
//static BGSKeyword* shieldKeyword;
static BGSMaterialType* shieldMaterial;
static unordered_map<TESObjectREFR*, bhkNPCollisionObject*> shieldCollisionObjects;

//POSTPONED:Is it possible to check if the actor/collision object still exists?

/*struct EquipManagerEvent {
};

class EquipEventSink : public BSTEventSink<EquipManagerEvent> {
	virtual BSEventNotifyControl ProcessEvent(const EquipManagerEvent& a_event, BSTEventSource<EquipManagerEvent>* a_source) override {
		Dump((void*)&a_event, 512);
		//TODO:If the actor equips an item, look for the shield keyword
		//TODO:Search for a NiNode named ShieldFramework and store the shield's collision object in the map.
		//TODO:If the actor unequips an item and it's a shield, then remove the collision object from the map.

		return BSEventNotifyControl::kContinue;
	}
	EquipEventSink() {};
	virtual ~EquipEventSink() {};
	F4_HEAP_REDEFINE_NEW(EquipEventSink);
};*/

//Rough dumps
struct _TESHitEvent {
	NiPoint3 hitPos; //0x0000 
	uint32_t unk_0C; //0x000C
	NiPoint3 normal; //0x0010
	uint32_t unk_1C; //0x001C
	NiPoint3 normal2; //0x0020 melee hits return 0,0,0
	uint32_t unk_2C; //0x002C
	bhkNPCollisionObject* colObj; //0x0030 
	char pad_0x0038[0x18]; //0x0038
	BGSAttackData* attackData; //0x0050
	TESObjectWEAP* weapon; //0x0058 
	TESObjectWEAP::InstanceData* weaponInstance; //0x0060 
	char pad_0x0068[0x18]; //0x0068
	TESForm* unkitem; //0x0080 
	char pad_0x0088[0x8]; //0x0088
	float N000005CF; //0x0090 
	float N00000831; //0x0094 
	float N000005D0; //0x0098 
	float N00000834; //0x009C 
	char pad_0x00A0[0x10]; //0x00A0
	float N000005E7; //0x00B0 
	float N0000082E; //0x00B4 
	char pad_0x00B8[0x8]; //0x00B8
	float N000005E9; //0x00C0 
	char pad_0x00C4[0x5C]; //0x00C4
	TESObjectREFR* victim; //0x0120 
	char pad_0x0128[0x38]; //0x0128
	TESObjectREFR* attacker; //0x0160 
};

struct __TESHitEvent {
	char pad_0x0000[0x1C]; //0x0000
	uint32_t flag; //0x001C ?????
	char pad_0x0020[0xC]; //0x0020
	uint32_t flag2; //0x002C ?????
	char pad_0x0030[0xA8]; //0x0030
	TESObjectREFR* attacker; //0x00D8 
	TESObjectREFR* victim; //0x00E0 
};

using std::vector;
ptrdiff_t ProcessProjectileFX_PatchOffset = 0x451;
REL::Relocation<uintptr_t> ProcessProjectileFX{ REL::ID(926879), ProcessProjectileFX_PatchOffset };
unordered_map<Actor*, BGSMaterialType*> materialTypeMap;

//I found this address using PlayerCharacter's BSTEventSink<TESHitEvent>
class HitEventSource : public BSTEventSource<_TESHitEvent> {
public:
	[[nodiscard]] static HitEventSource* GetSingleton() {
		REL::Relocation<HitEventSource*> singleton{ REL::ID(989868) };
		return singleton.get();
	}
};

//TESHitEvent gets fired twice on projectile hits, once on melee hits.
//The first event is _TESHitEvent and the second event is __TESHitEvent.
// 
//Since I couldn't figure out (or feel the need to reverse) why this kind of behavior happens and why they are so different,
//I just put bunch of ifs to distinguish them.
class HitEventSink : public BSTEventSink<_TESHitEvent> {
public:
	virtual BSEventNotifyControl ProcessEvent(const _TESHitEvent& a_event, BSTEventSource<_TESHitEvent>* a_source) override {
		if ((a_event.unk_1C == 0 || a_event.unk_2C == 0) && a_event.weapon && a_event.weaponInstance) {
			logger::warn("Hit event fired"sv);
			if (a_event.victim && a_event.victim->formType == ENUM_FORM_ID::kACHR) {
				Actor* a = (Actor*)a_event.victim;
				//Detect melee hits and swap material for a while.
				//I don't think it works though, since the sound effect plays before the hit event happens.
				if (a_event.attackData && a_event.colObj) {
					NiAVObject* parent = a_event.colObj->sceneObject;
					if (parent && strcmp(parent->name.c_str(), "ShieldFramework") == 0) {
						materialTypeMap.insert(std::pair<Actor*, BGSMaterialType*>(a, a->race->bloodImpactMaterial));
						a->race->bloodImpactMaterial = shieldMaterial;
					}
				}
				else {
					auto lookup = materialTypeMap.find(a);
					if (lookup != materialTypeMap.end()) {
						a->race->bloodImpactMaterial = lookup->second;
					}
				}
			}
		}
		return BSEventNotifyControl::kContinue;
	}
	HitEventSink() {};
	virtual ~HitEventSink() {};
	F4_HEAP_REDEFINE_NEW(HitEventSink);
};

//This will intercept ProcessImpacts() function in Projectile's vtable and check if the collidee is holding a shield.
class ProjectileHooks : public Projectile {
public:
	typedef bool (ProjectileHooks:: * FnProcessImpacts)();

	bool CheckShield() {
		for (auto it = this->impacts.begin(); it != this->impacts.end(); ++it) {
			if (it->colObj.get()) {
				logger::warn(_MESSAGE("colObj %llx", it->colObj.get()));
			}
			if (it->processed)
				continue;
			if (it->collidee.get() && it->collidee.get()->GetFormType() == ENUM_FORM_ID::kACHR) {
				//Do we even need to check the keyword??? I don't think so
				/*Actor* a = (Actor*)it->collidee.get().get();
				if (!a || !a->currentProcess || !a->currentProcess->middleHigh)
					continue;
				BSTArray<EquippedItem> equipped = a->currentProcess->middleHigh->equippedItems;
				BGSKeywordForm* keywordForm = nullptr;
				for (auto eqit = equipped.begin(); eqit != equipped.end(); ++eqit) {
					if (eqit->equipIndex.index == 0 && eqit->item.instanceData.get()) {
						keywordForm = ((TESObjectWEAP::InstanceData*)eqit->item.instanceData.get())->keywords;
						if (!keywordForm)
							keywordForm = (TESObjectWEAP*)eqit->item.object;
						break;
					}
				}

				if (!keywordForm)
					continue;

				BGSKeyword** keywords = keywordForm->keywords;
				bool found = false;
				uint32_t i = 0;
				while (!found && i < keywordForm->numKeywords) {
					if (keywords[i] == shieldKeyword) {
						found = true;
						logger::warn("Keyword found"sv);
					}
					++i;
				}*/

				//if (found) {
				if (it->colObj.get()) {
					NiAVObject* parent = it->colObj.get()->sceneObject;
					if (parent && strcmp(parent->name.c_str(), "ShieldFramework") == 0) {
						it->materialType = shieldMaterial;
						this->damage = 0.0f;
					}
				}
				//}
			}
		}
		FnProcessImpacts fn = fnHash.at(*(uint64_t*)this);
		return fn ? (this->*fn)() : false;
	}

	static void HookProcessImpacts(uint64_t addr, uint64_t offset) {
		FnProcessImpacts fn = SafeWrite64Function(addr + offset, &ProjectileHooks::CheckShield);
		fnHash.insert(std::pair<uint64_t, FnProcessImpacts>(addr, fn));
	}
protected:
	static unordered_map<uint64_t, FnProcessImpacts> fnHash;
};
unordered_map<uint64_t, ProjectileHooks::FnProcessImpacts> ProjectileHooks::fnHash;

void InitializeFramework() {
	uint64_t addr;
	uint64_t offset = 0x680;
	const auto& [map, lock] = TESForm::GetAllForms();
	BSAutoReadLock l{ lock };
	if (map) {
		for (auto k = map->begin(); k != map->end(); ++k) {
			/*if (k->second->formType == ENUM_FORM_ID::kKYWD) {
				if (strcmp(((BGSKeyword*)(k->second))->GetFormEditorID(), "ShieldFramework") == 0) {
					shieldKeyword = (BGSKeyword*)k->second;
					logger::warn(_MESSAGE("Shield Keyword %llx", k->second));
				}
			}*/
			if (k->second->formType == ENUM_FORM_ID::kMATT) {
				if (strcmp(((BGSMaterialType*)(k->second))->materialName.c_str(), "ActorArmored") == 0) {
					shieldMaterial = (BGSMaterialType*)k->second;
					logger::warn(_MESSAGE("Shield Material %llx", k->second));
				}
			}
		}
	}
	for (int i = 0; i < 1; ++i) {
		addr = Projectile::VTABLE[i].address();
		logger::warn(_MESSAGE("Patching Projectile %llx", addr));
		ProjectileHooks::HookProcessImpacts(addr, offset);

		addr = MissileProjectile::VTABLE[i].address();
		logger::warn(_MESSAGE("Patching MissileProjectile %llx", addr));
		ProjectileHooks::HookProcessImpacts(addr, offset);

		addr = ArrowProjectile::VTABLE[i].address();
		logger::warn(_MESSAGE("Patching ArrowProjectile %llx", addr));
		ProjectileHooks::HookProcessImpacts(addr, offset);

		addr = GrenadeProjectile::VTABLE[i].address();
		logger::warn(_MESSAGE("Patching GrenadeProjectile %llx", addr));
		ProjectileHooks::HookProcessImpacts(addr, offset);

		addr = BeamProjectile::VTABLE[i].address();
		logger::warn(_MESSAGE("Patching BeamProjectile %llx", addr));
		ProjectileHooks::HookProcessImpacts(addr, offset);

		addr = FlameProjectile::VTABLE[i].address();
		logger::warn(_MESSAGE("Patching FlameProjectile %llx", addr));
		ProjectileHooks::HookProcessImpacts(addr, offset);

		addr = ConeProjectile::VTABLE[i].address();
		logger::warn(_MESSAGE("Patching ConeProjectile %llx", addr));
		ProjectileHooks::HookProcessImpacts(addr, offset);

		addr = BarrierProjectile::VTABLE[i].address();
		logger::warn(_MESSAGE("Patching BarrierProjectile %llx", addr));
		ProjectileHooks::HookProcessImpacts(addr, offset);
	}

	//Fallout4.exe+0FD55F0 is the function that plays the impact effect based on various impact data.
	//0x451 from this function is the part where it checks the form type of collidee and play the impact effect based on the race data.
	//This will modify that behavior, forcing the game to always use the projectile impact data's materialType for the impact effect.
	logger::warn(_MESSAGE("Patching impact form type check %llx", ProcessProjectileFX.address()));
	REL::safe_write(ProcessProjectileFX.address(), (uint8_t)0xEB);
}

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Query(const F4SE::QueryInterface* a_f4se, F4SE::PluginInfo* a_info)
{
#ifndef NDEBUG
	auto sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
#else
	auto path = logger::log_directory();
	if (!path) {
		return false;
	}

	*path /= "ShieldFramework.log"sv;
	auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
#endif

	auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));

#ifndef NDEBUG
	log->set_level(spdlog::level::trace);
#else
	log->set_level(spdlog::level::info);
	log->flush_on(spdlog::level::warn);
#endif

	spdlog::set_default_logger(std::move(log));
	spdlog::set_pattern("%g(%#): [%^%l%$] %v"s);

	a_info->infoVersion = F4SE::PluginInfo::kVersion;
	a_info->name = "ShieldFramework";
	a_info->version = 1;

	if (a_f4se->IsEditor()) {
		logger::critical("loaded in editor"sv);
		return false;
	}

	const auto ver = a_f4se->RuntimeVersion();
	if (ver < F4SE::RUNTIME_1_10_162) {
		logger::critical("unsupported runtime v{}"sv, ver.string());
		return false;
	}

	return true;
}

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Load(const F4SE::LoadInterface* a_f4se)
{
	F4SE::Init(a_f4se);

	const F4SE::MessagingInterface* message = F4SE::GetMessagingInterface();
	message->RegisterListener([](F4SE::MessagingInterface::Message* msg) -> void {
		if (msg->type == F4SE::MessagingInterface::kGameDataReady) {
			InitializeFramework();
		}
		else if (msg->type == F4SE::MessagingInterface::kGameLoaded) {
			HitEventSource* src = HitEventSource::GetSingleton();
			logger::warn(_MESSAGE("HitEventSource %llx", src));
			HitEventSink* sink = new HitEventSink();
			src->RegisterSink(sink);
		}
	});
	return true;
}
