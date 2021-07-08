#include "SimpleIni.h"
using namespace RE;

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

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Query(const F4SE::QueryInterface* a_f4se, F4SE::PluginInfo* a_info)
{
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

//ini 정의, UTF-8 사용, 멀티키 X, 멀티라인 X 설정
CSimpleIniA ini(true, false, false);

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
	std::unordered_map<TESAmmo*, bool> ammoWhitelist = GetAmmunitionWhitelist();

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

uint16_t helmet_tier1Rating;
uint16_t helmet_tier2Rating;
uint16_t helmet_tier3Rating;
uint16_t vest_tier1Rating;
uint16_t vest_tier2Rating;
uint16_t vest_tier3Rating;
ActorValueInfo* helmetTier;
ActorValueInfo* vestTier;

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

void SetupArmors() {
	helmet_tier1Rating = (uint16_t)std::stoi(ini.GetValue("Armor", "HelmetTier1Reference", "5"));
	helmet_tier2Rating = (uint16_t)std::stoi(ini.GetValue("Armor", "HelmetTier2Reference", "15"));
	helmet_tier3Rating = (uint16_t)std::stoi(ini.GetValue("Armor", "HelmetTier3Reference", "25"));
	spdlog::log(spdlog::level::warn, _MESSAGE("Helmet Reference %d, %d, %d", helmet_tier1Rating, helmet_tier2Rating, helmet_tier3Rating));
	vest_tier1Rating = (uint16_t)std::stoi(ini.GetValue("Armor", "VestTier1Reference", "5"));
	vest_tier2Rating = (uint16_t)std::stoi(ini.GetValue("Armor", "VestTier2Reference", "15"));
	vest_tier3Rating = (uint16_t)std::stoi(ini.GetValue("Armor", "VestTier3Reference", "25"));
	spdlog::log(spdlog::level::warn, _MESSAGE("Vest Reference %d, %d, %d", vest_tier1Rating, vest_tier2Rating, vest_tier3Rating));

	helmetTier = GetAVIFByEditorID("EFD_Helmet_Tier");
	vestTier = GetAVIFByEditorID("EFD_Vest_Tier");
	spdlog::log(spdlog::level::warn, _MESSAGE("EFD_Helmet_Tier %llx", helmetTier));
	spdlog::log(spdlog::level::warn, _MESSAGE("EFD_Vest_Tier %llx", vestTier));
}

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
	uint32_t unk08;				//08
	uint32_t formId;			//0C
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

class EquipWatcher : public BSTEventSink<TESEquipEvent> {
public:
	virtual BSEventNotifyControl ProcessEvent(const TESEquipEvent& evn, BSTEventSource<TESEquipEvent>* a_source) {
		TESForm* item = TESForm::GetFormByID(evn.formId);
		if (item && item->formType == ENUM_FORM_ID::kARMO) {
			TESObjectARMO* armor = static_cast<TESObjectARMO*>(item);
			//TESObjectARMO::InstanceData* data = *(TESObjectARMO::InstanceData**)((uintptr_t)&evn + 0xd0);
			TESObjectARMO::InstanceData* data = &(armor->data);
			if (armor->bipedModelData.bipedObjectSlots & 0x800) {		//Vest
				if (evn.flag == 0x00000000ff000000) {
					evn.a->SetActorValue(*vestTier, 0.0f);
				}
				else {
					if (data->rating >= vest_tier3Rating) {
						spdlog::log(spdlog::level::warn, _MESSAGE("Tier3 vest %s (FormID %llx at %llx)", armor->fullName.c_str(), armor->formID, armor));
						evn.a->SetActorValue(*vestTier, 3.0f);
					}
					else if (data->rating >= vest_tier2Rating) {
						spdlog::log(spdlog::level::warn, _MESSAGE("Tier2 vest %s (FormID %llx at %llx)", armor->fullName.c_str(), armor->formID, armor));
						evn.a->SetActorValue(*vestTier, 2.0f);
					}
					else if (data->rating >= vest_tier1Rating) {
						spdlog::log(spdlog::level::warn, _MESSAGE("Tier1 vest %s (FormID %llx at %llx)", armor->fullName.c_str(), armor->formID, armor));
						evn.a->SetActorValue(*vestTier, 1.0f);
					}
					else {
						spdlog::log(spdlog::level::warn, _MESSAGE("Tier0 vest %s (FormID %llx at %llx)", armor->fullName.c_str(), armor->formID, armor));
						evn.a->SetActorValue(*vestTier, 0.0f);
					}
				}
			}
			else if (armor->bipedModelData.bipedObjectSlots & 0x7) {	//Helmet
				if (evn.flag == 0x00000000ff000000) {
					evn.a->SetActorValue(*helmetTier, 0.0f);
				}
				else {
					if (data->rating >= helmet_tier3Rating) {
						spdlog::log(spdlog::level::warn, _MESSAGE("Tier3 helmet %s (FormID %llx at %llx)", armor->fullName.c_str(), armor->formID, armor));
						evn.a->SetActorValue(*helmetTier, 3.0f);
					}
					else if (data->rating >= helmet_tier2Rating) {
						spdlog::log(spdlog::level::warn, _MESSAGE("Tier2 helmet %s (FormID %llx at %llx)", armor->fullName.c_str(), armor->formID, armor));
						evn.a->SetActorValue(*helmetTier, 2.0f);
					}
					else if (data->rating >= helmet_tier1Rating) {
						spdlog::log(spdlog::level::warn, _MESSAGE("Tier1 helmet %s (FormID %llx at %llx)", armor->fullName.c_str(), armor->formID, armor));
						evn.a->SetActorValue(*helmetTier, 1.0f);
					}
					else {
						spdlog::log(spdlog::level::warn, _MESSAGE("Tier0 helmet %s (FormID %llx at %llx)", armor->fullName.c_str(), armor->formID, armor));
						evn.a->SetActorValue(*helmetTier, 0.0f);
					}
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
		TESForm* form = TESForm::GetFormByID(evn.formId);
		if (form && form->formType == ENUM_FORM_ID::kACHR) {
			Actor* a = static_cast<Actor*>(form);
			spdlog::log(spdlog::level::warn, _MESSAGE("Actor FormID %llx at %llx", a->formID, a));
			for (auto item = a->inventoryList->data.begin(); item != a->inventoryList->data.end(); ++item) {
				if (item->stackData->IsEquipped()) {
					if (item->object->formType == ENUM_FORM_ID::kARMO) {
						TESObjectARMO* armor = static_cast<TESObjectARMO*>(item->object);
						TESObjectARMO::InstanceData* data = &(armor->data);
						if (armor->bipedModelData.bipedObjectSlots & 0x800) {		//Vest
							if (data->rating >= vest_tier3Rating) {
								spdlog::log(spdlog::level::warn, _MESSAGE("Tier3 vest %s (FormID %llx at %llx)", armor->fullName.c_str(), armor->formID, armor));
								a->SetActorValue(*vestTier, 3.0f);
							}
							else if (data->rating >= vest_tier2Rating) {
								spdlog::log(spdlog::level::warn, _MESSAGE("Tier2 vest %s (FormID %llx at %llx)", armor->fullName.c_str(), armor->formID, armor));
								a->SetActorValue(*vestTier, 2.0f);
							}
							else if (data->rating >= vest_tier1Rating) {
								spdlog::log(spdlog::level::warn, _MESSAGE("Tier1 vest %s (FormID %llx at %llx)", armor->fullName.c_str(), armor->formID, armor));
								a->SetActorValue(*vestTier, 1.0f);
							}
							else {
								spdlog::log(spdlog::level::warn, _MESSAGE("Tier0 vest %s (FormID %llx at %llx)", armor->fullName.c_str(), armor->formID, armor));
								a->SetActorValue(*vestTier, 0.0f);
							}
						}
						else if (armor->bipedModelData.bipedObjectSlots & 0x7) {	//Helmet
							if (data->rating >= helmet_tier3Rating) {
								spdlog::log(spdlog::level::warn, _MESSAGE("Tier3 helmet %s (FormID %llx at %llx)", armor->fullName.c_str(), armor->formID, armor));
								a->SetActorValue(*helmetTier, 1.0f);
							}
							else if (data->rating >= helmet_tier2Rating) {
								spdlog::log(spdlog::level::warn, _MESSAGE("Tier2 helmet %s (FormID %llx at %llx)", armor->fullName.c_str(), armor->formID, armor));
								a->SetActorValue(*helmetTier, 2.0f);
							}
							else if (data->rating >= helmet_tier1Rating) {
								spdlog::log(spdlog::level::warn, _MESSAGE("Tier1 helmet %s (FormID %llx at %llx)", armor->fullName.c_str(), armor->formID, armor));
								a->SetActorValue(*helmetTier, 3.0f);
							}
							else {
								spdlog::log(spdlog::level::warn, _MESSAGE("Tier0 helmet %s (FormID %llx at %llx)", armor->fullName.c_str(), armor->formID, armor));
								a->SetActorValue(*helmetTier, 0.0f);
							}
						}
					}
				}
			}
		}
		return BSEventNotifyControl::kContinue;
	}
	F4_HEAP_REDEFINE_NEW(ObjectLoadWatcher);
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
			EquipWatcher* ew = new EquipWatcher();
			EquipEventSource::GetSingleton()->RegisterSink(ew);

			ObjectLoadWatcher* olw = new ObjectLoadWatcher();
			ObjectLoadedEventSource::GetSingleton()->RegisterSink(olw);
		}
	});

	return true;
}
