#include <regex>
using namespace RE;
using std::regex;
using std::smatch;
using std::string;

char tempbuf[8192] = { 0 };
char* _MESSAGE(const char* fmt, ...) {
	va_list args;

	va_start(args, fmt);
	vsnprintf(tempbuf, sizeof(tempbuf), fmt, args);
	va_end(args);
	spdlog::log(spdlog::level::warn, tempbuf);

	return tempbuf;
}

#pragma region Papyrus
std::vector<string> GetAmmoData(std::monostate, TESObjectWEAP * wep) {
	std::vector<string> ret{ "", "" };
	if (wep->GetFormType() != ENUM_FORM_ID::kWEAP) {
		return ret;
	}
	PlayerCharacter* p = PlayerCharacter::GetSingleton();
	if (p->inventoryList) {
		for (auto invitem = p->inventoryList->data.begin(); invitem != p->inventoryList->data.end(); ++invitem) {
			if (invitem->object->formID == wep->formID) {
				ExtraInstanceData* wepInstanceExtra = (ExtraInstanceData*)invitem->stackData->extra->GetByType(EXTRA_DATA_TYPE::kInstanceData);
				if (wepInstanceExtra) {
					TESObjectWEAP::InstanceData* data = (TESObjectWEAP::InstanceData*)wepInstanceExtra->data.get();
					if (data && data->ammo) {
						regex pattern_vistag("\\[([A-Za-z]+)(?!\\])(.*?)\\]");
						smatch visresult;
						string fullname = data->ammo->GetFullName();
						if (regex_search(fullname, visresult, pattern_vistag)) {
							fullname = visresult.suffix().str();
							if (fullname.at(0) == ' ') {
								fullname = fullname.substr(1);
							}
						}
						ret[0] = fullname;
						ret[1] = std::to_string(data->ammo->formID);
						return ret;
					}
				}
			}
		}
	}
	return ret;
}

bool RegisterFuncs(BSScript::IVirtualMachine* a_vm) {
	a_vm->BindNativeMethod("WheelMenuHelper", "GetAmmoData", GetAmmoData);
	return true;
}

#pragma endregion

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Query(const F4SE::QueryInterface* a_f4se, F4SE::PluginInfo* a_info)
{
#ifndef NDEBUG
	auto sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
#else
	auto path = logger::log_directory();
	if (!path) {
		return false;
	}

	*path /= "WheelMenuHelper.log"sv;
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
	a_info->name = "WheelMenuHelper";
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

	const F4SE::PapyrusInterface* papyrus = F4SE::GetPapyrusInterface();
	bool succ = papyrus->Register(RegisterFuncs);
	if (succ) {
		logger::warn("Papyrus functions registered.");
	}

	return true;
}
