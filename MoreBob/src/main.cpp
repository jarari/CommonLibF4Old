#include <SimpleIni.h>
#include "Utilities.h"
using namespace RE;
using std::unordered_map;
CSimpleIniA ini(true, true, false);
PlayerCharacter* p;
float mul1;
float mul2;
float dur1;
float dur2;
float str1;
float str2;

class AnimationGraphEventWatcher {
public:
	typedef BSEventNotifyControl (AnimationGraphEventWatcher::* FnProcessEvent)(BSAnimationGraphEvent& evn, BSTEventSource<BSAnimationGraphEvent>* dispatcher);

	BSEventNotifyControl HookedProcessEvent(BSAnimationGraphEvent& evn, BSTEventSource<BSAnimationGraphEvent>* src) {
		if (evn.animEvent == "FootLeft" || evn.animEvent == "FootRight") {
			F4::ShakeCamera(mul1, p->data.location, dur1, str1);
		}
		else if (evn.animEvent == "FootSprintLeft" || evn.animEvent == "FootSprintRight") {
			F4::ShakeCamera(mul2, p->data.location, dur2, str2);
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
	static unordered_map<uint64_t, FnProcessEvent> fnHash;
};
unordered_map<uint64_t, AnimationGraphEventWatcher::FnProcessEvent> AnimationGraphEventWatcher::fnHash;

void LoadConfigs() {
	ini.LoadFile("Data\\F4SE\\Plugins\\MoreBob.ini");
	mul1 = std::stof(ini.GetValue("General", "Mul1", "0.5"));
	dur1 = std::stof(ini.GetValue("General", "Dur1", "0.3"));
	str1 = std::stof(ini.GetValue("General", "Str1", "0.1"));
	mul2 = std::stof(ini.GetValue("General", "Mul2", "0.75"));
	dur2 = std::stof(ini.GetValue("General", "Dur2", "0.45"));
	str2 = std::stof(ini.GetValue("General", "Str2", "0.15"));
	ini.Reset();
}

void InitializePlugin() {
	p = PlayerCharacter::GetSingleton();
	((AnimationGraphEventWatcher*)((uint64_t)p + 0x38))->HookSink();
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
