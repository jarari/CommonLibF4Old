#include "SimpleIni.h"
#include <random>
using namespace RE;

#pragma region Utilities

char tempbuf[8192] = { 0 };
char* _MESSAGE(const char* fmt, ...) {
	va_list args;

	va_start(args, fmt);
	vsprintf_s(tempbuf, sizeof(tempbuf), fmt, args);
	va_end(args);

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
			spdlog::log(spdlog::level::warn, _MESSAGE("%s", stream.str().c_str()));
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

#pragma endregion

#pragma region GlobalVariables

//ini 정의, UTF-8 사용, 멀티키 X, 멀티라인 X 설정
CSimpleIniA ini(true, false, false);
std::unordered_map<TESAmmo*, bool> ammoWhitelist;
std::vector<BGSProjectile*> EFDProjectiles;
std::vector<uint16_t> helmetRatings;
std::vector<uint16_t> vestRatings;
std::vector<std::string> cartridges;
std::unordered_map<std::string, std::vector<uint16_t>> protectionChances;
std::unordered_map<std::string, std::vector<float>> deathConditions;
std::vector<uint32_t> deathmarkMGEFs;
ActorValueInfo* helmetTier;
ActorValueInfo* vestTier;
ActorValueInfo* deathmarked;
ActorValueInfo* lasthitpart;
ActorValueInfo* enduranceCondition;
ActorValueInfo* perceptionCondition;
ActorValueInfo* brainCondition;
SpellItem* headHitMark;
SpellItem* torsoHitMark;
SpellItem* killDeathMarked;
EffectSetting* headHitMarkMGEF;
EffectSetting* torsoHitMarkMGEF;
std::vector<BGSBodyPartData::PartType> torsoParts = { BGSBodyPartData::PartType::COM, BGSBodyPartData::PartType::Torso, BGSBodyPartData::PartType::Pelvis };
std::vector<BGSBodyPartData::PartType> headParts = { BGSBodyPartData::PartType::Eye, BGSBodyPartData::PartType::Head1, BGSBodyPartData::PartType::Head2, BGSBodyPartData::PartType::Brain };
enum EFDBodyParts {
	Head,
	Torso,
	LArm,
	LLeg,
	RArm,
	RLeg
};
struct BleedingData {
	SpellItem* spell;
	float initial;
	float duration;
	float multiplier;
};
std::unordered_map<EFDBodyParts, BleedingData> bleedingConfigs;

#pragma endregion

#pragma region Data Initial Setup
//탄약 화이트리스트 함수
std::unordered_map<TESAmmo*, bool> GetAmmunitionWhitelist() {
	std::unordered_map<TESAmmo*, bool> ret;
	//ini 내 설정 읽기
	CSimpleIniA::TNamesDepend ammoKeys;
	ini.GetAllKeys("ManagedAmmunitionsFile", ammoKeys);
	spdlog::log(spdlog::level::warn, "Ammunition file list");

	//로그에 파일 목록 출력
	for (auto key = ammoKeys.begin(); key != ammoKeys.end(); ++key) {
		spdlog::log(spdlog::level::warn, _MESSAGE("%s", key->pItem));
	}

	//게임에 로드된 폼 중 탄약만 가져온다
	TESDataHandler* dh = TESDataHandler::GetSingleton();
	BSTArray<TESAmmo*> ammos = dh->GetFormArray<TESAmmo>();
	
	//모든 탄약 폼에 대하여
	for (auto it = ammos.begin(); it != ammos.end(); ++it) {
		bool whitelisted = false;
		
		//폼을 변조하는 파일 목록과 ini에 설정된 파일 대조
		for (auto key = ammoKeys.begin(); key != ammoKeys.end(); ++key) {
			const TESFileArray* fa = (*it)->sourceFiles.array;
			for (auto file = fa->begin(); file != fa->end(); ++file) {
				if (strcmp(key->pItem, (*file)->filename) == 0
					&& (*it)->data.damage != 0) {
					spdlog::log(spdlog::level::warn, _MESSAGE("%s added to the whitelist", (*it)->fullName.c_str()));
					whitelisted = true;
					ret.insert(std::pair<TESAmmo*, bool>(*it, true));
					break;
				}
			}
			if (whitelisted)
				break;
		}
	}
	return ret;
}

void SetupWeapons() {
	uint16_t uniqueDmg = (uint16_t)std::stoi(ini.GetValue("General", "UniqueDamage", "15"));
	spdlog::log(spdlog::level::warn, _MESSAGE("UniqueDamage %d", uniqueDmg));

	//탄약 화이트리스트 제작
	ammoWhitelist = GetAmmunitionWhitelist();

	for (auto it = ammoWhitelist.begin(); it != ammoWhitelist.end(); ++it) {
		BGSProjectile* ammoProj = it->first->data.projectile;
		EFDProjectiles.push_back(ammoProj);
		EFDProjectiles.push_back(ammoProj->data.vatsProjectile);
	}

	//바닐라에서 총과 유니크 무기의 정의를 가져옴
	BGSListForm* gunsKeywordList = (BGSListForm*)TESForm::GetFormByID(0xF78EC);
	BGSKeyword* uniqueKeyword = (BGSKeyword*)TESForm::GetFormByID(0x1B3FAC);

	spdlog::log(spdlog::level::warn, "Keyword list found.");
	//게임에 로드된 모든 무기 폼에 대하여
	TESDataHandler* dh = TESDataHandler::GetSingleton();
	BSTArray<TESObjectWEAP*> weapons = dh->GetFormArray<TESObjectWEAP>();
	for (auto it = weapons.begin(); it != weapons.end(); ++it) {
		TESObjectWEAP* wep = (*it);
		uint32_t i = 0;
		bool found = false;
		//무기에 부착물을 제외하고 기본적으로 정의된 키워드를 살펴본다
		while (i < wep->numKeywords && !found) {
			for (auto lit = gunsKeywordList->arrayOfForms.begin(); lit != gunsKeywordList->arrayOfForms.end(); ++lit) {
				//무기가 총기로 확인되면
				if (wep->keywords[i] == (*lit)) {
					//탄약 화이트리스트와 대조
					TESAmmo* ammo = wep->weaponData.ammo;
					if (ammo && ammoWhitelist.find(ammo) != ammoWhitelist.end()) {
						spdlog::log(spdlog::level::warn, _MESSAGE("Weapon Found : %s (FormID %llx at %llx)", wep->fullName.c_str(), wep->formID, wep));
						uint16_t oldDamage = wep->weaponData.attackDamage;
						//유니크 확인
						bool isUnique = false;
						for (uint32_t j = 0; j < wep->numKeywords; ++j) {
							if (wep->keywords[i] == uniqueKeyword) {
								isUnique = true;
							}
						}
						wep->weaponData.attackDamage = isUnique * uniqueDmg;
						spdlog::log(spdlog::level::warn, _MESSAGE("Old dmg %d, New dmg 0", oldDamage));
						found = true;
					}
				}
			}
			++i;
		}
	}
}

void SetupArmors() {
	CSimpleIniA::TNamesDepend helmetKeys;
	ini.GetAllKeys("Helmet", helmetKeys);
	spdlog::log(spdlog::level::warn, _MESSAGE("Helmet Tier References"));
	for (auto key = helmetKeys.begin(); key != helmetKeys.end(); ++key) {
		uint16_t ref = (uint16_t)std::stoi(ini.GetValue("Helmet", key->pItem));
		helmetRatings.push_back(ref);
		spdlog::log(spdlog::level::warn, _MESSAGE("%d", ref));
	}

	CSimpleIniA::TNamesDepend vestKeys;
	ini.GetAllKeys("Vest", vestKeys);
	spdlog::log(spdlog::level::warn, _MESSAGE("Vest Tier References"));
	for (auto key = vestKeys.begin(); key != vestKeys.end(); ++key) {
		uint16_t ref = (uint16_t)std::stoi(ini.GetValue("Vest", key->pItem));
		vestRatings.push_back(ref);
		spdlog::log(spdlog::level::warn, _MESSAGE("%d", ref));
	}

	helmetTier = GetAVIFByEditorID("EFD_Helmet_Tier");
	vestTier = GetAVIFByEditorID("EFD_Vest_Tier");
	spdlog::log(spdlog::level::warn, _MESSAGE("EFD_Helmet_Tier %llx", helmetTier));
	spdlog::log(spdlog::level::warn, _MESSAGE("EFD_Vest_Tier %llx", vestTier));

	TESDataHandler* dh = TESDataHandler::GetSingleton();
	BSTArray<TESObjectARMO*> armors = dh->GetFormArray<TESObjectARMO>();
	for (auto it = armors.begin(); it != armors.end(); ++it) {
		TESObjectARMO* armor = (*it);
		if (armor->bipedModelData.bipedObjectSlots & 0x800 || armor->bipedModelData.bipedObjectSlots & 0x40) {		//Vest
			spdlog::log(spdlog::level::warn, _MESSAGE("Vest\t\t%s\t\t\tDR %d\t\t(FormID %llx at %llx)", armor->fullName.c_str(), armor->data.rating, armor->formID, armor));
		}
		else if (armor->bipedModelData.bipedObjectSlots & 0x7) {	//Helmet
			spdlog::log(spdlog::level::warn, _MESSAGE("Helmet\t%s\t\t\t\tDR %d\t\t(FormID %llx at %llx)", armor->fullName.c_str(), armor->data.rating, armor->formID, armor));
		}
	}
}

void SetupDeathMark() {
	CSimpleIniA::TNamesDepend cartridgeKeys;
	ini.GetAllKeys("Cartridges", cartridgeKeys);
	spdlog::log(spdlog::level::warn, _MESSAGE("Cartridges"));
	for (auto key = cartridgeKeys.begin(); key != cartridgeKeys.end(); ++key) {
		cartridges.push_back(ini.GetValue("Cartridges", key->pItem));
		spdlog::log(spdlog::level::warn, _MESSAGE("%s", ini.GetValue("Cartridges", key->pItem)));
	}

	for (auto it = cartridges.begin(); it != cartridges.end(); ++it) {
		spdlog::log(spdlog::level::warn, _MESSAGE("%s Death Conditions", it->c_str()));
		std::vector<float> conditions;
		float headChance = std::stof(ini.GetValue("CartridgeFatalities", (*it + "Head").c_str()));
		spdlog::log(spdlog::level::warn, _MESSAGE("Head %f", headChance));
		float torsoChance = std::stof(ini.GetValue("CartridgeFatalities", (*it + "Torso").c_str()));
		spdlog::log(spdlog::level::warn, _MESSAGE("Torso %f", torsoChance));
		conditions.push_back(headChance);
		conditions.push_back(torsoChance);
		deathConditions.insert(std::pair<std::string, std::vector<float>>(*it, conditions));
	}

	for (auto it = cartridges.begin(); it != cartridges.end(); ++it) {
		CSimpleIniA::TNamesDepend protectionKeys;
		ini.GetAllKeys(it->c_str(), protectionKeys);
		spdlog::log(spdlog::level::warn, _MESSAGE("%s Protection Chances", it->c_str()));
		std::vector<uint16_t> chances;
		for (auto key = protectionKeys.begin(); key != protectionKeys.end(); ++key) {
			chances.push_back((uint16_t)std::stoi((ini.GetValue(it->c_str(), key->pItem))));
			spdlog::log(spdlog::level::warn, _MESSAGE("%d", std::stoi((ini.GetValue(it->c_str(), key->pItem)))));
		}
		protectionChances.insert(std::pair<std::string, std::vector<uint16_t>>(*it, chances));
	}

	deathmarked = GetAVIFByEditorID("EFD_DeathMark");
	lasthitpart = GetAVIFByEditorID("EFD_LastHitPart");
	perceptionCondition = (ActorValueInfo*)TESForm::GetFormByID(0x00036C);
	enduranceCondition = (ActorValueInfo*)TESForm::GetFormByID(0x00036D);
	brainCondition = (ActorValueInfo*)TESForm::GetFormByID(0x000372);
	spdlog::log(spdlog::level::warn, _MESSAGE("EFD_DeathMark %llx", deathmarked));
	spdlog::log(spdlog::level::warn, _MESSAGE("EFD_LastHitPart %llx", lasthitpart));

	TESDataHandler* dh = TESDataHandler::GetSingleton();
	BSTArray<EffectSetting*> mgefs = dh->GetFormArray<EffectSetting>();
	for (auto it = mgefs.begin(); it != mgefs.end(); ++it) {
		if (strncmp((*it)->GetFullName(), "EFDDeathMark", 12) == 0) {
			deathmarkMGEFs.push_back((*it)->formID);
		}
	}
}

void SetupBleeding() {
	BleedingData bldHead;
	bldHead.spell = GetSpellByFullName(std::string("EFD Condition Bleed Head"));
	bldHead.initial = std::stof(ini.GetValue("BleedingHead", "Initial"));
	bldHead.multiplier = std::stof(ini.GetValue("BleedingHead", "Multiplier"));
	bldHead.duration = std::stof(ini.GetValue("BleedingHead", "Duration"));
	bleedingConfigs.insert(std::pair<EFDBodyParts, BleedingData>(EFDBodyParts::Head, bldHead));
	spdlog::log(spdlog::level::warn, _MESSAGE("_SpellConditionBleedHead %llx", bldHead.spell));

	BleedingData bldTorso;
	bldTorso.spell = GetSpellByFullName(std::string("EFD Condition Bleed Torso"));
	bldTorso.initial = std::stof(ini.GetValue("BleedingTorso", "Initial"));
	bldTorso.multiplier = std::stof(ini.GetValue("BleedingTorso", "Multiplier"));
	bldTorso.duration = std::stof(ini.GetValue("BleedingTorso", "Duration"));
	bleedingConfigs.insert(std::pair<EFDBodyParts, BleedingData>(EFDBodyParts::Torso, bldTorso));
	spdlog::log(spdlog::level::warn, _MESSAGE("_SpellConditionBleedTorso %llx", bldTorso.spell));

	BleedingData bldLArm;
	bldLArm.spell = GetSpellByFullName(std::string("EFD Condition Bleed LArm"));
	bldLArm.initial = std::stof(ini.GetValue("BleedingArm", "Initial"));
	bldLArm.multiplier = std::stof(ini.GetValue("BleedingArm", "Multiplier"));
	bldLArm.duration = std::stof(ini.GetValue("BleedingArm", "Duration"));
	bleedingConfigs.insert(std::pair<EFDBodyParts, BleedingData>(EFDBodyParts::LArm, bldLArm));
	spdlog::log(spdlog::level::warn, _MESSAGE("_SpellConditionBleedLArm %llx", bldLArm.spell));

	BleedingData bldLLeg;
	bldLLeg.spell = GetSpellByFullName(std::string("EFD Condition Bleed LLeg"));
	bldLLeg.initial = std::stof(ini.GetValue("BleedingLeg", "Initial"));
	bldLLeg.multiplier = std::stof(ini.GetValue("BleedingLeg", "Multiplier"));
	bldLLeg.duration = std::stof(ini.GetValue("BleedingLeg", "Duration"));
	bleedingConfigs.insert(std::pair<EFDBodyParts, BleedingData>(EFDBodyParts::LLeg, bldLLeg));
	spdlog::log(spdlog::level::warn, _MESSAGE("_SpellConditionBleedLLeg %llx", bldLLeg.spell));

	BleedingData bldRArm;
	bldRArm.spell = GetSpellByFullName(std::string("EFD Condition Bleed RArm"));
	bldRArm.initial = std::stof(ini.GetValue("BleedingArm", "Initial"));
	bldRArm.multiplier = std::stof(ini.GetValue("BleedingArm", "Multiplier"));
	bldRArm.duration = std::stof(ini.GetValue("BleedingArm", "Duration"));
	bleedingConfigs.insert(std::pair<EFDBodyParts, BleedingData>(EFDBodyParts::RArm, bldRArm));
	spdlog::log(spdlog::level::warn, _MESSAGE("_SpellConditionBleedRArm %llx", bldRArm.spell));

	BleedingData bldRLeg;
	bldRLeg.spell = GetSpellByFullName(std::string("EFD Condition Bleed RLeg"));
	bldRLeg.initial = std::stof(ini.GetValue("BleedingLeg", "Initial"));
	bldRLeg.multiplier = std::stof(ini.GetValue("BleedingLeg", "Multiplier"));
	bldRLeg.duration = std::stof(ini.GetValue("BleedingLeg", "Duration"));
	bleedingConfigs.insert(std::pair<EFDBodyParts, BleedingData>(EFDBodyParts::RLeg, bldRLeg));
	spdlog::log(spdlog::level::warn, _MESSAGE("_SpellConditionBleedRLeg %llx", bldRLeg.spell));
}

#pragma endregion

#pragma region Events

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

//*********************Biped Slots********************
// 30	-	0x1
// 31	-	0x2
// 32	-	0x4
// 33	-	0x8
// 34	-	0x10
// 35	-	0x20
// 36	-	0x40
// 37	-	0x80
// 38	-	0x100
// 39	-	0x200
// 40	-	0x400
// 41	-	0x800
// 42	-	0x1000
// 43	-	0x2000
// 44	-	0x4000
// 45	-	0x8000
//****************************************************

void SetHelmetTier(Actor* a, uint16_t rating) {
	size_t i = helmetRatings.size() - 1;
	bool intervalFound = false;
	while (i >= 0 && !intervalFound) {
		if (rating >= helmetRatings[i]) {
			a->SetActorValue(*helmetTier, i);
			intervalFound = true;
			//spdlog::log(spdlog::level::warn, _MESSAGE("%llx has Tier %d helmet", a, i));
		}
		--i;
	}
	if (!intervalFound)
		a->SetActorValue(*helmetTier, 0);
}

void SetVestTier(Actor* a, uint16_t rating) {
	size_t i = vestRatings.size() - 1;
	bool intervalFound = false;
	while (i >= 0 && !intervalFound) {
		if (rating >= vestRatings[i]) {
			a->SetActorValue(*vestTier, i);
			intervalFound = true;
			//spdlog::log(spdlog::level::warn, _MESSAGE("%llx has Tier %d vest", a, i));
		}
		--i;
	}
	if(!intervalFound)
		a->SetActorValue(*vestTier, 0);
}

class EquipWatcher : public BSTEventSink<TESEquipEvent> {
public:
	virtual BSEventNotifyControl ProcessEvent(const TESEquipEvent& evn, BSTEventSource<TESEquipEvent>* a_source) {
		/*if (evn.flag != 0x00000000ff000000) {
			spdlog::log(spdlog::level::warn, _MESSAGE("Equip item called on %llx (FormID %llx)", evn.a, evn.formId));
		}
		else {
			spdlog::log(spdlog::level::warn, _MESSAGE("Unequip item called on %llx (FormID %llx)", evn.a, evn.formId));
		}*/
		TESForm* item = TESForm::GetFormByID(evn.formId);
		if (item && item->formType == ENUM_FORM_ID::kARMO) {
			TESObjectARMO* armor = static_cast<TESObjectARMO*>(item);
			//TESObjectARMO::InstanceData* data = *(TESObjectARMO::InstanceData**)((uintptr_t)&evn + 0xd0);
			TESObjectARMO::InstanceData* data = &(armor->data);
			if (armor->bipedModelData.bipedObjectSlots & 0x800 || armor->bipedModelData.bipedObjectSlots & 0x40) {		//Vest
				uint16_t vestAR = 0;
				for (auto invitem = evn.a->inventoryList->data.begin(); invitem != evn.a->inventoryList->data.end(); ++invitem) {
					if (invitem->stackData->IsEquipped()) {
						if (invitem->object->formType == ENUM_FORM_ID::kARMO) {
							TESObjectARMO* invarmor = static_cast<TESObjectARMO*>(invitem->object);
							TESObjectARMO::InstanceData* invdata = &(armor->data);
							if (invarmor->bipedModelData.bipedObjectSlots & 0x800 || invarmor->bipedModelData.bipedObjectSlots & 0x40) {		//Vest
								if (evn.flag != 0x00000000ff000000 || (evn.flag == 0x00000000ff000000 && invarmor != armor)) {
									vestAR += data->rating;
									//spdlog::log(spdlog::level::warn, _MESSAGE("Vest found %s (FormID %llx at %llx)", invarmor->fullName.c_str(), invarmor->formID, invarmor));
								}
							}
						}
					}
				}
				//spdlog::log(spdlog::level::warn, _MESSAGE("Vest AR %d", vestAR));
				SetVestTier(evn.a, vestAR);
				if (armor->bipedModelData.bipedObjectSlots & 0x7) {	//Vest has helmet
					uint16_t adjustedRating = data->rating / 4;
					if (evn.flag == 0x00000000ff000000) {
						evn.a->SetActorValue(*helmetTier, 0.0f);
					}
					else {
						SetHelmetTier(evn.a, adjustedRating);
					}
				}
			}
			else if (armor->bipedModelData.bipedObjectSlots & 0x7) {	//Helmet
				if (evn.flag == 0x00000000ff000000) {
					evn.a->SetActorValue(*helmetTier, 0.0f);
				}
				else {
					SetHelmetTier(evn.a, data->rating);
				}
			}
		}
		return BSEventNotifyControl::kContinue;
	}
	F4_HEAP_REDEFINE_NEW(EquipEventSink);
};

class ObjectLoadWatcher : public BSTEventSink<TESObjectLoadedEvent> {
public:
	virtual BSEventNotifyControl ProcessEvent(const TESObjectLoadedEvent& evn, BSTEventSource<TESObjectLoadedEvent>* a_source) {
		if (!evn.loaded) return BSEventNotifyControl::kContinue;
		TESForm* form = TESForm::GetFormByID(evn.formId);
		if (form && form->formType == ENUM_FORM_ID::kACHR) {
			Actor* a = static_cast<Actor*>(form);
			//spdlog::log(spdlog::level::warn, _MESSAGE("Actor FormID %llx at %llx", a->formID, a));
			uint16_t vestAR = 0;
			if (a->inventoryList) {
				for (auto item = a->inventoryList->data.begin(); item != a->inventoryList->data.end(); ++item) {
					if (item->stackData->IsEquipped()) {
						if (item->object->formType == ENUM_FORM_ID::kARMO) {
							TESObjectARMO* armor = static_cast<TESObjectARMO*>(item->object);
							TESObjectARMO::InstanceData* data = &(armor->data);
							if (armor->bipedModelData.bipedObjectSlots & 0x800 || armor->bipedModelData.bipedObjectSlots & 0x40) {		//Vest
								vestAR += data->rating;
								//spdlog::log(spdlog::level::warn, _MESSAGE("Vest found %s (FormID %llx at %llx)", armor->fullName.c_str(), armor->formID, armor));
								if (armor->bipedModelData.bipedObjectSlots & 0x7) {	//Vest has helmet
									uint16_t adjustedRating = data->rating / 4;
									SetHelmetTier(a, adjustedRating);
								}
							}
							else if (armor->bipedModelData.bipedObjectSlots & 0x7) {	//Helmet
								//spdlog::log(spdlog::level::warn, _MESSAGE("Helmet rating %d", data->rating));
								SetHelmetTier(a, data->rating);
							}
						}
					}
				}
				//spdlog::log(spdlog::level::warn, _MESSAGE("Vest AR %d", vestAR));
				SetVestTier(a, vestAR);
			}
		}
		return BSEventNotifyControl::kContinue;
	}
	F4_HEAP_REDEFINE_NEW(ObjectLoadWatcher);
};

std::random_device rd;
class MGEFWatcher : public BSTEventSink<TESMagicEffectApplyEvent> {
public:
	virtual BSEventNotifyControl ProcessEvent(const TESMagicEffectApplyEvent& evn, BSTEventSource<TESMagicEffectApplyEvent>* a_source) {
		EffectSetting* mgef = static_cast<EffectSetting*>(TESForm::GetFormByID(evn.magicEffectFormID));
		if (mgef->data.flags == 0x18008010 && mgef->data.castingType.underlying() == 1 && mgef->data.delivery.underlying() == 1) {
			if (evn.caster != evn.target && evn.target->formType == ENUM_FORM_ID::kACHR) {
				Actor* a = ((Actor*)evn.target.get());
				for (auto deathmark = deathmarkMGEFs.begin(); deathmark != deathmarkMGEFs.end(); ++deathmark) {
					if (evn.magicEffectFormID == *deathmark) {
						uintptr_t offset = 17;
						if (strncmp(mgef->GetFullName() + 12, "Head", 4) == 0) {
							offset = 16;
						}
						bool isTorso = offset - 16;
						if ((isTorso && a->GetActorValue(*lasthitpart) == 2) || (!isTorso && a->GetActorValue(*lasthitpart) == 1)) {
							std::string cartridge(mgef->GetFullName() + offset);
							spdlog::log(spdlog::level::warn, _MESSAGE("Processing DeathMark %s", cartridge.c_str()));
							auto dclookup = deathConditions.find(cartridge);
							if (dclookup != deathConditions.end()) {
								bool hasDeathChance = false;
								if (isTorso) {
									hasDeathChance = dclookup->second[1] >= a->GetActorValue(*enduranceCondition);
									spdlog::log(spdlog::level::warn, _MESSAGE("Torso condition %f current %f", dclookup->second[1], a->GetActorValue(*enduranceCondition)));
								}
								else {
									hasDeathChance = dclookup->second[0] >= a->GetActorValue(*perceptionCondition)
										|| dclookup->second[0] >= a->GetActorValue(*brainCondition);
									spdlog::log(spdlog::level::warn, _MESSAGE("Head condition %f current %f", dclookup->second[0], a->GetActorValue(*perceptionCondition)));
								}
								if (hasDeathChance) {
									auto pclookup = protectionChances.find(cartridge);
									if (pclookup != protectionChances.end()) {
										ActorValueInfo* avif = isTorso ? vestTier : helmetTier;
										uint16_t chance = pclookup->second[(size_t)a->GetActorValue(*avif)];
										std::mt19937 e{ rd() }; // or std::default_random_engine e{rd()};
										std::uniform_int_distribution<uint16_t> dist{ 1, 100 };
										uint16_t result = dist(e);
										spdlog::log(spdlog::level::warn, _MESSAGE("Protection Chance %d result %d", chance, result));
										if (result > chance) {
											killDeathMarked->Cast(a, a);
											if(a == PlayerCharacter::GetSingleton())
												spdlog::log(spdlog::level::warn, _MESSAGE("---Player killed by deathmark---"));
										}
									}
								}
							}
						}
					}
				}
			}
		}
		return BSEventNotifyControl::kContinue;
	}
	F4_HEAP_REDEFINE_NEW(MGEFWatcher);
};

#pragma endregion

#pragma region Event Sources

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

#pragma endregion

#pragma region Projectile Hook

using std::unordered_map;
class HookProjectile : public Projectile {
public:
	typedef bool (HookProjectile::* FnProcessImpacts)();

	bool CheckPlayerHitBoneHardcoded() {
		Projectile::ImpactData& ipct = this->impacts[0];
		if (this->impacts.size() > 0 && !ipct.processed && ipct.collidee.get() && ipct.collidee.get().get()->formType == ENUM_FORM_ID::kACHR && ipct.colObj) {
			Actor* a = static_cast<Actor*>(ipct.collidee.get().get());
			NiAVObject* parent = ipct.colObj.get()->sceneObject;
			if (parent) {
				if(a == PlayerCharacter::GetSingleton())
					spdlog::log(spdlog::level::warn, _MESSAGE("Player got hit on %s", parent->name.c_str()));
				else
					spdlog::log(spdlog::level::warn, _MESSAGE("%llx got hit on %s", a, parent->name.c_str()));
				int partType = -1;
				for (int i = 0; i < 26; ++i) {
					BGSBodyPart* part = a->race->bodyPartData->partArray[i];
					if (part && (parent->name == part->PartNode || parent->name == part->VATSTarget)) {
						partType = i;
					}
				}
				if (partType == -1) {
					if (parent->name == std::string_view("LArm_Hand")) {
						partType = 6;
					}
					else if (parent->name == std::string_view("RArm_Hand")) {
						partType = 8;
					}
					else {
						partType = 0;
					}
				}
				TESAmmo* ammo = this->ammoSource;
				if (ammo) {
					spdlog::log(spdlog::level::warn, _MESSAGE("Ammo : %s (FormID %llx) - Projectile FormID %llx",
															  this->ammoSource->fullName.c_str(), this->ammoSource->formID,
															  this->ammoSource->data.projectile->formID));
				}
				else if (this->weaponSource.instanceData && this->weaponSource.object->formType == ENUM_FORM_ID::kWEAP) {
					ammo = static_cast<TESObjectWEAP::InstanceData*>(this->weaponSource.instanceData.get())->ammo;
					spdlog::log(spdlog::level::warn, _MESSAGE("Ammo : %s (FormID %llx) - Projectile FormID %llx",
															  ammo->fullName.c_str(), ammo->formID,
															  ammo->data.projectile->formID));
				}
				int partFound = 0;
				for (auto torso = torsoParts.begin(); torso != torsoParts.end(); ++torso) {
					if (partType == *torso) {
						a->SetActorValue(*lasthitpart, 2);
						partFound = 2;
						spdlog::log(spdlog::level::warn, _MESSAGE("Node : %s is Torso", parent->name.c_str()));
					}
				}
				for (auto head = headParts.begin(); head != headParts.end(); ++head) {
					if (partType == *head) {
						a->SetActorValue(*lasthitpart, 1);
						partFound = 1;
						spdlog::log(spdlog::level::warn, _MESSAGE("Node : %s is Head", parent->name.c_str()));
					}
				}
				if (!partFound) {
					a->SetActorValue(*lasthitpart, 0);
				}
			}
		}
		FnProcessImpacts fn = fnHash.at(*(uint64_t*)this);
		return fn ? (this->*fn)() : false;
	}

	static void HookProcessImpacts(uint64_t addr, uint64_t offset) {
		FnProcessImpacts fn = SafeWrite64Function(addr + offset, &HookProjectile::CheckPlayerHitBoneHardcoded);
		fnHash.insert(std::pair<uint64_t, FnProcessImpacts>(addr, fn));
	}
protected:
	static unordered_map<uint64_t, FnProcessImpacts> fnHash;
};
unordered_map<uint64_t, HookProjectile::FnProcessImpacts> HookProjectile::fnHash;

void SetupProjectile() {
	uint64_t addr;
	uint64_t offset = 0x680;
	addr = MissileProjectile::VTABLE[0].address();
	spdlog::log(spdlog::level::warn, _MESSAGE("Patching MissileProjectile %llx", addr));
	HookProjectile::HookProcessImpacts(addr, offset);

	addr = BeamProjectile::VTABLE[0].address();
	spdlog::log(spdlog::level::warn, _MESSAGE("Patching BeamProjectile %llx", addr));
	HookProjectile::HookProcessImpacts(addr, offset);

	headHitMark = GetSpellByFullName(std::string("EFD Head HitMark"));
	headHitMarkMGEF = headHitMark->listOfEffects[0]->effectSetting;
	torsoHitMark = GetSpellByFullName(std::string("EFD Torso HitMark"));
	torsoHitMarkMGEF = torsoHitMark->listOfEffects[0]->effectSetting;
	killDeathMarked = GetSpellByFullName(std::string("EFD Kill DeathMarked"));
	spdlog::log(spdlog::level::warn, _MESSAGE("EFD Head HitMark %llx", headHitMark));
	spdlog::log(spdlog::level::warn, _MESSAGE("EFD Torso HitMark %llx", torsoHitMark));
	spdlog::log(spdlog::level::warn, _MESSAGE("EFD Kill DeathMarked %llx", killDeathMarked));
}

#pragma endregion

#pragma region F4SE Initialize

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Query(const F4SE::QueryInterface * a_f4se, F4SE::PluginInfo * a_info) {
#ifndef NDEBUG
	auto sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
#else
	auto path = logger::log_directory();
	if (!path) {
		return false;
	}

	*path /= "DeBadWeapons.log"sv;
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
	a_info->name = "DeBadWeapons";
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

	//ini 파일 읽기
	ini.LoadFile("Data\\F4SE\\Plugins\\DeBadWeapons.ini");

	//F4SE에서 제공하는 메세지 인터페이스
	const F4SE::MessagingInterface* message = F4SE::GetMessagingInterface();
	message->RegisterListener([](F4SE::MessagingInterface::Message* msg) -> void {
		//게임 데이터가 모두 로드된 시점 (메인화면이 나오기 직전)
		if (msg->type == F4SE::MessagingInterface::kGameDataReady) {
			spdlog::log(spdlog::level::warn, _MESSAGE("PlayerCharacter %llx", PlayerCharacter::GetSingleton()));
			SetupWeapons();
			SetupArmors();
			SetupDeathMark();
			SetupBleeding();
			SetupProjectile();
			EquipWatcher* ew = new EquipWatcher();
			EquipEventSource::GetSingleton()->RegisterSink(ew);

			ObjectLoadWatcher* olw = new ObjectLoadWatcher();
			ObjectLoadedEventSource::GetSingleton()->RegisterSink(olw);

			MGEFWatcher* mw = new MGEFWatcher();
			MGEFApplyEventSource::GetSingleton()->RegisterSink(mw);
		}
	});

	return true;
}

#pragma endregion
