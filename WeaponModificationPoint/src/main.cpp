#include <SimpleIni.h>
#include "Utilities.h"
#define ADDLEVELEDOBJECT(CLASSNAME) LL_##CLASSNAME->AddLeveledObject(0, 1, 0, LL_WMP_##CLASSNAME, nullptr)
#define REGISTERRATIOMAP(COMPONENT) ratioMap[c_##COMPONENT] = std::stof(ini.GetValue("Components", #COMPONENT, "0.33"));
#define LOADCONTAINER(CONTAINER) SetupWeaponModPoint(LL_WMP_Container_##CONTAINER, (uint8_t)std::stoi(ini.GetValue("Containers", #CONTAINER "Chance", "0")), (uint16_t)std::stoi(ini.GetValue("Containers", #CONTAINER "Count", "0")))
#define LOADFACTION(FACTION) SetupWeaponModPoint(LL_WMP_##FACTION, (uint8_t)std::stoi(ini.GetValue("Factions", #FACTION "Chance", "0")), (uint16_t)std::stoi(ini.GetValue("Factions", #FACTION "Count", "0")))
#define LOADPERK(PERK) SetupWeaponModPoint(LL_WMP_##PERK, 100, (uint16_t)std::stoi(ini.GetValue("Perks", #PERK "Count", "0")))
using namespace RE;
using std::unordered_map;
using std::vector;
CSimpleIniA ini(true, true, false);

bool COBJmodified = false;

BGSComponent* c_Acid;
BGSComponent* c_Adhesive;
BGSComponent* c_Aluminum;
BGSComponent* c_AntiBallisticFiber;
BGSComponent* c_Antiseptic;
BGSComponent* c_Asbestos;
BGSComponent* c_Bone;
BGSComponent* c_Ceramic;
BGSComponent* c_Circuitry;
BGSComponent* c_Cloth;
BGSComponent* c_Concrete;
BGSComponent* c_Copper;
BGSComponent* c_Cork;
BGSComponent* c_Crystal;
BGSComponent* c_Fertilizer;
BGSComponent* c_Fiberglass;
BGSComponent* c_FiberOptics;
BGSComponent* c_Gears;
BGSComponent* c_Glass;
BGSComponent* c_Gold;
BGSComponent* c_Lead;
BGSComponent* c_Leather;
BGSComponent* c_NuclearMaterial;
BGSComponent* c_Oil;
BGSComponent* c_Plastic;
BGSComponent* c_Rubber;
BGSComponent* c_Screws;
BGSComponent* c_Silver;
BGSComponent* c_Springs;
BGSComponent* c_Steel;
BGSComponent* c_Wood;
static unordered_map<TESForm*, float> ratioMap;

TESLevItem* LL_Container_AmmoBox;
TESLevItem* LL_Container_AmmoBox_Raider;
TESLevItem* LL_Container_DuffleBag_Guns;
TESLevItem* LL_Container_DuffleBag_Guns_Raider;
TESLevItem* LL_Container_Raider_Boss;
TESLevItem* LL_Container_Raider_Safe;
TESLevItem* LL_Container_Safe_Prewar;
TESLevItem* LL_Container_ToolBox;
TESLevItem* LL_Container_ToolBox_Large;
TESLevItem* LL_Container_ToolBox_Raider;
TESLevItem* LL_Container_ToolCase_Guns;
TESLevItem* LL_Container_ToolCase_Guns_Raider;
TESLevItem* LL_Container_Trunk;
TESLevItem* LL_Container_Trunk_Prewar;
TESLevItem* LL_Container_Trunk_Prewar_Boss;
TESLevItem* LL_Container_Trunk_Raider_Boss;
TESLevItem* LL_Perk_ScroungerSmall;
TESLevItem* LL_Perk_Scrounger100;

TESLevItem* LL_WMP_Assaultron;
TESLevItem* LL_WMP_Bloatfly;
TESLevItem* LL_WMP_Bloodbug;
TESLevItem* LL_WMP_BoS;
TESLevItem* LL_WMP_Brahmin;
TESLevItem* LL_WMP_CoA;
TESLevItem* LL_WMP_Deathclaw;
TESLevItem* LL_WMP_Dog;
TESLevItem* LL_WMP_EyeBot;
TESLevItem* LL_WMP_Ghoul;
TESLevItem* LL_WMP_Gunner;
TESLevItem* LL_WMP_Institute;
TESLevItem* LL_WMP_Minuteman;
TESLevItem* LL_WMP_Mirelurk;
TESLevItem* LL_WMP_MirelurkHunter;
TESLevItem* LL_WMP_MirelurkKing;
TESLevItem* LL_WMP_MirelurkQueen;
TESLevItem* LL_WMP_Molerat;
TESLevItem* LL_WMP_Protectron;
TESLevItem* LL_WMP_Radroach;
TESLevItem* LL_WMP_RadScorpion;
TESLevItem* LL_WMP_RadStag;
TESLevItem* LL_WMP_Raider;
TESLevItem* LL_WMP_Railroad;
TESLevItem* LL_WMP_SentryBot;
TESLevItem* LL_WMP_Supermutant;
TESLevItem* LL_WMP_SupermutantBehemoth;
TESLevItem* LL_WMP_Turret;
TESLevItem* LL_WMP_YaoGuai;
TESLevItem* LL_WMP_Perk_ScroungerSmall;
TESLevItem* LL_WMP_Perk_Scrounger100;

TESLevItem* LL_WMP_Container_AmmoBox;
TESLevItem* LL_WMP_Container_AmmoBox_Raider;
TESLevItem* LL_WMP_Container_Change;
TESLevItem* LL_WMP_Container_DuffleBag_Guns;
TESLevItem* LL_WMP_Container_DuffleBag_Guns_Raider;
TESLevItem* LL_WMP_Container_Raider_Boss;
TESLevItem* LL_WMP_Container_Raider_Safe;
TESLevItem* LL_WMP_Container_Safe_Prewar;
TESLevItem* LL_WMP_Container_ToolBox;
TESLevItem* LL_WMP_Container_ToolBox_Large;
TESLevItem* LL_WMP_Container_ToolBox_Raider;
TESLevItem* LL_WMP_Container_ToolCase_Guns;
TESLevItem* LL_WMP_Container_ToolCase_Guns_Raider;
TESLevItem* LL_WMP_Container_Trunk;
TESLevItem* LL_WMP_Container_Trunk_Prewar;
TESLevItem* LL_WMP_Container_Trunk_Prewar_Boss;
TESLevItem* LL_WMP_Container_Trunk_Raider_Boss;

TESObjectMISC* WeaponModPoint;
BGSComponent* c_WMP;
TESGlobal* WMPScrapScalar;

void AcquireForms() {
	c_Acid = (BGSComponent*)TESForm::GetFormByID(0x1FA8C);
	c_Adhesive = (BGSComponent*)TESForm::GetFormByID(0x1FAA5);
	c_Aluminum = (BGSComponent*)TESForm::GetFormByID(0x1FA91);
	c_AntiBallisticFiber = (BGSComponent*)TESForm::GetFormByID(0x1FA94);
	c_Antiseptic = (BGSComponent*)TESForm::GetFormByID(0x1FA96);
	c_Asbestos = (BGSComponent*)TESForm::GetFormByID(0x1FA97);
	c_Bone = (BGSComponent*)TESForm::GetFormByID(0x1FA98);
	c_Ceramic = (BGSComponent*)TESForm::GetFormByID(0x1FA9A);
	c_Circuitry = (BGSComponent*)TESForm::GetFormByID(0x1FA9B);
	c_Cloth = (BGSComponent*)TESForm::GetFormByID(0x1223C7);
	c_Concrete = (BGSComponent*)TESForm::GetFormByID(0x5A0C4);
	c_Copper = (BGSComponent*)TESForm::GetFormByID(0x1FA9C);
	c_Cork = (BGSComponent*)TESForm::GetFormByID(0x1FA9D);
	c_Crystal = (BGSComponent*)TESForm::GetFormByID(0x1FA9F);
	c_Fertilizer = (BGSComponent*)TESForm::GetFormByID(0x5A0C7);
	c_Fiberglass = (BGSComponent*)TESForm::GetFormByID(0x1FAA1);
	c_FiberOptics = (BGSComponent*)TESForm::GetFormByID(0x1FAA0);
	c_Gears = (BGSComponent*)TESForm::GetFormByID(0x1FAB0);
	c_Glass = (BGSComponent*)TESForm::GetFormByID(0x1FAA4);
	c_Gold = (BGSComponent*)TESForm::GetFormByID(0x1FAA6);
	c_Lead = (BGSComponent*)TESForm::GetFormByID(0x1FAAD);
	c_Leather = (BGSComponent*)TESForm::GetFormByID(0x1FAAE);
	c_NuclearMaterial = (BGSComponent*)TESForm::GetFormByID(0x1FAB3);
	c_Oil = (BGSComponent*)TESForm::GetFormByID(0x1FAB4);
	c_Plastic = (BGSComponent*)TESForm::GetFormByID(0x1FAB7);
	c_Rubber = (BGSComponent*)TESForm::GetFormByID(0x1FAB9);
	c_Screws = (BGSComponent*)TESForm::GetFormByID(0x3D294);
	c_Silver = (BGSComponent*)TESForm::GetFormByID(0x1FABB);
	c_Springs = (BGSComponent*)TESForm::GetFormByID(0x1FABC);
	c_Steel = (BGSComponent*)TESForm::GetFormByID(0x1FABD);
	c_Wood = (BGSComponent*)TESForm::GetFormByID(0x1FAC2);

	LL_Container_AmmoBox = (TESLevItem*)TESForm::GetFormByID(0x6D4A3);
	LL_Container_AmmoBox_Raider = (TESLevItem*)TESForm::GetFormByID(0x6D4A4);
	LL_Container_DuffleBag_Guns = (TESLevItem*)TESForm::GetFormByID(0x6D4A8);
	LL_Container_DuffleBag_Guns_Raider = (TESLevItem*)TESForm::GetFormByID(0x6D4A9);
	LL_Container_Raider_Boss = (TESLevItem*)TESForm::GetFormByID(0x6D4B0);
	LL_Container_Raider_Safe = (TESLevItem*)TESForm::GetFormByID(0x1B8812);
	LL_Container_Safe_Prewar = (TESLevItem*)TESForm::GetFormByID(0x6D4AF);
	LL_Container_ToolBox = (TESLevItem*)TESForm::GetFormByID(0x6D4B3);
	LL_Container_ToolBox_Large = (TESLevItem*)TESForm::GetFormByID(0x21E0AC);
	LL_Container_ToolBox_Raider = (TESLevItem*)TESForm::GetFormByID(0x6D4B4);
	LL_Container_ToolCase_Guns = (TESLevItem*)TESForm::GetFormByID(0x6D4B5);
	LL_Container_ToolCase_Guns_Raider = (TESLevItem*)TESForm::GetFormByID(0x6D4B6);
	LL_Container_Trunk = (TESLevItem*)TESForm::GetFormByID(0xD6E66);
	LL_Container_Trunk_Prewar = (TESLevItem*)TESForm::GetFormByID(0x6D4B7);
	LL_Container_Trunk_Prewar_Boss = (TESLevItem*)TESForm::GetFormByID(0x192FA);
	LL_Container_Trunk_Raider_Boss = (TESLevItem*)TESForm::GetFormByID(0x6D4B8);
	LL_Perk_ScroungerSmall = (TESLevItem*)TESForm::GetFormByID(0x1081B9);
	LL_Perk_Scrounger100 = (TESLevItem*)TESForm::GetFormByID(0x5859B);

	LL_WMP_Assaultron = (TESLevItem*)GetFormFromMod("WeaponModPoints.esp", 0x829);
	LL_WMP_Bloatfly = (TESLevItem*)GetFormFromMod("WeaponModPoints.esp", 0x81A);
	LL_WMP_Bloodbug = (TESLevItem*)GetFormFromMod("WeaponModPoints.esp", 0x81B);
	LL_WMP_BoS = (TESLevItem*)GetFormFromMod("WeaponModPoints.esp", 0x817);
	LL_WMP_Brahmin = (TESLevItem*)GetFormFromMod("WeaponModPoints.esp", 0x81C);
	LL_WMP_CoA = (TESLevItem*)GetFormFromMod("WeaponModPoints.esp", 0x81E);
	LL_WMP_Deathclaw = (TESLevItem*)GetFormFromMod("WeaponModPoints.esp", 0x81F);
	LL_WMP_Dog = (TESLevItem*)GetFormFromMod("WeaponModPoints.esp", 0x81D);
	LL_WMP_EyeBot = (TESLevItem*)GetFormFromMod("WeaponModPoints.esp", 0x82A);
	LL_WMP_Ghoul = (TESLevItem*)GetFormFromMod("WeaponModPoints.esp", 0x815);
	LL_WMP_Gunner = (TESLevItem*)GetFormFromMod("WeaponModPoints.esp", 0x813);
	LL_WMP_Institute = (TESLevItem*)GetFormFromMod("WeaponModPoints.esp", 0x818);
	LL_WMP_Minuteman = (TESLevItem*)GetFormFromMod("WeaponModPoints.esp", 0x814);
	LL_WMP_Mirelurk = (TESLevItem*)GetFormFromMod("WeaponModPoints.esp", 0x821);
	LL_WMP_MirelurkHunter = (TESLevItem*)GetFormFromMod("WeaponModPoints.esp", 0x822);
	LL_WMP_MirelurkKing = (TESLevItem*)GetFormFromMod("WeaponModPoints.esp", 0x823);
	LL_WMP_MirelurkQueen = (TESLevItem*)GetFormFromMod("WeaponModPoints.esp", 0x824);
	LL_WMP_Molerat = (TESLevItem*)GetFormFromMod("WeaponModPoints.esp", 0x825);
	LL_WMP_Protectron = (TESLevItem*)GetFormFromMod("WeaponModPoints.esp", 0x82B);
	LL_WMP_Radroach = (TESLevItem*)GetFormFromMod("WeaponModPoints.esp", 0x826);
	LL_WMP_RadScorpion = (TESLevItem*)GetFormFromMod("WeaponModPoints.esp", 0x827);
	LL_WMP_RadStag = (TESLevItem*)GetFormFromMod("WeaponModPoints.esp", 0x828);
	LL_WMP_Raider = (TESLevItem*)GetFormFromMod("WeaponModPoints.esp", 0x812);
	LL_WMP_Railroad = (TESLevItem*)GetFormFromMod("WeaponModPoints.esp", 0x819);
	LL_WMP_SentryBot = (TESLevItem*)GetFormFromMod("WeaponModPoints.esp", 0x82C);
	LL_WMP_Supermutant = (TESLevItem*)GetFormFromMod("WeaponModPoints.esp", 0x816);
	LL_WMP_SupermutantBehemoth = (TESLevItem*)GetFormFromMod("WeaponModPoints.esp", 0x82D);
	LL_WMP_Turret = (TESLevItem*)GetFormFromMod("WeaponModPoints.esp", 0x830);
	LL_WMP_YaoGuai = (TESLevItem*)GetFormFromMod("WeaponModPoints.esp", 0x820);
	LL_WMP_Perk_ScroungerSmall = (TESLevItem*)GetFormFromMod("WeaponModPoints.esp", 0x82E);
	LL_WMP_Perk_Scrounger100 = (TESLevItem*)GetFormFromMod("WeaponModPoints.esp", 0x82F);

	LL_WMP_Container_AmmoBox = (TESLevItem*)GetFormFromMod("WeaponModPoints.esp", 0x802);
	LL_WMP_Container_AmmoBox_Raider = (TESLevItem*)GetFormFromMod("WeaponModPoints.esp", 0x803);
	LL_WMP_Container_Change = (TESLevItem*)GetFormFromMod("WeaponModPoints.esp", 0x801);
	LL_WMP_Container_DuffleBag_Guns = (TESLevItem*)GetFormFromMod("WeaponModPoints.esp", 0x804);
	LL_WMP_Container_DuffleBag_Guns_Raider = (TESLevItem*)GetFormFromMod("WeaponModPoints.esp", 0x805);
	LL_WMP_Container_Raider_Boss = (TESLevItem*)GetFormFromMod("WeaponModPoints.esp", 0x806);
	LL_WMP_Container_Raider_Safe = (TESLevItem*)GetFormFromMod("WeaponModPoints.esp", 0x807);
	LL_WMP_Container_Safe_Prewar = (TESLevItem*)GetFormFromMod("WeaponModPoints.esp", 0x808);
	LL_WMP_Container_ToolBox = (TESLevItem*)GetFormFromMod("WeaponModPoints.esp", 0x809);
	LL_WMP_Container_ToolBox_Large = (TESLevItem*)GetFormFromMod("WeaponModPoints.esp", 0x80A);
	LL_WMP_Container_ToolBox_Raider = (TESLevItem*)GetFormFromMod("WeaponModPoints.esp", 0x80B);
	LL_WMP_Container_ToolCase_Guns = (TESLevItem*)GetFormFromMod("WeaponModPoints.esp", 0x80C);
	LL_WMP_Container_ToolCase_Guns_Raider = (TESLevItem*)GetFormFromMod("WeaponModPoints.esp", 0x80D);
	LL_WMP_Container_Trunk = (TESLevItem*)GetFormFromMod("WeaponModPoints.esp", 0x80E);
	LL_WMP_Container_Trunk_Prewar = (TESLevItem*)GetFormFromMod("WeaponModPoints.esp", 0x80F);
	LL_WMP_Container_Trunk_Prewar_Boss = (TESLevItem*)GetFormFromMod("WeaponModPoints.esp", 0x810);
	LL_WMP_Container_Trunk_Raider_Boss = (TESLevItem*)GetFormFromMod("WeaponModPoints.esp", 0x811);

	WeaponModPoint = (TESObjectMISC*)GetFormFromMod("WeaponModPoints.esp", 0x800);
	c_WMP = (BGSComponent*)GetFormFromMod("WeaponModPoints.esp", 0x831);
	WMPScrapScalar = (TESGlobal*)GetFormFromMod("WeaponModPoints.esp", 0x832);
}

void InsertLeveledItem() {
	ADDLEVELEDOBJECT(Container_AmmoBox);
	ADDLEVELEDOBJECT(Container_AmmoBox_Raider);
	ADDLEVELEDOBJECT(Container_DuffleBag_Guns);
	ADDLEVELEDOBJECT(Container_DuffleBag_Guns_Raider);
	ADDLEVELEDOBJECT(Container_Raider_Boss);
	ADDLEVELEDOBJECT(Container_Raider_Safe);
	ADDLEVELEDOBJECT(Container_Safe_Prewar);
	ADDLEVELEDOBJECT(Container_ToolBox);
	ADDLEVELEDOBJECT(Container_ToolBox_Large);
	ADDLEVELEDOBJECT(Container_ToolBox_Raider);
	ADDLEVELEDOBJECT(Container_ToolCase_Guns);
	ADDLEVELEDOBJECT(Container_ToolCase_Guns_Raider);
	ADDLEVELEDOBJECT(Container_Trunk);
	ADDLEVELEDOBJECT(Container_Trunk_Prewar);
	ADDLEVELEDOBJECT(Container_Trunk_Prewar_Boss);
	ADDLEVELEDOBJECT(Container_Trunk_Raider_Boss);
	ADDLEVELEDOBJECT(Perk_ScroungerSmall);
	ADDLEVELEDOBJECT(Perk_Scrounger100);
}

void ModifyCOBJ() {
	TESDataHandler* dh = TESDataHandler::GetSingleton();
	BSTArray<BGSConstructibleObject*> cobjs = dh->GetFormArray<BGSConstructibleObject>();
	for (auto it = cobjs.begin(); it != cobjs.end(); ++it) {
		if ((*it)->createdItem && (*it)->requiredItems && (*it)->createdItem->GetFormType() == ENUM_FORM_ID::kOMOD) {
			BGSMod::Attachment::Mod* mod = (BGSMod::Attachment::Mod * )(*it)->createdItem;
			if (mod->targetFormType == ENUM_FORM_ID::kWEAP) {
				float pointsReq = 0;
				vector<std::pair<TESForm*, BGSTypedFormValuePair::SharedVal>> newList;
				for (auto req = (*it)->requiredItems->begin(); req != (*it)->requiredItems->end(); ++req) {
					auto compit = ratioMap.find(req->first);
					if (compit != ratioMap.end()) {
						pointsReq += req->second.i * compit->second * ((BGSComponent*)compit->first)->value;
					}
					else {
						newList.push_back(std::pair<TESForm*, BGSTypedFormValuePair::SharedVal>(req->first, req->second));
					}
				}
				(*it)->requiredItems->clear();
				for (auto newreq = newList.begin(); newreq != newList.end(); ++newreq) {
					(*it)->requiredItems->push_back(BSTTuple<TESForm*, BGSTypedFormValuePair::SharedVal>(newreq->first, newreq->second));
				}
				(*it)->requiredItems->push_back(BSTTuple<TESForm*, BGSTypedFormValuePair::SharedVal>(c_WMP, (uint32_t)ceil(pointsReq)));
			}
		}
	}
}

void SetupWeaponModPoint(TESLevItem* form, uint8_t chance, uint16_t count) {
	for (int i = 0; i < form->baseListCount; ++i) {
		if (form->leveledLists[i].form == WeaponModPoint) {
			form->leveledLists[i].chanceNone = 100 - chance;
			form->leveledLists[i].count = count;
		}
	}
}

void LoadConfigs() {
	ini.LoadFile("Data\\F4SE\\Plugins\\WMP.ini");

	if (!COBJmodified) {
		REGISTERRATIOMAP(Acid);
		REGISTERRATIOMAP(Adhesive);
		REGISTERRATIOMAP(Aluminum);
		REGISTERRATIOMAP(AntiBallisticFiber);
		REGISTERRATIOMAP(Antiseptic);
		REGISTERRATIOMAP(Asbestos);
		REGISTERRATIOMAP(Bone);
		REGISTERRATIOMAP(Ceramic);
		REGISTERRATIOMAP(Circuitry);
		REGISTERRATIOMAP(Cloth);
		REGISTERRATIOMAP(Concrete);
		REGISTERRATIOMAP(Copper);
		REGISTERRATIOMAP(Cork);
		REGISTERRATIOMAP(Crystal);
		REGISTERRATIOMAP(Fertilizer);
		REGISTERRATIOMAP(Fiberglass);
		REGISTERRATIOMAP(FiberOptics);
		REGISTERRATIOMAP(Gears);
		REGISTERRATIOMAP(Glass);
		REGISTERRATIOMAP(Gold);
		REGISTERRATIOMAP(Lead);
		REGISTERRATIOMAP(Leather);
		REGISTERRATIOMAP(NuclearMaterial);
		REGISTERRATIOMAP(Oil);
		REGISTERRATIOMAP(Plastic);
		REGISTERRATIOMAP(Rubber);
		REGISTERRATIOMAP(Screws);
		REGISTERRATIOMAP(Silver);
		REGISTERRATIOMAP(Springs);
		REGISTERRATIOMAP(Steel);
		REGISTERRATIOMAP(Wood);
		ModifyCOBJ();
		COBJmodified = true;
	}

	LOADCONTAINER(AmmoBox);
	LOADCONTAINER(AmmoBox_Raider);
	LOADCONTAINER(DuffleBag_Guns);
	LOADCONTAINER(DuffleBag_Guns_Raider);
	LOADCONTAINER(Raider_Boss);
	LOADCONTAINER(Raider_Safe);
	LOADCONTAINER(Safe_Prewar);
	LOADCONTAINER(ToolBox);
	LOADCONTAINER(ToolBox_Large);
	LOADCONTAINER(ToolBox_Raider);
	LOADCONTAINER(ToolCase_Guns);
	LOADCONTAINER(ToolCase_Guns_Raider);
	LOADCONTAINER(Trunk);
	LOADCONTAINER(Trunk_Prewar);
	LOADCONTAINER(Trunk_Prewar_Boss);
	LOADCONTAINER(Trunk_Raider_Boss);

	LOADFACTION(Assaultron);
	LOADFACTION(Bloatfly);
	LOADFACTION(Bloodbug);
	LOADFACTION(BoS);
	LOADFACTION(Brahmin);
	LOADFACTION(CoA);
	LOADFACTION(Deathclaw);
	LOADFACTION(Dog);
	LOADFACTION(EyeBot);
	LOADFACTION(Ghoul);
	LOADFACTION(Gunner);
	LOADFACTION(Institute);
	LOADFACTION(Minuteman);
	LOADFACTION(Mirelurk);
	LOADFACTION(MirelurkHunter);
	LOADFACTION(MirelurkKing);
	LOADFACTION(MirelurkQueen);
	LOADFACTION(Molerat);
	LOADFACTION(Protectron);
	LOADFACTION(Radroach);
	LOADFACTION(RadScorpion);
	LOADFACTION(RadStag);
	LOADFACTION(Raider);
	LOADFACTION(Railroad);
	LOADFACTION(SentryBot);
	LOADFACTION(Supermutant);
	LOADFACTION(SupermutantBehemoth);
	LOADFACTION(Turret);
	LOADFACTION(YaoGuai);

	LOADPERK(Perk_ScroungerSmall);
	LOADPERK(Perk_Scrounger100);

	WMPScrapScalar->value = std::stof(ini.GetValue("Components", "ScrapRate", "0.1"));

	ini.Reset();
}

void InitializePlugin() {
	AcquireForms();
	InsertLeveledItem();
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
		else if (msg->type == F4SE::MessagingInterface::kPreLoadGame) {
			LoadConfigs();
		}
		else if (msg->type == F4SE::MessagingInterface::kNewGame) {
			LoadConfigs();
		}
	});

	return true;
}
