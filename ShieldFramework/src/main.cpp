#include <Windows.h>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include "nlohmann/json.hpp"

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

struct ShieldCollisionBound {
	std::string boneOrigin;
	hkVector4f pos;
	hkVector4f rot;
	hkVector4f halfExtent;
	ShieldCollisionBound() {};
	ShieldCollisionBound(std::string orig, hkVector4f p, hkVector4f r, hkVector4f he) {
		boneOrigin = orig;
		pos = p;
		rot = r;
		halfExtent = he;
	}
};

struct ShieldData {
	BGSMaterialType* material = nullptr;
	SpellItem* spell = nullptr;
	float damageThreshold = 0.0f;
	ShieldCollisionBound bounds;
};

using std::unordered_map;
using std::vector;
ptrdiff_t ProcessProjectileFX_PatchOffset = 0x1A1;
REL::Relocation<uintptr_t> ProcessProjectileFX{ REL::ID(806412), ProcessProjectileFX_PatchOffset };
static unordered_map<Actor*, BGSMaterialType*> originalMaterialMap;
static unordered_map<uint32_t, vector<std::string>> shieldPartsMap;
static unordered_map<uint32_t, vector<ShieldData>> shieldDataMap;
static vector<uint32_t> shieldIDs;
static ActorValueInfo* damageThresholdAdd;
static ActorValueInfo* damageThresholdMul;

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
			if (a_event.victim && a_event.victim->formType == ENUM_FORM_ID::kACHR) {
				//Actor* a = (Actor*)a_event.victim;
				return BSEventNotifyControl::kStop;
			}
		}
		return BSEventNotifyControl::kContinue;
	}
	HitEventSink() {};
	virtual ~HitEventSink() {};
	F4_HEAP_REDEFINE_NEW(HitEventSink);
};

struct RayCastResult {
	bool hit;
	hkVector4f pos;
	RayCastResult(bool h, hkVector4f p) {
		hit = h;
		pos = p;
	}
};

RayCastResult RayCastShield(hkVector4f worldOrigin, hkVector4f direction, NiNode* bone, hkVector4f pos, hkVector4f rot, hkVector4f halfExtent) {
	if (!bone) return RayCastResult(false, hkVector4f());
	//TODO:Implement this using https://gdbooks.gitbooks.io/3dcollisions/content/Chapter3/raycast_aabb.html
	return RayCastResult(false, hkVector4f());
}

//This will intercept ProcessImpacts() function in Projectile's vtable and check if the collidee is holding a shield.
class ProjectileHooks : public Projectile {
public:
	typedef bool (ProjectileHooks:: * FnProcessImpacts)();

	bool CheckShield() {
		for (auto it = this->impacts.begin(); it != this->impacts.end(); ++it) {
			if (it->processed || !it->collidee.get() || it->collidee.get()->GetFormType() != ENUM_FORM_ID::kACHR)
				continue;

			Actor* a = (Actor*)it->collidee.get().get();
			if (!a || !a->currentProcess || !a->Get3D())
				continue;

			BSTArray<EquippedItem> equipped = a->currentProcess->middleHigh->equippedItems;
			for (auto eqit = equipped.begin(); eqit != equipped.end(); ++eqit) {
				for (int i = 0; i < shieldIDs.size(); ++i) {
					uint32_t eqipid = eqit->item.object->formID;
					logger::warn(_MESSAGE("equipment formid %llx", eqipid));
					if (shieldIDs[i] == eqit->item.object->formID) {
						auto partslookup = shieldPartsMap.find(eqipid);
						if (partslookup != shieldPartsMap.end()) {
							vector<std::string> parts = partslookup->second;
							auto datalookup = shieldDataMap.find(eqipid);
							vector<ShieldData> sd = datalookup->second;

							for (int j = 0; j < parts.size(); ++j) {
								NiAVObject* bone = a->Get3D()->GetObjectByName(BSFixedString(sd[j].bounds.boneOrigin));
								if (bone) {
									hkVector4f projOrigin = hkVector4f(it->location) - hkVector4f(this->movementDirection) * this->distanceMoved;
									RayCastResult result = RayCastShield(projOrigin, this->movementDirection, bone->IsNode(), sd[j].bounds.pos, sd[j].bounds.rot, sd[j].bounds.halfExtent);
									if (result.hit) {
										it->location = result.pos;
										if (sd[j].material) {
											it->materialType = sd[j].material;
										}
										if (sd[j].spell) {
											sd[j].spell->Cast(this->shooter.get().get(), a);
										}
										float dtAdd = a->GetActorValue(*damageThresholdAdd);
										float dtMul = a->GetActorValue(*damageThresholdMul);
										if (sd[j].damageThreshold <= 0
											|| this->damage < (sd[j].damageThreshold + dtAdd) * dtMul) {
											this->damage = 0.0f;
										}
									}
								}
							}
						}
					}
				}
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

BGSMaterialType* GetMaterialTypeByName(std::string materialname) {
	TESDataHandler* dh = TESDataHandler::GetSingleton();
	BSTArray<BGSMaterialType*> materials = dh->GetFormArray<BGSMaterialType>();
	BSFixedString matname = BSFixedString(materialname);
	for (auto it = materials.begin(); it != materials.end(); ++it) {
		if ((*it)->materialName == matname) {
			return (*it);
		}
	}
	return nullptr;
}

EffectSetting* GetMagicEffectByFullName(std::string effectname) {
	TESDataHandler* dh = TESDataHandler::GetSingleton();
	BSTArray<EffectSetting*> mgefs = dh->GetFormArray<EffectSetting>();
	for (auto it = mgefs.begin(); it != mgefs.end(); ++it) {
		if (strcmp((*it)->GetFullName(), effectname.c_str()) == 0) {
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

void InitializeShieldData() {
	namespace fs = std::filesystem;
	fs::path jsonPath = fs::current_path();
	jsonPath += "\\Data\\F4SE\\Plugins\\ShieldFramework\\ShieldData";
	std::stringstream stream;
	fs::directory_entry jsonEntry{ jsonPath };
	if (!jsonEntry.exists()) {
		logger::warn("Shield Data directory does not exist!"sv);
		return;
	}
	for (auto& it : fs::directory_iterator(jsonEntry)) {
		if (it.path().extension().compare(".json") == 0) {
			stream << it.path().filename();
			logger::warn(_MESSAGE("Loading shield data %s", stream.str().c_str()));
			stream.str(std::string());
			std::ifstream reader;
			reader.open(it.path());
			nlohmann::json j;
			reader >> j;

			for (auto modit = j.begin(); modit != j.end(); ++modit) {
				for (auto formit = (*modit).begin(); formit != (*modit).end(); ++formit) {
					TESForm* shieldForm = GetFormFromMod(modit.key(), std::stoi(formit.key(), 0, 16));
					uint32_t formid = shieldForm->formID;
					shieldIDs.push_back(formid);
					logger::warn(_MESSAGE("Shield FormID %llx", formid));
					if (shieldPartsMap.find(formid) == shieldPartsMap.end()) {
						vector<std::string> parts;
						vector<ShieldData> shielddata;
						for (auto partit = (*formit).begin(); partit != (*formit).end(); ++partit) {
							parts.push_back(partit.key());

							ShieldData sd;
							auto lookup = (*partit).find("MaterialType");
							if (lookup != (*partit).end()) {
								BGSMaterialType* result = GetMaterialTypeByName(lookup.value().get<std::string>());
								if (result) {
									sd.material = result;
									logger::warn(_MESSAGE("MaterialType %s found at %llx", lookup.value().get<std::string>().c_str(), result));
								}
								else {
									logger::warn(_MESSAGE("MaterialType %s not found", lookup.value().get<std::string>().c_str()));
								}
							}
							lookup = (*partit).find("Spell");
							if (lookup != (*partit).end()) {
								std::string spellName = lookup.value().get<std::string>();
								if (spellName.length() > 0) {
									SpellItem* result = GetSpellByFullName(spellName);
									if (result) {
										sd.spell = result;
										logger::warn(_MESSAGE("Spell %s found at %llx", lookup.value().get<std::string>().c_str(), result));
									}
									else {
										logger::warn(_MESSAGE("Spell %s not found", lookup.value().get<std::string>().c_str()));
									}
								}
							}
							lookup = (*partit).find("DamageThreshold");
							if (lookup != (*partit).end()) {
								sd.damageThreshold = lookup.value().get<float>();
								logger::warn(_MESSAGE("DamageThreshold set to %f", sd.damageThreshold));
							}
							lookup = (*partit).find("CollisionBound");
							if (lookup != (*partit).end()) {
								std::string bone = lookup.value()["Bone"].get<std::string>();
								float posX = lookup.value()["PosX"].get<float>();
								float posY = lookup.value()["PosY"].get<float>();
								float posZ = lookup.value()["PosZ"].get<float>();
								float rotX = lookup.value()["RotX"].get<float>();
								float rotY = lookup.value()["RotY"].get<float>();
								float rotZ = lookup.value()["RotZ"].get<float>();
								float heX = lookup.value()["HalfExtentX"].get<float>();
								float heY = lookup.value()["HalfExtentY"].get<float>();
								float heZ = lookup.value()["HalfExtentZ"].get<float>();
								sd.bounds = ShieldCollisionBound(bone, hkVector4f(posX, posY, posZ), hkVector4f(rotX, rotY, rotZ), hkVector4f(heX, heY, heZ));
								logger::warn(_MESSAGE("CollisionBound Bone %s\nPos\tX %f\tY %f\tZ %f\nRot\tX %f\tY %f\tZ %f\nHalfExtent\tX %f\tY %f\tZ %f"
													  , bone.c_str(), posX, posY, posZ, rotX, rotY, rotZ, heX, heY, heZ));
							}
							shielddata.push_back(sd);
						}
						shieldPartsMap.insert(std::pair<uint32_t, vector<std::string>>(formid, parts));
						shieldDataMap.insert(std::pair<uint32_t, vector<ShieldData>>(formid, shielddata));
					}
				}
			}
		}
	}
}

void InitializeImpactData() {
	namespace fs = std::filesystem;
	fs::path jsonPath = fs::current_path();
	jsonPath += "\\Data\\F4SE\\Plugins\\ShieldFramework\\ImpactData";
	std::stringstream stream;
	fs::directory_entry jsonEntry{ jsonPath };
	if (!jsonEntry.exists()) {
		logger::warn("ImpactData directory does not exist!"sv);
		return;
	}
	BGSImpactDataSet* bullet_normal = (BGSImpactDataSet*)TESForm::GetFormByID(0x004B37);
	BGSImpactDataSet* bullet_heavy = (BGSImpactDataSet*)TESForm::GetFormByID(0x05DDEE);
	BGSImpactDataSet* bullet_explosive = (BGSImpactDataSet*)TESForm::GetFormByID(0x21C79C);
	BGSImpactDataSet* bullet_noparallax = (BGSImpactDataSet*)TESForm::GetFormByID(0x22CC14);
	BGSImpactDataSet* bullet_incendiary = (BGSImpactDataSet*)TESForm::GetFormByID(0x1B5EE6);
	BGSImpactDataSet* bullet_cryo = (BGSImpactDataSet*)TESForm::GetFormByID(0x06F11F);
	BGSImpactDataSet* flamethrower = (BGSImpactDataSet*)TESForm::GetFormByID(0x0D76F5);
	for (auto& it : fs::directory_iterator(jsonEntry)) {
		if (it.path().extension().compare(".json") == 0) {
			stream << it.path().filename();
			logger::warn(_MESSAGE("Loading impact data %s", stream.str().c_str()));
			stream.str(std::string());
			std::ifstream reader;
			reader.open(it.path());
			nlohmann::json j;
			reader >> j;

			for (auto typeit = j.begin(); typeit != j.end(); ++typeit) {
				if (typeit.key() == "Normal") {
					for (auto impactit = (*typeit).begin(); impactit != (*typeit).end(); ++impactit) {
						auto matlookup = impactit.value().find("MaterialType");
						if (matlookup == impactit.value().end())
							continue;
						auto ipctlookup = impactit.value().find("Impact");
						if (ipctlookup == impactit.value().end())
							continue;
						std::string matname = matlookup.value().get<std::string>();
						if (matname.length() == 0)
							continue;
						BGSMaterialType* mat = GetMaterialTypeByName(matlookup.value().get<std::string>());
						BGSImpactData* ipct = (BGSImpactData*)GetFormFromMod((*ipctlookup.value().find("Mod")).get<std::string>(), 
																			 std::stoi((*ipctlookup.value().find("FormID")).get<std::string>(), 0, 16));
						if (mat && ipct) {
							bullet_normal->impactMap.insert(BSTTuple<BGSMaterialType*, BGSImpactData*>(mat, ipct));
						}
						else {
							logger::error(_MESSAGE("Impact data for %s type failed! Mat : %s, ImpactData %llx", matname.c_str(), ipct));
						}
					}
				}
				else if (typeit.key() == "Heavy") {
					for (auto impactit = (*typeit).begin(); impactit != (*typeit).end(); ++impactit) {
						auto matlookup = impactit.value().find("MaterialType");
						if (matlookup == impactit.value().end())
							continue;
						auto ipctlookup = impactit.value().find("Impact");
						if (ipctlookup == impactit.value().end())
							continue;
						std::string matname = matlookup.value().get<std::string>();
						if (matname.length() == 0)
							continue;
						BGSMaterialType* mat = GetMaterialTypeByName(matlookup.value().get<std::string>());
						BGSImpactData* ipct = (BGSImpactData*)GetFormFromMod((*ipctlookup.value().find("Mod")).get<std::string>(),
																			 std::stoi((*ipctlookup.value().find("FormID")).get<std::string>(), 0, 16));
						if (mat && ipct) {
							bullet_heavy->impactMap.insert(BSTTuple<BGSMaterialType*, BGSImpactData*>(mat, ipct));
						}
						else {
							logger::error(_MESSAGE("Impact data for %s type failed! Mat : %s, ImpactData %llx", matname.c_str(), ipct));
						}
					}
				}
				else if (typeit.key() == "Explosive") {
					for (auto impactit = (*typeit).begin(); impactit != (*typeit).end(); ++impactit) {
						auto matlookup = impactit.value().find("MaterialType");
						if (matlookup == impactit.value().end())
							continue;
						auto ipctlookup = impactit.value().find("Impact");
						if (ipctlookup == impactit.value().end())
							continue;
						std::string matname = matlookup.value().get<std::string>();
						if (matname.length() == 0)
							continue;
						BGSMaterialType* mat = GetMaterialTypeByName(matlookup.value().get<std::string>());
						BGSImpactData* ipct = (BGSImpactData*)GetFormFromMod((*ipctlookup.value().find("Mod")).get<std::string>(),
																			 std::stoi((*ipctlookup.value().find("FormID")).get<std::string>(), 0, 16));
						if (mat && ipct) {
							bullet_explosive->impactMap.insert(BSTTuple<BGSMaterialType*, BGSImpactData*>(mat, ipct));
						}
						else {
							logger::error(_MESSAGE("Impact data for %s type failed! Mat : %s, ImpactData %llx", matname.c_str(), ipct));
						}
					}
				}
				else if (typeit.key() == "NoParallax") {
					for (auto impactit = (*typeit).begin(); impactit != (*typeit).end(); ++impactit) {
						auto matlookup = impactit.value().find("MaterialType");
						if (matlookup == impactit.value().end())
							continue;
						auto ipctlookup = impactit.value().find("Impact");
						if (ipctlookup == impactit.value().end())
							continue;
						std::string matname = matlookup.value().get<std::string>();
						if (matname.length() == 0)
							continue;
						BGSMaterialType* mat = GetMaterialTypeByName(matlookup.value().get<std::string>());
						BGSImpactData* ipct = (BGSImpactData*)GetFormFromMod((*ipctlookup.value().find("Mod")).get<std::string>(),
																			 std::stoi((*ipctlookup.value().find("FormID")).get<std::string>(), 0, 16));
						if (mat && ipct) {
							bullet_noparallax->impactMap.insert(BSTTuple<BGSMaterialType*, BGSImpactData*>(mat, ipct));
						}
						else {
							logger::error(_MESSAGE("Impact data for %s type failed! Mat : %s, ImpactData %llx", matname.c_str(), ipct));
						}
					}
				}
				else if (typeit.key() == "Incendiary") {
					for (auto impactit = (*typeit).begin(); impactit != (*typeit).end(); ++impactit) {
						auto matlookup = impactit.value().find("MaterialType");
						if (matlookup == impactit.value().end())
							continue;
						auto ipctlookup = impactit.value().find("Impact");
						if (ipctlookup == impactit.value().end())
							continue;
						std::string matname = matlookup.value().get<std::string>();
						if (matname.length() == 0)
							continue;
						BGSMaterialType* mat = GetMaterialTypeByName(matlookup.value().get<std::string>());
						BGSImpactData* ipct = (BGSImpactData*)GetFormFromMod((*ipctlookup.value().find("Mod")).get<std::string>(),
																			 std::stoi((*ipctlookup.value().find("FormID")).get<std::string>(), 0, 16));
						if (mat && ipct) {
							bullet_incendiary->impactMap.insert(BSTTuple<BGSMaterialType*, BGSImpactData*>(mat, ipct));
						}
						else {
							logger::error(_MESSAGE("Impact data for %s type failed! Mat : %s, ImpactData %llx", matname.c_str(), ipct));
						}
					}
				}
				else if (typeit.key() == "Cryo") {
					for (auto impactit = (*typeit).begin(); impactit != (*typeit).end(); ++impactit) {
						auto matlookup = impactit.value().find("MaterialType");
						if (matlookup == impactit.value().end())
							continue;
						auto ipctlookup = impactit.value().find("Impact");
						if (ipctlookup == impactit.value().end())
							continue;
						std::string matname = matlookup.value().get<std::string>();
						if (matname.length() == 0)
							continue;
						BGSMaterialType* mat = GetMaterialTypeByName(matlookup.value().get<std::string>());
						BGSImpactData* ipct = (BGSImpactData*)GetFormFromMod((*ipctlookup.value().find("Mod")).get<std::string>(),
																			 std::stoi((*ipctlookup.value().find("FormID")).get<std::string>(), 0, 16));
						if (mat && ipct) {
							bullet_cryo->impactMap.insert(BSTTuple<BGSMaterialType*, BGSImpactData*>(mat, ipct));
						}
						else {
							logger::error(_MESSAGE("Impact data for %s type failed! Mat : %s, ImpactData %llx", matname.c_str(), ipct));
						}
					}
				}
				else if (typeit.key() == "FlameThrower") {
					for (auto impactit = (*typeit).begin(); impactit != (*typeit).end(); ++impactit) {
						auto matlookup = impactit.value().find("MaterialType");
						if (matlookup == impactit.value().end())
							continue;
						auto ipctlookup = impactit.value().find("Impact");
						if (ipctlookup == impactit.value().end())
							continue;
						std::string matname = matlookup.value().get<std::string>();
						if (matname.length() == 0)
							continue;
						BGSMaterialType* mat = GetMaterialTypeByName(matlookup.value().get<std::string>());
						BGSImpactData* ipct = (BGSImpactData*)GetFormFromMod((*ipctlookup.value().find("Mod")).get<std::string>(),
																			 std::stoi((*ipctlookup.value().find("FormID")).get<std::string>(), 0, 16));
						if (mat && ipct) {
							flamethrower->impactMap.insert(BSTTuple<BGSMaterialType*, BGSImpactData*>(mat, ipct));
						}
						else {
							logger::error(_MESSAGE("Impact data for %s type failed! Mat : %s, ImpactData %llx", matname.c_str(), ipct));
						}
					}
				}
			}
		}
	}
}


void InitializeFramework() {
	uint64_t addr;
	uint64_t offset = 0x680;
	addr = Projectile::VTABLE[0].address();
	logger::warn(_MESSAGE("Patching Projectile %llx", addr));
	ProjectileHooks::HookProcessImpacts(addr, offset);

	addr = MissileProjectile::VTABLE[0].address();
	logger::warn(_MESSAGE("Patching MissileProjectile %llx", addr));
	ProjectileHooks::HookProcessImpacts(addr, offset);

	addr = ArrowProjectile::VTABLE[0].address();
	logger::warn(_MESSAGE("Patching ArrowProjectile %llx", addr));
	ProjectileHooks::HookProcessImpacts(addr, offset);

	addr = GrenadeProjectile::VTABLE[0].address();
	logger::warn(_MESSAGE("Patching GrenadeProjectile %llx", addr));
	ProjectileHooks::HookProcessImpacts(addr, offset);

	addr = BeamProjectile::VTABLE[0].address();
	logger::warn(_MESSAGE("Patching BeamProjectile %llx", addr));
	ProjectileHooks::HookProcessImpacts(addr, offset);

	addr = FlameProjectile::VTABLE[0].address();
	logger::warn(_MESSAGE("Patching FlameProjectile %llx", addr));
	ProjectileHooks::HookProcessImpacts(addr, offset);

	addr = ConeProjectile::VTABLE[0].address();
	logger::warn(_MESSAGE("Patching ConeProjectile %llx", addr));
	ProjectileHooks::HookProcessImpacts(addr, offset);

	addr = BarrierProjectile::VTABLE[0].address();
	logger::warn(_MESSAGE("Patching BarrierProjectile %llx", addr));
	ProjectileHooks::HookProcessImpacts(addr, offset);

	//Fallout4.exe+0xFD58A0 is the function that plays the impact effect based on various impact data.
	//0x1A1 from this function is the part where it checks the form type of collidee and play the impact effect based on the race data.
	//This will modify that behavior, forcing the game to always use the projectile impact data's materialType for the impact effect.
	logger::warn(_MESSAGE("Patching impact form type check %llx", ProcessProjectileFX.address()));
	REL::safe_write(ProcessProjectileFX.address(), (uint8_t)0xEB);

	damageThresholdAdd = GetAVIFByEditorID(std::string("ShieldDTAdd"));
	damageThresholdMul = GetAVIFByEditorID(std::string("ShieldDTMul"));

	logger::warn(_MESSAGE("PlayerCharacter %llx", PlayerCharacter::GetSingleton()));
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
		if (msg->type == F4SE::MessagingInterface::kGameLoaded) {
			InitializeShieldData();
			InitializeImpactData();
			InitializeFramework();

			HitEventSource* src = HitEventSource::GetSingleton();
			logger::warn(_MESSAGE("HitEventSource %llx", src));
			HitEventSink* sink = new HitEventSink();
			src->RegisterSink(sink);

		}
	});
	return true;
}
