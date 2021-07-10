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

	*path /= "PlayerVoiceFrequency.log"sv;
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
	a_info->name = "PlayerVoiceFrequency";
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

	F4SE::AllocTrampoline(1024 * 64);

	return true;
}


CSimpleIniA ini(true, false, false);
BGSSoundCategory* playerSoundCategory;
float playerVoiceFrequency = 1.0f;
REL::Relocation<uintptr_t> ptr_changeTime{ REL::ID(249054), 0x2B };
REL::Relocation<uintptr_t> ptr_revertTime{ REL::ID(249054), 0x6B };
REL::Relocation<uintptr_t> ptr_overrideJump{ REL::ID(157156), 0x97 }; //This is the function where it checks if the frequency was overriden by a task.
REL::Relocation<float*> ptr_globalTimeMultiplier{ REL::ID(1266509) };

void SetPlayerVoiceFrequency(float currentTime) {
	ini.LoadFile("Data\\F4SE\\Plugins\\PlayerVoiceFrequency.ini");
	playerVoiceFrequency = std::stof(ini.GetValue("General", "Frequency", "1.0f"));
	if (playerVoiceFrequency < 0.05f) {
		playerVoiceFrequency = 0.05f;
	}

	if (playerSoundCategory) {
		playerSoundCategory->frequencyMult = playerVoiceFrequency;
		playerSoundCategory->minFrequencyMult = playerVoiceFrequency * 0.1f;
	}
}

void SetGlobalTimeMultiplier(uint64_t unk, float f) {
	typedef void (*func_t)(uint64_t, float);
	REL::Relocation<func_t> func{ REL::ID(1419977) };
	func(unk, f);
	SetPlayerVoiceFrequency(f);
}

struct MenuWatcher : public BSTEventSink<MenuOpenCloseEvent> {
	virtual BSEventNotifyControl ProcessEvent(const MenuOpenCloseEvent& evn, BSTEventSource<MenuOpenCloseEvent>* a_source) override {
		if (evn.menuName == BSFixedString("LoadingMenu") && !evn.opening) {
			SetPlayerVoiceFrequency(*ptr_globalTimeMultiplier);
		}
		return BSEventNotifyControl::kContinue;
	}
};

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Load(const F4SE::LoadInterface* a_f4se)
{
	F4SE::Init(a_f4se);

	F4SE::Trampoline& trampoline = F4SE::GetTrampoline();
	trampoline.write_call<5>(ptr_changeTime.address(), &SetGlobalTimeMultiplier);
	trampoline.write_call<5>(ptr_revertTime.address(), &SetGlobalTimeMultiplier);
	uint8_t bytes[] = { 0x90, 0x90 };
	REL::safe_write<uint8_t>(ptr_overrideJump.address(), std::span{ bytes });

	const F4SE::MessagingInterface* message = F4SE::GetMessagingInterface();
	message->RegisterListener([](F4SE::MessagingInterface::Message* msg) -> void {
		if (msg->type == F4SE::MessagingInterface::kGameDataReady) {
			MenuWatcher* mw = new MenuWatcher();
			UI::GetSingleton()->GetEventSource<MenuOpenCloseEvent>()->RegisterSink(mw);
			TESDataHandler* dh = TESDataHandler::GetSingleton();
			BSTArray<BGSSoundCategory*> sc = dh->GetFormArray<BGSSoundCategory>();
			for (auto it = sc.begin(); it != sc.end(); ++it) {
				if (strcmp((*it)->fullName.c_str(), "AudioCategoryVOCPlayer") == 0) {
					playerSoundCategory = *it;
					logger::warn(_MESSAGE("AudioCategoryVOCPlayer %llx", *it));
				}
			}
		}
	});

	return true;
}
