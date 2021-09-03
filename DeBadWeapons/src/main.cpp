#include "SimpleIni.h"
#include <random>
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

#pragma endregion

#pragma region GlobalVariables

//ini 정의, UTF-8 사용, 멀티키 X, 멀티라인 X 설정
CSimpleIniA ini(true, false, false);
bool playerForceKill = false;
std::unordered_map<TESAmmo*, bool> ammoWhitelist;
std::vector<BGSProjectile*> EFDProjectiles;
size_t maxTier;
std::vector<uint16_t> helmetRatings;
std::vector<uint16_t> vestRatings;
struct CartridgeData {
	std::vector<uint16_t> protectionChances;
	std::vector<float> deathConditions;
};
std::unordered_map<std::string, CartridgeData> cartridgeData;
std::vector<uint32_t> deathmarkMGEFs;
ActorValueInfo* helmetTier;
ActorValueInfo* vestTier;
ActorValueInfo* deathmarked;
ActorValueInfo* lasthitpart;
ActorValueInfo* perceptionCondition;
ActorValueInfo* enduranceCondition;
ActorValueInfo* leftAttackCondition;
ActorValueInfo* rightAttackCondition;
ActorValueInfo* leftMobilityCondition;
ActorValueInfo* rightMobilityCondition;
ActorValueInfo* brainCondition;
ActorValueInfo* health;
EffectSetting* headHitMarkMGEF;
EffectSetting* torsoHitMarkMGEF;
SpellItem* headHitMark;
SpellItem* torsoHitMark;
SpellItem* EFDDeathMarkGlobal;
SpellItem* killDeathMarked;
std::vector<BGSBodyPartData::PartType> torsoParts = { BGSBodyPartData::PartType::COM, BGSBodyPartData::PartType::Torso, BGSBodyPartData::PartType::Pelvis };
std::vector<BGSBodyPartData::PartType> headParts = { BGSBodyPartData::PartType::Eye, BGSBodyPartData::PartType::Head1, BGSBodyPartData::PartType::Head2, BGSBodyPartData::PartType::Brain };
std::vector<BGSBodyPartData::PartType> larmParts = { BGSBodyPartData::PartType::LeftArm1 };
std::vector<BGSBodyPartData::PartType> llegParts = { BGSBodyPartData::PartType::LeftLeg1, BGSBodyPartData::PartType::LeftFoot };
std::vector<BGSBodyPartData::PartType> rarmParts = { BGSBodyPartData::PartType::RightArm1, BGSBodyPartData::PartType::Weapon };
std::vector<BGSBodyPartData::PartType> rlegParts = { BGSBodyPartData::PartType::RightLeg1, BGSBodyPartData::PartType::RightFoot };

enum EFDBodyParts {
	None,
	Head,
	Torso,
	LArm,
	LLeg,
	RArm,
	RLeg
};

std::string EFDBodyPartsName[] = {
	"None",
	"Head",
	"Torso",
	"LArm",
	"LLeg",
	"RArm",
	"RLeg"
};

ActorValueInfo** EFDConditions[] = {
	nullptr,
	&perceptionCondition,
	&enduranceCondition,
	&leftAttackCondition,
	&leftMobilityCondition,
	&rightAttackCondition,
	&rightMobilityCondition
};

struct BleedingData {
	SpellItem* spell;
	float conditionStart;
	float conditionThreshold;
	float chanceMin;
	float chanceMax;
	float initialDamage;
	float multiplier;
	float duration;
	void PrintData() {
		_MESSAGE("Condition Start %f", conditionStart);
		_MESSAGE("Condition Threshold %f", conditionThreshold);
		_MESSAGE("Chance Min %f", chanceMin);
		_MESSAGE("Chance Max %f", chanceMax);
		_MESSAGE("Initial Damage %f", initialDamage);
		_MESSAGE("Multiplier %f", multiplier);
		_MESSAGE("Duration %f", duration);
	}
};
std::unordered_map<EFDBodyParts, BleedingData> bleedingConfigs;
REL::Relocation<uint64_t*> ptr_engineTime{ REL::ID(1280610) };

BGSExplosion* globalExplosion;
struct GlobalData {
	float armorDamageInitial;
	float armorDamageMultiplier;
	float headFatality;
	float torsoFatality;
	float formulaA;
	float formulaB;
	float formulaC;
	float formulaD;
	void PrintData() {
		_MESSAGE("ArmorDamageInitial %f", armorDamageInitial);
		_MESSAGE("ArmorDamageMultiplier %f", armorDamageMultiplier);
		_MESSAGE("HeadFatality %f", headFatality);
		_MESSAGE("TorsoFatality %f", torsoFatality);
		_MESSAGE("FormulaA %f", formulaA);
		_MESSAGE("FormulaB %f", formulaB);
		_MESSAGE("FormulaC %f", formulaC);
		_MESSAGE("FormulaC %f", formulaD);
	}
};
GlobalData gd;

#pragma endregion

#pragma region Data Initial Setup
//탄약 화이트리스트 함수
std::unordered_map<TESAmmo*, bool> GetAmmunitionWhitelist() {
	std::unordered_map<TESAmmo*, bool> ret;

	//게임에 로드된 폼 중 탄약만 가져온다
	TESDataHandler* dh = TESDataHandler::GetSingleton();
	BSTArray<TESAmmo*> ammos = dh->GetFormArray<TESAmmo>();

	globalExplosion = GetExplosionByFullName("EFD Global Explosion");
	_MESSAGE("EFD Global Explosion %llx", globalExplosion);
	
	//모든 탄약 폼에 대하여
	for (auto it = ammos.begin(); it != ammos.end(); ++it) {
		TESAmmo* ammo = *it;
		if (ammo->data.damage > 0 && ammo->data.projectile && ammo->data.projectile->data.flags & 0x10000) {
			/*BGSProjectile* proj = ammo->data.projectile;
			if (proj->data.flags != 0x10a0a) {
				proj->data.flags = 0x10a0a;
				proj->data.explosionType = globalExplosion;
				_MESSAGE("Patching projectile of %s...", ammo->fullName.c_str());
			}
			BGSProjectile* vatsproj = proj->data.vatsProjectile;
			if (vatsproj && vatsproj->data.flags != 0x10a0a) {
				vatsproj->data.flags = 0x10a0a;
				vatsproj->data.explosionType = globalExplosion;
				_MESSAGE("Patching VATS projectile of %s...", ammo->fullName.c_str());
			}*/
			_MESSAGE("%s is DeBadFall certified.", ammo->fullName.c_str());
			ret.insert(std::pair<TESAmmo*, bool>(ammo, true));
		}
	}
	return ret;
}

void SetupWeapons() {
	uint16_t uniqueDmg = (uint16_t)std::stoi(ini.GetValue("General", "UniqueDamage", "15"));
	_MESSAGE("UniqueDamage %d", uniqueDmg);

	//탄약 화이트리스트 제작
	ammoWhitelist = GetAmmunitionWhitelist();

	//바닐라에서 총과 유니크 무기의 정의를 가져옴
	BGSListForm* gunsKeywordList = (BGSListForm*)TESForm::GetFormByID(0xF78EC);
	BGSKeyword* uniqueKeyword = (BGSKeyword*)TESForm::GetFormByID(0x1B3FAC);

	_MESSAGE("Keyword list found.");
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
						_MESSAGE("Weapon Found : %s (FormID %llx at %llx)", wep->fullName.c_str(), wep->formID, wep);
						uint16_t oldDamage = wep->weaponData.attackDamage;
						//유니크 확인
						bool isUnique = false;
						for (uint32_t j = 0; j < wep->numKeywords; ++j) {
							if (wep->keywords[i] == uniqueKeyword) {
								isUnique = true;
							}
						}
						wep->weaponData.attackDamage = isUnique * uniqueDmg;
						_MESSAGE("Old dmg %d, New dmg 0", oldDamage);
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
	_MESSAGE("Helmet Tier References");
	for (auto key = helmetKeys.begin(); key != helmetKeys.end(); ++key) {
		uint16_t ref = (uint16_t)std::stoi(ini.GetValue("Helmet", key->pItem));
		helmetRatings.push_back(ref);
		_MESSAGE("%d", ref);
	}

	CSimpleIniA::TNamesDepend vestKeys;
	ini.GetAllKeys("Vest", vestKeys);
	_MESSAGE("Vest Tier References");
	for (auto key = vestKeys.begin(); key != vestKeys.end(); ++key) {
		uint16_t ref = (uint16_t)std::stoi(ini.GetValue("Vest", key->pItem));
		vestRatings.push_back(ref);
		_MESSAGE("%d", ref);
	}
	maxTier = max(helmetRatings.size(), vestRatings.size());
	_MESSAGE("Tier max : %d", maxTier);

	helmetTier = GetAVIFByEditorID("EFD_Helmet_Tier");
	vestTier = GetAVIFByEditorID("EFD_Vest_Tier");
	_MESSAGE("EFD_Helmet_Tier %llx", helmetTier);
	_MESSAGE("EFD_Vest_Tier %llx", vestTier);

	/*TESDataHandler* dh = TESDataHandler::GetSingleton();
	BSTArray<TESObjectARMO*> armors = dh->GetFormArray<TESObjectARMO>();
	for (auto it = armors.begin(); it != armors.end(); ++it) {
		TESObjectARMO* armor = (*it);
		if (armor->bipedModelData.bipedObjectSlots & 0x800 || armor->bipedModelData.bipedObjectSlots & 0x40) {		//Vest
			_MESSAGE("Vest\t\t%s\t\t\tDR %d\t\t(FormID %llx at %llx)", armor->fullName.c_str(), armor->data.rating, armor->formID, armor);
		}
		else if (armor->bipedModelData.bipedObjectSlots & 0x7) {	//Helmet
			_MESSAGE("Helmet\t%s\t\t\t\tDR %d\t\t(FormID %llx at %llx)", armor->fullName.c_str(), armor->data.rating, armor->formID, armor);
		}
	}*/
}

void SetupDeathMark() {
	/*CSimpleIniA::TNamesDepend cartridgeKeys;
	ini.GetAllKeys("Cartridges", cartridgeKeys);
	_MESSAGE("Cartridges");
	std::vector<std::string> cartridges;
	for (auto key = cartridgeKeys.begin(); key != cartridgeKeys.end(); ++key) {
		cartridges.push_back(ini.GetValue("Cartridges", key->pItem));
		_MESSAGE("%s", ini.GetValue("Cartridges", key->pItem));
	}

	for (auto it = cartridges.begin(); it != cartridges.end(); ++it) {
		_MESSAGE("%s Death Conditions", it->c_str());
		std::vector<float> conditions;
		float headChance = std::stof(ini.GetValue("CartridgeFatalities", (*it + "Head").c_str()));
		_MESSAGE("Head %f", headChance);
		float torsoChance = std::stof(ini.GetValue("CartridgeFatalities", (*it + "Torso").c_str()));
		_MESSAGE("Torso %f", torsoChance);
		conditions.push_back(headChance);
		conditions.push_back(torsoChance);

		CSimpleIniA::TNamesDepend protectionKeys;
		ini.GetAllKeys(it->c_str(), protectionKeys);
		_MESSAGE("%s Protection Chances", it->c_str());
		std::vector<uint16_t> chances;
		for (auto key = protectionKeys.begin(); key != protectionKeys.end(); ++key) {
			chances.push_back((uint16_t)std::stoi((ini.GetValue(it->c_str(), key->pItem))));
			_MESSAGE("%d", std::stoi((ini.GetValue(it->c_str(), key->pItem))));
		}
		if (chances.size() < maxTier) {
			_MESSAGE("Cartridge %s is missing some data! Auto filling...", it->c_str());
			if (chances.size() == 0) {
				for (int i = 0; i < maxTier; ++i) {
					chances.push_back(0);
				}
			}
			else {
				for (int i = 0; i < maxTier - chances.size(); ++i) {
					chances.push_back(chances[chances.size() - 1]);
					_MESSAGE("%d", chances[chances.size() - 1]);
				}
			}
		}
		CartridgeData cd;
		cd.deathConditions = conditions;
		cd.protectionChances = chances;
		cartridgeData.insert(std::pair<std::string, CartridgeData>(*it, cd));
	}*/
	playerForceKill = std::stoi(ini.GetValue("General", "PlayerForceKill")) > 0;
	_MESSAGE("PlayerForceKill %d", playerForceKill);
	_MESSAGE("Laser");
	std::vector<float> laserConditions;
	float headChance = std::stof(ini.GetValue("CartridgeFatalities", "LaserHead"));
	_MESSAGE("Head %f", headChance);
	float torsoChance = std::stof(ini.GetValue("CartridgeFatalities", "LaserTorso"));
	_MESSAGE("Torso %f", torsoChance);
	laserConditions.push_back(headChance);
	laserConditions.push_back(torsoChance);
	CSimpleIniA::TNamesDepend protectionKeys;
	ini.GetAllKeys("Laser", protectionKeys);
	std::vector<uint16_t> laserChances;
	for (auto key = protectionKeys.begin(); key != protectionKeys.end(); ++key) {
		laserChances.push_back((uint16_t)std::stoi((ini.GetValue("Laser", key->pItem))));
		_MESSAGE("%d", std::stoi((ini.GetValue("Laser", key->pItem))));
	}
	if (laserChances.size() < maxTier) {
		_MESSAGE("Cartridge %s is missing some data! Auto filling...", "Laser");
		if (laserChances.size() == 0) {
			for (int i = 0; i < maxTier; ++i) {
				laserChances.push_back(0);
			}
		}
		else {
			for (int i = 0; i < maxTier - laserChances.size(); ++i) {
				laserChances.push_back(laserChances[laserChances.size() - 1]);
				_MESSAGE("%d", laserChances[laserChances.size() - 1]);
			}
		}
	}
	CartridgeData laserCd;
	laserCd.deathConditions = laserConditions;
	laserCd.protectionChances = laserChances;
	cartridgeData.insert(std::pair<std::string, CartridgeData>(std::string("Laser"), laserCd));


	_MESSAGE("Global");
	gd.armorDamageInitial = std::stof(ini.GetValue("Global", "ArmorDamageInitial"));
	gd.armorDamageMultiplier = std::stof(ini.GetValue("Global", "ArmorDamageMultiplier"));
	gd.headFatality = std::stof(ini.GetValue("Global", "HeadFatality"));
	gd.torsoFatality = std::stof(ini.GetValue("Global", "TorsoFatality"));
	gd.formulaA = std::stof(ini.GetValue("Global", "FormulaA"));
	gd.formulaB = std::stof(ini.GetValue("Global", "FormulaB"));
	gd.formulaC = std::stof(ini.GetValue("Global", "FormulaC"));
	gd.formulaD = std::stof(ini.GetValue("Global", "FormulaD"));
	gd.PrintData();

	std::vector<float> globalConditions;
	globalConditions.push_back(gd.headFatality);
	globalConditions.push_back(gd.torsoFatality);
	std::vector<uint16_t> globalChances;
	for (int i = 0; i < maxTier; ++i) {
		globalChances.push_back(0);
	}
	CartridgeData globalCd;
	globalCd.deathConditions = globalConditions;
	globalCd.protectionChances = globalChances;
	cartridgeData.insert(std::pair<std::string, CartridgeData>(std::string("Global"), globalCd));


	deathmarked = GetAVIFByEditorID("EFD_DeathMark");
	lasthitpart = GetAVIFByEditorID("EFD_LastHitPart");
	health = (ActorValueInfo*)TESForm::GetFormByID(0x0002D4);
	perceptionCondition = (ActorValueInfo*)TESForm::GetFormByID(0x00036C);
	enduranceCondition = (ActorValueInfo*)TESForm::GetFormByID(0x00036D);
	leftAttackCondition = (ActorValueInfo*)TESForm::GetFormByID(0x00036E);
	rightAttackCondition = (ActorValueInfo*)TESForm::GetFormByID(0x00036F);
	leftMobilityCondition = (ActorValueInfo*)TESForm::GetFormByID(0x000370);
	rightMobilityCondition = (ActorValueInfo*)TESForm::GetFormByID(0x000371);
	brainCondition = (ActorValueInfo*)TESForm::GetFormByID(0x000372);
	_MESSAGE("EFD_DeathMark %llx", deathmarked);
	_MESSAGE("EFD_LastHitPart %llx", lasthitpart);

	/*TESDataHandler* dh = TESDataHandler::GetSingleton();
	BSTArray<EffectSetting*> mgefs = dh->GetFormArray<EffectSetting>();
	for (auto it = mgefs.begin(); it != mgefs.end(); ++it) {
		if (strncmp((*it)->GetFullName(), "EFDDeathMark", 12) == 0) {
			deathmarkMGEFs.push_back((*it)->formID);
		}
	}*/

	EFDDeathMarkGlobal = GetSpellByFullName("EFD DeathMark Global");
	_MESSAGE("EFD DeathMark Global %llx", EFDDeathMarkGlobal);
	for (auto it = EFDDeathMarkGlobal->listOfEffects.begin(); it != EFDDeathMarkGlobal->listOfEffects.end(); ++it) {
		if (strncmp((*it)->effectSetting->GetFullName(), "EFDDeathMark", 12) == 0) {
			deathmarkMGEFs.push_back((*it)->effectSetting->formID);
		}
	}
	SpellItem* EFDDeathMarkLaser = GetSpellByFullName("EFD DeathMark Laser");
	_MESSAGE("EFD DeathMark Laser %llx", EFDDeathMarkLaser);
	for (auto it = EFDDeathMarkLaser->listOfEffects.begin(); it != EFDDeathMarkLaser->listOfEffects.end(); ++it) {
		if (strncmp((*it)->effectSetting->GetFullName(), "EFDDeathMark", 12) == 0) {
			deathmarkMGEFs.push_back((*it)->effectSetting->formID);
		}
	}
}

void SetupBleeding() {
	BleedingData bldHead;
	bldHead.spell = GetSpellByFullName(std::string("EFD Condition Bleed Head"));
	bldHead.conditionStart = std::stof(ini.GetValue("BleedingHead", "ConditionStart"));
	bldHead.conditionThreshold = std::stof(ini.GetValue("BleedingHead", "ConditionThreshold"));
	bldHead.chanceMin = std::stof(ini.GetValue("BleedingHead", "ChanceMin"));
	bldHead.chanceMax = std::stof(ini.GetValue("BleedingHead", "ChanceMax"));
	bldHead.initialDamage = std::stof(ini.GetValue("BleedingHead", "InitialDamage"));
	bldHead.multiplier = std::stof(ini.GetValue("BleedingHead", "Multiplier"));
	bldHead.duration = std::stof(ini.GetValue("BleedingHead", "Duration"));
	for (auto it = bldHead.spell->listOfEffects.begin(); it != bldHead.spell->listOfEffects.end(); ++it) {
		(*it)->data.magnitude = bldHead.initialDamage;
		(*it)->data.duration = (int32_t)floor(bldHead.duration);
	}
	bleedingConfigs.insert(std::pair<EFDBodyParts, BleedingData>(EFDBodyParts::Head, bldHead));
	_MESSAGE("_SpellConditionBleedHead %llx", bldHead.spell);
	bldHead.PrintData();

	BleedingData bldTorso;
	bldTorso.spell = GetSpellByFullName(std::string("EFD Condition Bleed Torso"));
	bldTorso.conditionThreshold = std::stof(ini.GetValue("BleedingTorso", "ConditionThreshold"));
	bldTorso.chanceMin = std::stof(ini.GetValue("BleedingTorso", "ChanceMin"));
	bldTorso.chanceMax = std::stof(ini.GetValue("BleedingTorso", "ChanceMax"));
	bldTorso.initialDamage = std::stof(ini.GetValue("BleedingTorso", "InitialDamage"));
	bldTorso.multiplier = std::stof(ini.GetValue("BleedingTorso", "Multiplier"));
	bldTorso.duration = std::stof(ini.GetValue("BleedingTorso", "Duration"));
	for (auto it = bldTorso.spell->listOfEffects.begin(); it != bldTorso.spell->listOfEffects.end(); ++it) {
		(*it)->data.magnitude = bldTorso.initialDamage;
		(*it)->data.duration = (int32_t)floor(bldTorso.duration);
	}
	bleedingConfigs.insert(std::pair<EFDBodyParts, BleedingData>(EFDBodyParts::Torso, bldTorso));
	_MESSAGE("_SpellConditionBleedTorso %llx", bldTorso.spell);
	bldTorso.PrintData();

	BleedingData bldLArm;
	bldLArm.spell = GetSpellByFullName(std::string("EFD Condition Bleed LArm"));
	bldLArm.conditionThreshold = std::stof(ini.GetValue("BleedingArm", "ConditionThreshold"));
	bldLArm.chanceMin = std::stof(ini.GetValue("BleedingArm", "ChanceMin"));
	bldLArm.chanceMax = std::stof(ini.GetValue("BleedingArm", "ChanceMax"));
	bldLArm.initialDamage = std::stof(ini.GetValue("BleedingArm", "InitialDamage"));
	bldLArm.multiplier = std::stof(ini.GetValue("BleedingArm", "Multiplier"));
	bldLArm.duration = std::stof(ini.GetValue("BleedingArm", "Duration"));
	for (auto it = bldLArm.spell->listOfEffects.begin(); it != bldLArm.spell->listOfEffects.end(); ++it) {
		(*it)->data.magnitude = bldLArm.initialDamage;
		(*it)->data.duration = (int32_t)floor(bldLArm.duration);
	}
	bleedingConfigs.insert(std::pair<EFDBodyParts, BleedingData>(EFDBodyParts::LArm, bldLArm));
	_MESSAGE("_SpellConditionBleedLArm %llx", bldLArm.spell);
	bldLArm.PrintData();

	BleedingData bldLLeg;
	bldLLeg.spell = GetSpellByFullName(std::string("EFD Condition Bleed LLeg"));
	bldLLeg.conditionThreshold = std::stof(ini.GetValue("BleedingLeg", "ConditionThreshold"));
	bldLLeg.chanceMin = std::stof(ini.GetValue("BleedingLeg", "ChanceMin"));
	bldLLeg.chanceMax = std::stof(ini.GetValue("BleedingLeg", "ChanceMax"));
	bldLLeg.initialDamage = std::stof(ini.GetValue("BleedingLeg", "InitialDamage"));
	bldLLeg.multiplier = std::stof(ini.GetValue("BleedingLeg", "Multiplier"));
	bldLLeg.duration = std::stof(ini.GetValue("BleedingLeg", "Duration"));
	for (auto it = bldLLeg.spell->listOfEffects.begin(); it != bldLLeg.spell->listOfEffects.end(); ++it) {
		(*it)->data.magnitude = bldLLeg.initialDamage;
		(*it)->data.duration = (int32_t)floor(bldLLeg.duration);
	}
	bleedingConfigs.insert(std::pair<EFDBodyParts, BleedingData>(EFDBodyParts::LLeg, bldLLeg));
	_MESSAGE("_SpellConditionBleedLLeg %llx", bldLLeg.spell);
	bldLLeg.PrintData();

	BleedingData bldRArm;
	bldRArm.spell = GetSpellByFullName(std::string("EFD Condition Bleed RArm"));
	bldRArm.conditionThreshold = std::stof(ini.GetValue("BleedingArm", "ConditionThreshold"));
	bldRArm.chanceMin = std::stof(ini.GetValue("BleedingArm", "ChanceMin"));
	bldRArm.chanceMax = std::stof(ini.GetValue("BleedingArm", "ChanceMax"));
	bldRArm.initialDamage = std::stof(ini.GetValue("BleedingArm", "InitialDamage"));
	bldRArm.multiplier = std::stof(ini.GetValue("BleedingArm", "Multiplier"));
	bldRArm.duration = std::stof(ini.GetValue("BleedingArm", "Duration"));
	for (auto it = bldRArm.spell->listOfEffects.begin(); it != bldRArm.spell->listOfEffects.end(); ++it) {
		(*it)->data.magnitude = bldRArm.initialDamage;
		(*it)->data.duration = (int32_t)floor(bldRArm.duration);
	}
	bleedingConfigs.insert(std::pair<EFDBodyParts, BleedingData>(EFDBodyParts::RArm, bldRArm));
	_MESSAGE("_SpellConditionBleedRArm %llx", bldRArm.spell);
	bldRArm.PrintData();

	BleedingData bldRLeg;
	bldRLeg.spell = GetSpellByFullName(std::string("EFD Condition Bleed RLeg"));
	bldRLeg.conditionThreshold = std::stof(ini.GetValue("BleedingLeg", "ConditionThreshold"));
	bldRLeg.chanceMin = std::stof(ini.GetValue("BleedingLeg", "ChanceMin"));
	bldRLeg.chanceMax = std::stof(ini.GetValue("BleedingLeg", "ChanceMax"));
	bldRLeg.initialDamage = std::stof(ini.GetValue("BleedingLeg", "InitialDamage"));
	bldRLeg.multiplier = std::stof(ini.GetValue("BleedingLeg", "Multiplier"));
	bldRLeg.duration = std::stof(ini.GetValue("BleedingLeg", "Duration"));
	for (auto it = bldRLeg.spell->listOfEffects.begin(); it != bldRLeg.spell->listOfEffects.end(); ++it) {
		(*it)->data.magnitude = bldRLeg.initialDamage;
		(*it)->data.duration = (int32_t)floor(bldRLeg.duration);
	}
	bleedingConfigs.insert(std::pair<EFDBodyParts, BleedingData>(EFDBodyParts::RLeg, bldRLeg));
	_MESSAGE("_SpellConditionBleedRLeg %llx", bldRLeg.spell);
	bldRLeg.PrintData();
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
		}
		--i;
	}
	if (!intervalFound)
		a->SetActorValue(*helmetTier, 0);
	//_MESSAGE("Helmet rating %d tier %f", rating, a->GetActorValue(*helmetTier));
}

void SetVestTier(Actor* a, uint16_t rating) {
	size_t i = vestRatings.size() - 1;
	bool intervalFound = false;
	while (i >= 0 && !intervalFound) {
		if (rating >= vestRatings[i]) {
			a->SetActorValue(*vestTier, i);
			intervalFound = true;
		}
		--i;
	}
	if(!intervalFound)
		a->SetActorValue(*vestTier, 0);
	//_MESSAGE("Vest rating %d tier %f", rating, a->GetActorValue(*vestTier));
}

void CalculateArmorTiers(Actor* a) {
	uint16_t vestAR = 0;
	uint16_t helmetAR = 0;
	if (!a->inventoryList) {
		return;
	}
	for (auto invitem = a->inventoryList->data.begin(); invitem != a->inventoryList->data.end(); ++invitem) {
		if (invitem->object->formType == ENUM_FORM_ID::kARMO) {
			TESObjectARMO* invarmor = static_cast<TESObjectARMO*>(invitem->object);
			TESObjectARMO::InstanceData* invdata = &(invarmor->data);
			if (invitem->stackData->IsEquipped()) {
				if (invarmor->bipedModelData.bipedObjectSlots & 0x800 || invarmor->bipedModelData.bipedObjectSlots & 0x40) {		//Vest
					ExtraInstanceData* extraInstanceData = (ExtraInstanceData*)invitem->stackData->extra->GetByType(EXTRA_DATA_TYPE::kInstanceData);
					uint16_t currentVestAR = 0;
					if (extraInstanceData) {
						currentVestAR = ((TESObjectARMO::InstanceData*)extraInstanceData->data.get())->rating;
					}
					else {
						currentVestAR = invdata->rating;
					}
					if (invarmor->bipedModelData.bipedObjectSlots & 0x7) {	//Vest has helmet
						helmetAR = currentVestAR / 4;
					}
					vestAR += currentVestAR;
				}
				else if (invarmor->bipedModelData.bipedObjectSlots & 0x1) {	//Helmet exclusive
					ExtraInstanceData* extraInstanceData = (ExtraInstanceData*)invitem->stackData->extra->GetByType(EXTRA_DATA_TYPE::kInstanceData);
					if (extraInstanceData) {
						helmetAR = ((TESObjectARMO::InstanceData*)extraInstanceData->data.get())->rating;
					}
					else {
						helmetAR = invdata->rating;
					}
				}
			}
		}
	}
	SetVestTier(a, vestAR);
	SetHelmetTier(a, helmetAR);
}

class EquipWatcher : public BSTEventSink<TESEquipEvent> {
public:
	virtual BSEventNotifyControl ProcessEvent(const TESEquipEvent& evn, BSTEventSource<TESEquipEvent>* a_source) {
		TESForm* item = TESForm::GetFormByID(evn.formId);
		if (item && item->formType == ENUM_FORM_ID::kARMO) {
			CalculateArmorTiers(evn.a);
		}
		return BSEventNotifyControl::kContinue;
	}
	F4_HEAP_REDEFINE_NEW(EquipEventSink);
};

class ObjectLoadWatcher : public BSTEventSink<TESObjectLoadedEvent> {
public:
	virtual BSEventNotifyControl ProcessEvent(const TESObjectLoadedEvent& evn, BSTEventSource<TESObjectLoadedEvent>* a_source) {
		if (!evn.loaded) {
			return BSEventNotifyControl::kContinue;
		}
		TESForm* form = TESForm::GetFormByID(evn.formId);
		if (form && form->formType == ENUM_FORM_ID::kACHR) {
			Actor* a = static_cast<Actor*>(form);
			CalculateArmorTiers(a);
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
			if (evn.target->formType == ENUM_FORM_ID::kACHR) {
				Actor* a = ((Actor*)evn.target.get());
				if (a->GetActorValue(*lasthitpart) <= 2) {
					for (auto deathmark = deathmarkMGEFs.begin(); deathmark != deathmarkMGEFs.end(); ++deathmark) {
						if (evn.magicEffectFormID == *deathmark) {
							uintptr_t offset = 17;
							if (strncmp(mgef->GetFullName() + 12, "Head", 4) == 0) {
								offset = 16;
							}
							bool isTorso = offset - 16;
							if ((isTorso && a->GetActorValue(*lasthitpart) == 2) || (!isTorso && a->GetActorValue(*lasthitpart) == 1)) {
								std::string cartridge(mgef->GetFullName() + offset);
								_MESSAGE("Processing DeathMark %s", cartridge.c_str());
								auto cdlookup = cartridgeData.find(cartridge);
								if (cdlookup != cartridgeData.end()) {
									bool hasDeathChance = false;
									if (isTorso) {
										hasDeathChance = cdlookup->second.deathConditions[1] >= a->GetActorValue(*enduranceCondition);
										_MESSAGE("Torso condition %f current %f", cdlookup->second.deathConditions[1], a->GetActorValue(*enduranceCondition));
									}
									else {
										hasDeathChance = cdlookup->second.deathConditions[0] >= a->GetActorValue(*perceptionCondition)
											|| cdlookup->second.deathConditions[0] >= a->GetActorValue(*brainCondition);
										_MESSAGE("Head condition %f current %f", cdlookup->second.deathConditions[0], a->GetActorValue(*perceptionCondition));
									}
									if (hasDeathChance) {
										ActorValueInfo* avif = isTorso ? vestTier : helmetTier;
										uint16_t chance = cdlookup->second.protectionChances[(int)a->GetActorValue(*avif)];
										std::mt19937 e{ rd() }; // or std::default_random_engine e{rd()};
										std::uniform_int_distribution<uint16_t> dist{ 1, 100 };
										uint16_t result = dist(e);
										_MESSAGE("Protection Chance %d result %d", chance, result);
										if (result > chance) {
											killDeathMarked->Cast(a, a);
											if (a == PlayerCharacter::GetSingleton()) {
												if (playerForceKill) {
													a->KillImpl(a, 9999, true, false);
												}
												_MESSAGE("---Player Killed by DeathMark---");
											}
											else
												_MESSAGE("%s(%llx) should be killed by deathmark", a->GetNPC()->fullName.c_str());
										}
									}
								}
							}
							break; //Break if deathmark was found
						}
					}
				}
			}
		}
		return BSEventNotifyControl::kContinue;
	}
	F4_HEAP_REDEFINE_NEW(MGEFWatcher);
};

class PlayerDeathWatcher : public BSTEventSink<BGSActorDeathEvent> {
	virtual BSEventNotifyControl ProcessEvent(const BGSActorDeathEvent& evn, BSTEventSource<BGSActorDeathEvent>* src) override {
		_MESSAGE("---Player Death---");
		PlayerCharacter* p = PlayerCharacter::GetSingleton();
		ActiveEffectList* aeList = p->GetActiveEffectList();
		if (aeList) {
			for (auto it = aeList->data.begin(); it != aeList->data.end(); ++it) {
				ActiveEffect* ae = it->get();
				if (ae && !(ae->flags & ActiveEffect::kFlag_Inactive)) {
					EffectSetting* avEffectSetting = *(EffectSetting**)((uint64_t)(it->get()->effect) + 0x10);
					if (avEffectSetting && avEffectSetting->data.primaryAV == health) {
						_MESSAGE("Active Effect : %s with magnitude %f", avEffectSetting->fullName.c_str(), ae->magnitude);
					}
				}
			}
		}
		_MESSAGE("Head Condition\t%f", p->GetActorValue(*perceptionCondition));
		_MESSAGE("Torso Condition\t%f", p->GetActorValue(*enduranceCondition));
		_MESSAGE("LArm Condition\t%f", p->GetActorValue(*leftAttackCondition));
		_MESSAGE("LLeg Condition\t%f", p->GetActorValue(*leftMobilityCondition));
		_MESSAGE("RArm Condition\t%f", p->GetActorValue(*rightAttackCondition));
		_MESSAGE("RLeg Condition\t%f", p->GetActorValue(*rightMobilityCondition));
		_MESSAGE("Last Health remaining\t%f", evn.lastHealth);
		_MESSAGE("Last Damage taken\t%f", evn.damageTaken);
		return BSEventNotifyControl::kContinue;
	}
public:
	F4_HEAP_REDEFINE_NEW(PlayerDeathWatcher);
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
		float additionalDamage = 0;
		if (this->weaponSource.instanceData) {
			BSTArray<BSTTuple<TESForm*, BGSTypedFormValuePair::SharedVal>>* dtArray =
				static_cast<TESObjectWEAP::InstanceData*>(this->weaponSource.instanceData.get())->damageTypes;
			if (dtArray) {
				for (auto it = dtArray->begin(); it != dtArray->end(); ++it) {
					additionalDamage += it->second.i;
				}
			}
		}
		float calculatedDamage = this->damage + additionalDamage;
		if (calculatedDamage > 0 && 
			this->impacts.size() > 0 && 
			!ipct.processed 
			&& ipct.collidee.get() 
			&& ipct.collidee.get().get()->formType == ENUM_FORM_ID::kACHR 
			&& ipct.colObj
			&& ipct.collidee.get()->parentCell
			&& !ipct.collidee.get()->IsDead(true)) {
			Actor* a = static_cast<Actor*>(ipct.collidee.get().get());
			NiAVObject* parent = ipct.colObj.get()->sceneObject;
			if (parent) {
				_MESSAGE("Projectile %llx (formid %llx)", this, this->formID);

				if (a == PlayerCharacter::GetSingleton())
					_MESSAGE("%s(Player) got hit on %s", a->GetNPC()->fullName.c_str(), parent->name.c_str());
				else
					_MESSAGE("%s(%llx) got hit on %s", a->GetNPC()->fullName.c_str(), a, parent->name.c_str());
				int partType = -1;
				if (ipct.damageLimb.underlying() >= 0) {
					partType = ipct.damageLimb.underlying();
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
					_MESSAGE("Ammo : %s (FormID %llx) - Projectile FormID %llx",
								this->ammoSource->fullName.c_str(), this->ammoSource->formID,
								this->ammoSource->data.projectile->formID);
				}
				else if (this->weaponSource.instanceData && this->weaponSource.object->formType == ENUM_FORM_ID::kWEAP) {
					ammo = static_cast<TESObjectWEAP::InstanceData*>(this->weaponSource.instanceData.get())->ammo;
					_MESSAGE("Ammo : %s (FormID %llx) - Projectile FormID %llx",
								ammo->fullName.c_str(), ammo->formID,
								ammo->data.projectile->formID);
				}

				int partFound = 0;
				int i = 0;
				std::vector<BGSBodyPartData::PartType> partsList[] = { headParts, torsoParts, larmParts, llegParts, rarmParts, rlegParts };
				while (partFound == 0 && i < 6) {
					for (auto part = partsList[i].begin(); part != partsList[i].end(); ++part) {
						if (partType == *part) {
							a->SetActorValue(*lasthitpart, i + 1);
							partFound = i + 1;
						}
					}
					++i;
				}
				if (!partFound) {
					a->SetActorValue(*lasthitpart, 7);
				}
				else {
					_MESSAGE("Node : %s is %s (partType %d)", parent->name.c_str(), EFDBodyPartsName[partFound].c_str(), partType);
					if (this->damage > 0 && !this->IsBeamProjectile()) {
						_MESSAGE("Physical projectile with base damage %f", this->damage);
						ActiveEffectList* aeList = a->GetActiveEffectList();
						if (aeList) {
							BleedingData& bld = bleedingConfigs.at((EFDBodyParts)partFound);
							_MESSAGE("Bleeding condition start %f current %f", bld.conditionStart, a->GetActorValue(**EFDConditions[partFound]));
							if (bld.conditionStart >= a->GetActorValue(**EFDConditions[partFound])) {
								float bleedChance = bld.chanceMin
									+ (bld.chanceMax - bld.chanceMin) * (bld.conditionStart - max(bld.conditionThreshold, a->GetActorValue(**EFDConditions[partFound]))) / (bld.conditionStart - bld.conditionThreshold);
								std::mt19937 e{ rd() };
								std::uniform_real_distribution<float> dist{ 0, 100 };
								float result = dist(e);
								_MESSAGE("Bleeding Chance %f result %f", bleedChance, result);
								if (result && result <= bleedChance) {
									ActiveEffect* bleedae = nullptr;
									for (auto it = aeList->data.begin(); it != aeList->data.end(); ++it) {
										ActiveEffect* ae = it->get();
										if (ae && !(ae->flags & ActiveEffect::kFlag_Inactive) && ae->item == bld.spell) {
											bleedae = ae;
											_MESSAGE("%s is already bleeding", EFDBodyPartsName[partFound].c_str());
										}
									}
									float bleedmag = this->damage * bld.multiplier;
									if (bleedae) {
										bleedae->magnitude -= bleedmag;
										bleedae->elapsed = 0;
										_MESSAGE("Current bleeding magnitude %f", bleedae->magnitude * -1.0f);
									}
									else {
										bld.spell->listOfEffects[0]->data.magnitude = bld.initialDamage + bleedmag;
										bld.spell->listOfEffects[1]->data.magnitude = bld.initialDamage + bleedmag;
										bld.spell->Cast(a, a);
										_MESSAGE("Bleeding start magnitude %f", bld.initialDamage + bleedmag);
									}
								}
							}
						}
						else {
							_MESSAGE("ActiveEffectList not found. Bleeding can't be processed");
						}
						if (this->shooter.get()) {
							EFDDeathMarkGlobal->listOfEffects[0]->data.magnitude = gd.armorDamageInitial + this->damage * gd.armorDamageMultiplier;
							_MESSAGE("ArmorDamage magnitude %f", EFDDeathMarkGlobal->listOfEffects[0]->data.magnitude);
							EFDDeathMarkGlobal->Cast(this->shooter.get().get(), a);
						}
					}
				}
				if (partFound <= 2) { //Only calculate this when needed.
					CartridgeData& gcd = cartridgeData.at(std::string("Global"));
					for (int j = 0; j < maxTier; ++j) {
						gcd.protectionChances[j] = (uint16_t)max(min(log10(vestRatings[j] / gd.formulaA + 1.0f) * gd.formulaB + (gd.formulaC - calculatedDamage) * 0.58823529f + gd.formulaD, 100), 0);
					}
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
	_MESSAGE("Patching MissileProjectile %llx", addr);
	HookProjectile::HookProcessImpacts(addr, offset);

	addr = BeamProjectile::VTABLE[0].address();
	_MESSAGE("Patching BeamProjectile %llx", addr);
	HookProjectile::HookProcessImpacts(addr, offset);

	headHitMark = GetSpellByFullName(std::string("EFD Head HitMark"));
	headHitMarkMGEF = headHitMark->listOfEffects[0]->effectSetting;
	torsoHitMark = GetSpellByFullName(std::string("EFD Torso HitMark"));
	torsoHitMarkMGEF = torsoHitMark->listOfEffects[0]->effectSetting;
	killDeathMarked = GetSpellByFullName(std::string("EFD Kill DeathMarked"));
	_MESSAGE("EFD Head HitMark %llx", headHitMark);
	_MESSAGE("EFD Torso HitMark %llx", torsoHitMark);
	_MESSAGE("EFD Kill DeathMarked %llx", killDeathMarked);
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
			_MESSAGE("PlayerCharacter %llx", PlayerCharacter::GetSingleton());
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

			PlayerCharacter* p = PlayerCharacter::GetSingleton();
			PlayerDeathWatcher* pdw = new PlayerDeathWatcher();
			((BSTEventSource<BGSActorDeathEvent>*)p)->RegisterSink(pdw);
		}
	});

	return true;
}

#pragma endregion
