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

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Load(const F4SE::LoadInterface* a_f4se)
{
	F4SE::Init(a_f4se);

	//F4SE에서 제공하는 메세지 인터페이스
	const F4SE::MessagingInterface* message = F4SE::GetMessagingInterface();
	message->RegisterListener([](F4SE::MessagingInterface::Message* msg) -> void {
		//게임 데이터가 모두 로드된 시점 (메인화면이 나오기 직전)
		if (msg->type == F4SE::MessagingInterface::kGameDataReady) {
			//ini 파일 읽기
			ini.LoadFile("Data\\F4SE\\Plugins\\DeBadWeapons.ini");
			uint16_t uniqueDmg = (uint16_t)std::stoi(ini.GetValue("General", "UniqueDamage", "15"));
			spdlog::log(spdlog::level::warn, _MESSAGE("UniqueDamage %d", uniqueDmg));

			//탄약 화이트리스트 제작
			std::unordered_map<TESAmmo*, bool> ammoWhitelist = GetAmmunitionWhitelist();

			//바닐라에서 총과 유니크 무기의 정의를 가져옴
			BGSListForm* gunsKeywordList = (BGSListForm*)TESForm::GetFormByID(0xF78EC);
			BGSKeyword* uniqueKeyword = (BGSKeyword*)TESForm::GetFormByID(0x1B3FAC);

			//습관적으로 써버린 null체크 (사실 필요없음)
			if (gunsKeywordList) {
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
		}
	});

	return true;
}
