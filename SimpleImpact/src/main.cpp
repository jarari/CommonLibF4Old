#include <algorithm>
#include <chrono>
#include <fstream>
#include <random>
#include <SimpleIni.h>
#include "nlohmann/json.hpp"
using namespace RE;
using std::ifstream;
using std::unordered_map;
using std::vector;
using std::queue;
using std::default_random_engine;
using std::shuffle;

char tempbuf[8192] = { 0 };
char* _MESSAGE(const char* fmt, ...) {
	va_list args;

	va_start(args, fmt);
	vsnprintf(tempbuf, sizeof(tempbuf), fmt, args);
	va_end(args);
	spdlog::log(spdlog::level::warn, tempbuf);

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
			_MESSAGE("%s", stream.str().c_str());
			stream.str(std::string());
			row++;
		}
	}
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

namespace F4 {
	struct Unk {
		uint32_t unk00 = 0xFFFFFFFF;
		uint32_t unk04 = 0x0;
		uint32_t unk08 = 1;
	};

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
}

REL::Relocation<float*> ptr_engineTime{ REL::ID(599343) };
CSimpleIniA ini(true, true, false);
PlayerCharacter* p;
PlayerCamera* pcam;
vector<TESRace*> headshotExcludedList;
vector<BGSSoundDescriptorForm*> hs_body_soundList;
vector<BGSSoundDescriptorForm*> hs_head_soundList;
vector<BGSSoundDescriptorForm*> ks_body_soundList;
vector<BGSSoundDescriptorForm*> ks_head_soundList;
BGSSoundDescriptorForm* hitSound;
BGSSoundDescriptorForm* hitSoundHead;
BGSSoundDescriptorForm* killSound;
BGSSoundDescriptorForm* killSoundHead;
BGSListForm* killQuoteList;
vector<int> killQuoteQueue;
float lastHitSound;
float lastKillSound;
std::chrono::system_clock::duration lastKillQuote = std::chrono::system_clock::now().time_since_epoch();
Actor* lastVictimHit;
uint32_t lastBodypartHit;
float lastTimeHit;
TESImageSpaceModifier* imod;
unordered_map<TESForm*, float> customShakeDuration;
unordered_map<TESForm*, float> customShakeStrength;
unordered_map<TESForm*, float> customFXStrength;

bool enableHitSound = true;
int hs_body_soundType = 0;
uint8_t hs_body_freqShift = 0;
uint8_t hs_body_freqVariance = 0;
uint16_t hs_body_staticAttenuation = 700;
bool enableHitSoundHeadshot = true;
int hs_head_soundType = 0;
uint8_t hs_head_freqShift = 0;
uint8_t hs_head_freqVariance = 0;
uint16_t hs_head_staticAttenuation = 700;
bool playOnAliveOnly = true;
bool playOnGunOnly = true;

bool enableKillSound = true;
int ks_body_soundType = 0;
uint8_t ks_body_freqShift = 0;
uint8_t ks_body_freqVariance = 0;
uint16_t ks_body_staticAttenuation = 700;
bool enableKillSoundHeadshot = true;
int ks_head_soundType = 0;
uint8_t ks_head_freqShift = 0;
uint8_t ks_head_freqVariance = 0;
uint16_t ks_head_staticAttenuation = 700;

bool enableKillQuote = true;
uint16_t kq_staticAttenuation = 700;
float kq_minimumCooldown = 2.0f;

float shakeDuration = 0;
float shakeStrength = 1;
float fxStrength = 0;

bool enableShake = true;
float durationMultiplier = 1;
float strengthMultiplier = 1;
float PADurationMultiplier = 1;
float PAStrengthMultiplier = 1;
float thirdPersonDurationMultiplier = 1;
float thirdPersonStrengthMultiplier = 1;
float ADSDurationMultiplier = 1;
float ADSStrengthMultiplier = 1;
float SneakDurationMultiplier = 1;
float SneakStrengthMultiplier = 1;
float rofCoefficient = 1;
float damageCoefficient = 1;
float minimumDuration = 0.2f;
float maximumDuration = 0.5f;
float minimumStrength = 0.7f;
float maximumStrength = 1.0f;

bool enableFX = true;
float minimumFXStrength = 0.5f;
float maximumFXStrength = 0.8f;

bool aimRecoveryDisabled = false;
bool disableOnFreecam = true;

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
	float unk90; //0x0090 
	float unk94; //0x0094 
	float unk98; //0x0098 
	float unk9C; //0x009C 
	char pad_0x00A0[0x10]; //0x00A0
	float unkB0; //0x00B0 
	float unkB4; //0x00B4 
	char pad_0x00B8[0x8]; //0x00B8
	float unkC0; //0x00C0 
	char pad_0x00C4[0xC]; //0x00C4
	uint32_t bodypart; //0x00D0 
	char pad_0x00D4[0x4C]; //0x00D4
	TESObjectREFR* victim; //0x0120 
	char pad_0x0128[0x38]; //0x0128
	TESObjectREFR* attacker; //0x0160 
};

class HitEventSource : public BSTEventSource<_TESHitEvent> {
public:
	[[nodiscard]] static HitEventSource* GetSingleton() {
		REL::Relocation<HitEventSource*> singleton{ REL::ID(989868) };
		return singleton.get();
	}
};

class HitEventSink : public BSTEventSink<_TESHitEvent> {
public:
	virtual BSEventNotifyControl ProcessEvent(const _TESHitEvent& evn, BSTEventSource<_TESHitEvent>* src) override {
		if ((evn.attackData || evn.weapon) && evn.attacker == p && evn.victim && evn.victim->formType == ENUM_FORM_ID::kACHR) {
			if (enableHitSound) {
				if (!playOnGunOnly || (evn.weaponInstance && evn.weaponInstance->type == 9)) {
					if (*ptr_engineTime - lastHitSound > 0.001f) {
						if (!playOnAliveOnly || (playOnAliveOnly && !evn.victim->IsDead(true))) {
							BGSSoundDescriptorForm* sndr = hitSound;
							if (enableHitSoundHeadshot && evn.bodypart == 1) {
								bool raceHasHead = true;
								int i = 0;
								while (raceHasHead && i < headshotExcludedList.size()) {
									if (headshotExcludedList[i] == ((Actor*)evn.victim)->race) {
										raceHasHead = false;
									}
									++i;
								}

								if (raceHasHead) {
									sndr = hitSoundHead;
								}
							}
							F4::PlaySound(sndr, evn.attacker->data.location, pcam->cameraRoot.get());
							lastHitSound = *ptr_engineTime;
						}
					}
				}
			}
			if (enableKillSound || enableKillQuote) {
				lastVictimHit = (Actor*)evn.victim;
				lastBodypartHit = evn.bodypart;
				lastTimeHit = *ptr_engineTime;
			}
		}
		return BSEventNotifyControl::kContinue;
	}
	HitEventSink() {};
	virtual ~HitEventSink() {};
	F4_HEAP_REDEFINE_NEW(HitEventSink);
};

bool CheckPA(Actor* a) {
	if (!a->extraList) {
		return false;
	}
	return a->extraList->HasType(EXTRA_DATA_TYPE::kPowerArmor);;
}

bool IsThirdPerson() {
	return pcam->currentState == pcam->cameraStates[CameraState::k3rdPerson];
}

bool IsInADS(Actor* a) {
	return a->gunState == 0x8;
}

bool IsSneaking(Actor* a) {
	return a->stance == 0x1;
}

class AnimationGraphEventWatcher : public BSTEventSink<BSAnimationGraphEvent> {
public:
	typedef BSEventNotifyControl (AnimationGraphEventWatcher::* FnProcessEvent)(BSAnimationGraphEvent& evn, BSTEventSource<BSAnimationGraphEvent>* dispatcher);

	BSEventNotifyControl HookedProcessEvent(BSAnimationGraphEvent& evn, BSTEventSource<BSAnimationGraphEvent>* src) {
		if (evn.animEvent == std::string("weaponFire") && evn.argument != std::string("2") && (p->gunState == 0x8 || p->gunState == 0x7)) {
			if (p->currentProcess) {
				BSTArray<EquippedItem>& equipped = p->currentProcess->middleHigh->equippedItems;
				if (equipped.size() > 0 && equipped[0].item.instanceData &&
					((TESObjectWEAP::InstanceData*)equipped[0].item.instanceData.get())->type == 9 && (!disableOnFreecam || pcam->currentState != pcam->cameraStates[CameraState::kFree])) {
					if (enableShake) {
						float tempShakeDuration = shakeDuration;
						float tempShakeStrength = shakeStrength;
						if (CheckPA(p)) {
							tempShakeDuration *= PADurationMultiplier;
							tempShakeStrength *= PAStrengthMultiplier;
						}
						if (IsThirdPerson()) {
							tempShakeDuration *= thirdPersonDurationMultiplier;
							tempShakeStrength *= thirdPersonStrengthMultiplier;
						}
						if (IsInADS(p)) {
							tempShakeDuration *= ADSDurationMultiplier;
							tempShakeStrength *= ADSStrengthMultiplier;
						}
						if (IsSneaking(p)) {
							tempShakeDuration *= SneakDurationMultiplier;
							tempShakeStrength *= SneakStrengthMultiplier;
						}
						F4::ShakeCamera(1, p->data.location, tempShakeDuration, tempShakeStrength);
					}
					if (enableFX) {
						F4::ApplyImageSpaceModifier(imod, fxStrength, nullptr);
					}
				}
			}
		}
		FnProcessEvent fn = fnHash.at(*(uint64_t*)this);
		return fn ? (this->*fn)(evn, src) : BSEventNotifyControl::kContinue;
	}

	void HookSink() {
		uint64_t vtable = *(uint64_t*)this;
		auto it = fnHash.find(vtable);
		if (it == fnHash.end()) {
			FnProcessEvent fn = SafeWrite64Function(vtable + 0x8, &AnimationGraphEventWatcher::HookedProcessEvent);
			fnHash.insert(std::pair<uint64_t, FnProcessEvent>(vtable, fn));
		}
	}

	void UnHookSink() {
		uint64_t vtable = *(uint64_t*)this;
		auto it = fnHash.find(vtable);
		if (it == fnHash.end())
			return;
		SafeWrite64Function(vtable + 0x8, it->second);
		fnHash.erase(it);
	}

protected:
	static std::unordered_map<uint64_t, FnProcessEvent> fnHash;
};
std::unordered_map<uint64_t, AnimationGraphEventWatcher::FnProcessEvent> AnimationGraphEventWatcher::fnHash;

float Lerp(float a, float b, float t) {
	return a + (b - a) * min(max(t, 0), 1);
}

void CalculateScore(Actor* a) {
	if (a->currentProcess) {
		BSTArray<EquippedItem>& equipped = a->currentProcess->middleHigh->equippedItems;
		if (equipped.size() > 0 && equipped[0].item.instanceData &&
			((TESObjectWEAP::InstanceData*)equipped[0].item.instanceData.get())->type == 9) {
			TESObjectWEAP* wep = ((TESObjectWEAP*)equipped[0].item.object);
			TESObjectWEAP::InstanceData* instance = (TESObjectWEAP::InstanceData*)equipped[0].item.instanceData.get();
			uint32_t ammoID = 0;
			if (instance->ammo) {
				ammoID = instance->ammo->formID;
			}
			_MESSAGE("Name : %s Form ID : %llx Ammo : %llx", wep->GetFullName(), wep->formID, ammoID);
			float damage = instance->attackDamage;
			if (instance->ammo)
				damage += instance->ammo->data.damage;
			if (instance->damageTypes) {
				for (auto it = instance->damageTypes->begin(); it != instance->damageTypes->end(); ++it) {
					damage += it->second.i;
				}
			}
			bool isAutomatic = (instance->flags & 0x8000) == 0x8000;
			float rof = instance->attackDelaySec / instance->speed;
			if (isAutomatic)
				rof /= 2.0f;
			float score = log10f((damage + 10) / damageCoefficient) + pow(rof, rofCoefficient);

			float customDurMult = 1;
			float customStrMult = 1;
			float customFXMult = 1;
			auto durFind = customShakeDuration.find(wep);
			if (durFind != customShakeDuration.end()) {
				customDurMult *= durFind->second;
			}
			auto strFind = customShakeStrength.find(wep);
			if (strFind != customShakeStrength.end()) {
				customStrMult *= strFind->second;
			}
			auto fxFind = customFXStrength.find(wep);
			if (fxFind != customFXStrength.end()) {
				customFXMult *= fxFind->second;
			}

			durFind = customShakeDuration.find(instance->ammo);
			if (durFind != customShakeDuration.end()) {
				customDurMult *= durFind->second;
			}
			strFind = customShakeStrength.find(instance->ammo);
			if (strFind != customShakeStrength.end()) {
				customStrMult *= strFind->second;
			}
			fxFind = customFXStrength.find(instance->ammo);
			if (fxFind != customFXStrength.end()) {
				customFXMult *= fxFind->second;
			}

			shakeDuration = min(max(Lerp(minimumDuration, maximumDuration, score) * durationMultiplier * customDurMult, minimumDuration), maximumDuration);
			shakeStrength = min(max(Lerp(minimumStrength, maximumStrength, score) * strengthMultiplier * customStrMult, minimumStrength), maximumStrength);
			fxStrength = min(max(Lerp(minimumFXStrength, maximumFXStrength, score) * durationMultiplier * customFXMult, minimumFXStrength), maximumFXStrength);

			_MESSAGE("score %f shakeDuration %f shakeStrength %f fxStrength %f", score, shakeDuration, shakeStrength, fxStrength);
		}
	}
}

struct TESEquipEvent {
	Actor* a;					//00
	uint32_t formId;			//0C
	uint32_t unk08;				//08
	uint64_t flag;				//10 0x00000000ff000000 for unequip
};

class EquipWatcher : public BSTEventSink<TESEquipEvent> {
public:
	virtual BSEventNotifyControl ProcessEvent(const TESEquipEvent& evn, BSTEventSource<TESEquipEvent>* a_source) {
		if ((enableShake || enableFX) && evn.a == p) {
			TESForm* item = TESForm::GetFormByID(evn.formId);
			if (item && item->formType == ENUM_FORM_ID::kWEAP) {
				CalculateScore(evn.a);
			}
		}
		return BSEventNotifyControl::kContinue;
	}
	F4_HEAP_REDEFINE_NEW(EquipWatcher);
};

class EquipEventSource : public BSTEventSource<TESEquipEvent> {
public:
	[[nodiscard]] static EquipEventSource* GetSingleton() {
		REL::Relocation<EquipEventSource*> singleton{ REL::ID(485633) };
		return singleton.get();
	}
}; 

void ShuffleKillQuote() {
	unsigned seed = 0;
	shuffle(killQuoteQueue.begin(), killQuoteQueue.end(), default_random_engine(seed));
}

class MenuWatcher : public BSTEventSink<MenuOpenCloseEvent> {
	virtual BSEventNotifyControl ProcessEvent(const MenuOpenCloseEvent& evn, BSTEventSource<MenuOpenCloseEvent>* src) override {
		if (evn.opening && evn.menuName == BSFixedString("LoadingMenu")) {
			ShuffleKillQuote();
		}
		return BSEventNotifyControl::kContinue;
	}
public:
	F4_HEAP_REDEFINE_NEW(MenuWatcher);
};

struct TESDeathEvent {
	TESObjectREFR* victim;
	TESObjectREFR* attacker;
	bool processed;
};

class DeathWatcher : public BSTEventSink<TESDeathEvent> {
public:
	virtual BSEventNotifyControl ProcessEvent(const TESDeathEvent& evn, BSTEventSource<TESDeathEvent>* a_source) {
		if (!evn.processed && evn.attacker == p && evn.victim == lastVictimHit && *ptr_engineTime - lastTimeHit < 0.075f && evn.attacker->Get3D()) {
			if (enableKillSound) {
				if (*ptr_engineTime - lastKillSound > 0.001f) {
					BGSSoundDescriptorForm* sndr = killSound;
					if (enableKillSoundHeadshot && lastVictimHit == evn.victim && lastBodypartHit == 1) {
						bool raceHasHead = true;
						int i = 0;
						while (raceHasHead && i < headshotExcludedList.size()) {
							if (headshotExcludedList[i] == ((Actor*)evn.victim)->race) {
								raceHasHead = false;
							}
							++i;
						}

						if (raceHasHead) {
							sndr = killSoundHead;
						}
					}
					F4::PlaySound(sndr, evn.attacker->data.location, pcam->cameraRoot.get());
					lastKillSound = *ptr_engineTime;
				}
			}
			if (enableKillQuote) {
				if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch() - lastKillQuote).count() >= kq_minimumCooldown * 1000.0f) {
					int i = killQuoteQueue.front();
					BGSSoundDescriptorForm* sndr = (BGSSoundDescriptorForm*)killQuoteList->arrayOfForms[i];
					if (sndr && sndr->formType == ENUM_FORM_ID::kSNDR) {
						F4::PlaySound(sndr, evn.attacker->data.location, pcam->cameraRoot.get());
						lastKillQuote = std::chrono::system_clock::now().time_since_epoch();
					}
					killQuoteQueue.erase(killQuoteQueue.begin());
					killQuoteQueue.push_back(i);
				}
			}
		}
		return BSEventNotifyControl::kContinue;
	}
	F4_HEAP_REDEFINE_NEW(DeathWatcher);
};

class DeathEventSource : public BSTEventSource<TESDeathEvent> {
public:
	[[nodiscard]] static DeathEventSource* GetSingleton() {
		REL::Relocation<DeathEventSource*> singleton{ REL::ID(1058809) };
		return singleton.get();
	}
};

void InitializePlugin() {
	p = PlayerCharacter::GetSingleton();
	pcam = PlayerCamera::GetSingleton();
	hs_body_soundList.push_back((BGSSoundDescriptorForm*)GetFormFromMod(std::string("SimpleImpact.esp"), 0x800));
	hs_body_soundList.push_back((BGSSoundDescriptorForm*)GetFormFromMod(std::string("SimpleImpact.esp"), 0x801));
	hs_body_soundList.push_back((BGSSoundDescriptorForm*)GetFormFromMod(std::string("SimpleImpact.esp"), 0x802));
	hs_body_soundList.push_back((BGSSoundDescriptorForm*)GetFormFromMod(std::string("SimpleImpact.esp"), 0x803));
	hs_body_soundList.push_back((BGSSoundDescriptorForm*)GetFormFromMod(std::string("SimpleImpact.esp"), 0x804));
	hs_body_soundList.push_back((BGSSoundDescriptorForm*)GetFormFromMod(std::string("SimpleImpact.esp"), 0x805));
	hs_body_soundList.push_back((BGSSoundDescriptorForm*)GetFormFromMod(std::string("SimpleImpact.esp"), 0x806));
	hitSound = hs_body_soundList[0];
	for (auto it = hs_body_soundList.begin(); it != hs_body_soundList.end(); ++it) {
		_MESSAGE("HitSound %llx", *it);
	}

	hs_head_soundList.push_back((BGSSoundDescriptorForm*)GetFormFromMod(std::string("SimpleImpact.esp"), 0x808));
	hs_head_soundList.push_back((BGSSoundDescriptorForm*)GetFormFromMod(std::string("SimpleImpact.esp"), 0x809));
	hs_head_soundList.push_back((BGSSoundDescriptorForm*)GetFormFromMod(std::string("SimpleImpact.esp"), 0x80A));
	hs_head_soundList.push_back((BGSSoundDescriptorForm*)GetFormFromMod(std::string("SimpleImpact.esp"), 0x80B));
	hs_head_soundList.push_back((BGSSoundDescriptorForm*)GetFormFromMod(std::string("SimpleImpact.esp"), 0x80C));
	hs_head_soundList.push_back((BGSSoundDescriptorForm*)GetFormFromMod(std::string("SimpleImpact.esp"), 0x80D));
	hitSoundHead = hs_head_soundList[0];
	for (auto it = hs_head_soundList.begin(); it != hs_head_soundList.end(); ++it) {
		_MESSAGE("HitSound Headshot %llx", *it);
	}

	ks_body_soundList.push_back((BGSSoundDescriptorForm*)GetFormFromMod(std::string("SimpleImpact.esp"), 0x80E));
	ks_body_soundList.push_back((BGSSoundDescriptorForm*)GetFormFromMod(std::string("SimpleImpact.esp"), 0x80F));
	ks_body_soundList.push_back((BGSSoundDescriptorForm*)GetFormFromMod(std::string("SimpleImpact.esp"), 0x810));
	ks_body_soundList.push_back((BGSSoundDescriptorForm*)GetFormFromMod(std::string("SimpleImpact.esp"), 0x811));
	ks_body_soundList.push_back((BGSSoundDescriptorForm*)GetFormFromMod(std::string("SimpleImpact.esp"), 0x812));
	ks_body_soundList.push_back((BGSSoundDescriptorForm*)GetFormFromMod(std::string("SimpleImpact.esp"), 0x813));
	killSound = ks_body_soundList[0];
	for (auto it = ks_body_soundList.begin(); it != ks_body_soundList.end(); ++it) {
		_MESSAGE("KillSound %llx", *it);
	}

	ks_head_soundList.push_back((BGSSoundDescriptorForm*)GetFormFromMod(std::string("SimpleImpact.esp"), 0x814));
	ks_head_soundList.push_back((BGSSoundDescriptorForm*)GetFormFromMod(std::string("SimpleImpact.esp"), 0x815));
	ks_head_soundList.push_back((BGSSoundDescriptorForm*)GetFormFromMod(std::string("SimpleImpact.esp"), 0x816));
	killSoundHead = ks_head_soundList[0];
	for (auto it = ks_head_soundList.begin(); it != ks_head_soundList.end(); ++it) {
		_MESSAGE("KillSound Headshot %llx", *it);
	}

	killQuoteList = (BGSListForm*)GetFormFromMod(std::string("SimpleImpact.esp"), 0x817);
	for (uint32_t i = 0; i < killQuoteList->arrayOfForms.size(); ++i) {
		killQuoteQueue.push_back(i);
	}

	imod = (TESImageSpaceModifier*)GetFormFromMod(std::string("SimpleImpact.esp"), 0x807);
	_MESSAGE("ImageSpaceModifier %llx", imod);

	BGSKeyword* actorTypeLibertyPrime = (BGSKeyword*)GetFormFromMod(std::string("Fallout4.esm"), 0x10D529);
	BGSKeyword* actorTypeMirelurk = (BGSKeyword*)GetFormFromMod(std::string("Fallout4.esm"), 0x2CB71);
	BGSKeyword* actorTypeRobot = (BGSKeyword*)GetFormFromMod(std::string("Fallout4.esm"), 0x2CB73);
	BGSKeyword* actorTypeTurret = (BGSKeyword*)GetFormFromMod(std::string("Fallout4.esm"), 0xB2BF3);
	BGSKeyword* isVertibird = (BGSKeyword*)GetFormFromMod(std::string("Fallout4.esm"), 0xF9899);
	TESDataHandler* dh = TESDataHandler::GetSingleton();
	BSTArray<TESRace*> races = dh->GetFormArray<TESRace>();
	for (auto it = races.begin(); it != races.end(); ++it) {
		BGSKeyword** keywords = (*it)->keywords;
		uint32_t i = 0;
		while (i < (*it)->numKeywords) {
			if (keywords[i] == actorTypeLibertyPrime || keywords[i] == actorTypeMirelurk
				|| keywords[i] == actorTypeRobot || keywords[i] == actorTypeTurret || keywords[i] == isVertibird) {
				headshotExcludedList.push_back(*it);
				i = (*it)->numKeywords;
				_MESSAGE("%s excluded", (*it)->GetFullName());
			}
			++i;
		}
	}

	((AnimationGraphEventWatcher*)((uint64_t)p + 0x38))->HookSink();

	HitEventSink* sink = new HitEventSink();
	HitEventSource::GetSingleton()->RegisterSink(sink);

	EquipWatcher* ew = new EquipWatcher();
	EquipEventSource::GetSingleton()->RegisterSink(ew);

	DeathWatcher* dw = new DeathWatcher();
	DeathEventSource::GetSingleton()->RegisterSink(dw);

	MenuWatcher* mw = new MenuWatcher();
	UI::GetSingleton()->GetEventSource<MenuOpenCloseEvent>()->RegisterSink(mw);
}

void LoadConfigs() {
	_MESSAGE("Loading configs");
	ini.LoadFile("Data\\F4SE\\Plugins\\SimpleImpact.ini");
	enableHitSound = std::stoi(ini.GetValue("HitSound", "Enabled", "1")) > 0;
	if (enableHitSound)
		_MESSAGE("HitSound Enabled");
	hs_body_soundType = min(max(std::stoi(ini.GetValue("HitSound", "SoundType", "1")), 1), 7);
	hs_body_freqShift = (uint8_t)std::stoi(ini.GetValue("HitSound", "FrequencyShift", "0"));
	hs_body_freqVariance = (uint8_t)std::stoi(ini.GetValue("HitSound", "FrequencyVariance", "0"));
	hs_body_staticAttenuation = (uint16_t)std::stoi(ini.GetValue("HitSound", "StaticAttenuation", "7")) * 100;

	enableHitSoundHeadshot = std::stoi(ini.GetValue("HitSound", "HeadshotEnabled", "1")) > 0;
	if (enableHitSoundHeadshot)
		_MESSAGE("HitSound Headshot Enabled");
	hs_head_soundType = min(max(std::stoi(ini.GetValue("HitSound", "HeadshotSoundType", "1")), 1), 6);
	hs_head_freqShift = (uint8_t)std::stoi(ini.GetValue("HitSound", "HeadshotFrequencyShift", "0"));
	hs_head_freqVariance = (uint8_t)std::stoi(ini.GetValue("HitSound", "HeadshotFrequencyVariance", "0"));
	hs_head_staticAttenuation = (uint16_t)std::stoi(ini.GetValue("HitSound", "HeadshotStaticAttenuation", "7")) * 100;

	enableKillSound = std::stoi(ini.GetValue("KillSound", "Enabled", "1")) > 0;
	if (enableKillSound)
		_MESSAGE("KillSound Enabled");
	ks_body_soundType = min(max(std::stoi(ini.GetValue("KillSound", "SoundType", "1")), 1), 6);
	ks_body_freqShift = (uint8_t)std::stoi(ini.GetValue("KillSound", "FrequencyShift", "0"));
	ks_body_freqVariance = (uint8_t)std::stoi(ini.GetValue("KillSound", "FrequencyVariance", "0"));
	ks_body_staticAttenuation = (uint16_t)std::stoi(ini.GetValue("KillSound", "StaticAttenuation", "7")) * 100;

	enableKillSoundHeadshot = std::stoi(ini.GetValue("KillSound", "HeadshotEnabled", "1")) > 0;
	if (enableKillSoundHeadshot)
		_MESSAGE("KillSound Headshot Enabled");
	ks_head_soundType = min(max(std::stoi(ini.GetValue("KillSound", "HeadshotSoundType", "1")), 1), 3);
	ks_head_freqShift = (uint8_t)std::stoi(ini.GetValue("KillSound", "HeadshotFrequencyShift", "0"));
	ks_head_freqVariance = (uint8_t)std::stoi(ini.GetValue("KillSound", "HeadshotFrequencyVariance", "0"));
	ks_head_staticAttenuation = (uint16_t)std::stoi(ini.GetValue("KillSound", "HeadshotStaticAttenuation", "7")) * 100;

	enableKillQuote = std::stoi(ini.GetValue("KillQuote", "Enabled", "1")) > 0;
	kq_staticAttenuation = (uint16_t)std::stoi(ini.GetValue("KillQuote", "StaticAttenuation", "7")) * 100;
	kq_minimumCooldown = std::stof(ini.GetValue("KillQuote", "Cooldown", "2.0"));

	/* BGSSoundDescriptor
	* 0x8 BGSSoundCategory*
	* 0x18~0x28 BSTArray?
	* 0x38 int8 Frequency Shift
	* 0x39 int8 Frequency Variance
	* 0x3A int16 Priority
	* 0x3C int16 Static Attenuation * 100
	* 0x48 BGSSoundOutput
	*/

	hitSound = hs_body_soundList[hs_body_soundType - 1];
	*(uint8_t*)((uintptr_t)hitSound->impl + 0x38) = hs_body_freqShift;
	*(uint8_t*)((uintptr_t)hitSound->impl + 0x39) = hs_body_freqVariance;
	*(uint16_t*)((uintptr_t)hitSound->impl + 0x3C) = hs_body_staticAttenuation;

	hitSoundHead = hs_head_soundList[hs_head_soundType - 1];
	*(uint8_t*)((uintptr_t)hitSoundHead->impl + 0x38) = hs_head_freqShift;
	*(uint8_t*)((uintptr_t)hitSoundHead->impl + 0x39) = hs_head_freqVariance;
	*(uint16_t*)((uintptr_t)hitSoundHead->impl + 0x3C) = hs_head_staticAttenuation;

	playOnAliveOnly = std::stoi(ini.GetValue("HitSound", "AliveOnly", "1")) > 0;
	playOnGunOnly = std::stoi(ini.GetValue("HitSound", "GunOnly", "1")) > 0;

	killSound = ks_body_soundList[ks_body_soundType - 1];
	*(uint8_t*)((uintptr_t)killSound->impl + 0x38) = ks_body_freqShift;
	*(uint8_t*)((uintptr_t)killSound->impl + 0x39) = ks_body_freqVariance;
	*(uint16_t*)((uintptr_t)killSound->impl + 0x3C) = ks_body_staticAttenuation;

	killSoundHead = ks_head_soundList[ks_head_soundType - 1];
	*(uint8_t*)((uintptr_t)killSoundHead->impl + 0x38) = ks_head_freqShift;
	*(uint8_t*)((uintptr_t)killSoundHead->impl + 0x39) = ks_head_freqVariance;
	*(uint16_t*)((uintptr_t)killSoundHead->impl + 0x3C) = ks_head_staticAttenuation;
	
	for (auto it = killQuoteList->arrayOfForms.begin(); it != killQuoteList->arrayOfForms.end(); ++it) {
		if ((*it)->formType == ENUM_FORM_ID::kSNDR) {
			*(uint16_t*)((uintptr_t)((BGSSoundDescriptorForm*)*it)->impl + 0x3C) = kq_staticAttenuation;
		}
		else {
			_MESSAGE("Non-sound form detected in the form list!");
		}
	}

	enableShake = std::stoi(ini.GetValue("Shake", "Enabled", "1")) > 0;
	if (enableShake)
		_MESSAGE("Shake Enabled");
	durationMultiplier = std::stof(ini.GetValue("Shake", "DurationMultiplier", "1.0"));
	strengthMultiplier = std::stof(ini.GetValue("Shake", "StrengthMultiplier", "1.0"));
	PADurationMultiplier = std::stof(ini.GetValue("Shake", "PowerArmorDurationMultiplier", "0.7"));
	PAStrengthMultiplier = std::stof(ini.GetValue("Shake", "PowerArmorStrengthMultiplier", "0.7"));
	thirdPersonDurationMultiplier = std::stof(ini.GetValue("Shake", "ThirdPersonDurationMultiplier", "0.9"));
	thirdPersonStrengthMultiplier = std::stof(ini.GetValue("Shake", "ThirdPersonStrengthMultiplier", "0.8"));
	ADSDurationMultiplier = std::stof(ini.GetValue("Shake", "ADSDurationMultiplier", "0.7"));
	ADSStrengthMultiplier = std::stof(ini.GetValue("Shake", "ADSStrengthMultiplier", "0.8"));
	SneakDurationMultiplier = std::stof(ini.GetValue("Shake", "SneakDurationMultiplier", "0.9"));
	SneakStrengthMultiplier = std::stof(ini.GetValue("Shake", "SneakStrengthMultiplier", "0.9"));
	minimumDuration = std::stof(ini.GetValue("Shake", "MinimumDuration", "0.2"));
	maximumDuration = std::stof(ini.GetValue("Shake", "MaximumDuration", "0.5"));
	minimumStrength = std::stof(ini.GetValue("Shake", "MinimumStrength", "0.7"));
	maximumStrength = std::stof(ini.GetValue("Shake", "MaximumStrength", "1.0"));
	rofCoefficient = std::stof(ini.GetValue("Shake", "RoFCoefficient", "3.0"));
	damageCoefficient = std::stof(ini.GetValue("Shake", "DamageCoefficient", "10.0"));

	enableFX = std::stoi(ini.GetValue("FX", "Enabled", "1")) > 0;
	if (enableFX)
		_MESSAGE("FX Enabled");
	minimumFXStrength = std::stof(ini.GetValue("FX", "MinimumStrength", "0.5"));
	maximumFXStrength = std::stof(ini.GetValue("FX", "MaximumStrength", "0.8"));

	if (enableShake || enableFX) {
		CalculateScore(p);
	}

	if (!aimRecoveryDisabled && std::stoi(ini.GetValue("Extra", "DisableAimRecovery", "1")) > 0) {
		_MESSAGE("Disabling aim recovery...");
		TESDataHandler* dh = TESDataHandler::GetSingleton();
		BSTArray<BGSAimModel*> aimModels = dh->GetFormArray<BGSAimModel>();
		for (auto it = aimModels.begin(); it != aimModels.end(); ++it) {
			(*it)->aimModelData.aimModelRecoilDiminishSpringForce = 0;
		}
		aimRecoveryDisabled = true;
	}
	disableOnFreecam = std::stoi(ini.GetValue("Extra", "DisableOnFreecam", "1")) > 0;
	ini.Reset();

	customShakeDuration.clear();
	customShakeStrength.clear();
	customFXStrength.clear();
	ifstream reader;
	reader.open("Data\\F4SE\\Plugins\\SimpleImpactCustom.json");
	nlohmann::json customData;
	reader >> customData;
	for (auto pluginit = customData.begin(); pluginit != customData.end(); ++pluginit) {
		for (auto formit = pluginit.value().begin(); formit != pluginit.value().end(); ++formit) {
			TESForm* form = GetFormFromMod(pluginit.key(), std::stoi(formit.key(), 0, 16));
			if (form) {
				for (auto valit = formit.value().begin(); valit != formit.value().end(); ++valit) {
					if (valit.key() == "ShakeDuration") {
						customShakeDuration.insert(std::pair<TESForm*, float>(form, valit.value().get<float>()));
					}
					else if (valit.key() == "ShakeStrength") {
						customShakeStrength.insert(std::pair<TESForm*, float>(form, valit.value().get<float>()));
					}
					else if (valit.key() == "FXStrength") {
						customFXStrength.insert(std::pair<TESForm*, float>(form, valit.value().get<float>()));
					}
				}
			}
		}
	}
	reader.close();

	_MESSAGE("Done!");
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

	*path /= fmt::format(FMT_STRING("{}.log"), Version::PROJECT);
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

	logger::info(FMT_STRING("{} v{}"), Version::PROJECT, Version::NAME);

	a_info->infoVersion = F4SE::PluginInfo::kVersion;
	a_info->name = Version::PROJECT.data();
	a_info->version = Version::MAJOR;

	if (a_f4se->IsEditor()) {
		logger::critical("loaded in editor"sv);
		return false;
	}

	const auto ver = a_f4se->RuntimeVersion();
	if (ver < F4SE::RUNTIME_1_10_162) {
		logger::critical(FMT_STRING("unsupported runtime v{}"), ver.string());
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
			InitializePlugin();
			LoadConfigs();
		}
		else if (msg->type == F4SE::MessagingInterface::kPostLoadGame) {
			LoadConfigs();
		}
		else if (msg->type == F4SE::MessagingInterface::kNewGame) {
			LoadConfigs();
		}
	});

	return true;
}
