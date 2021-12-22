#include <Windows.h>
#include "SimpleIni.h"
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

#pragma endregion
CSimpleIniA ini(true, false, false);

PlayerCharacter* p;
PlayerCamera* pcam;

bool PUBGMode = true;
float tapTimeout = 0.12f;
bool performanceMode = true;
uint32_t defaultZoomKey = 257;
uint32_t defaultSprintKey = 160;

bool aiming = false;
bool waitSprint = false;
REL::Relocation<uint64_t*> ptr_engineTime{ REL::ID(1280610) };
uint64_t timer_sightEnter;
uint64_t timer_sightExit;

/*
* f3rdPersonAimFOV:Camera
* fDefault1stPersonFOV:Display
*/
Setting* TPAimFOV;
Setting* FPFOV;
TESIdleForm* FPSighted;

void SightEnter() {
	if (!p->currentProcess)
		return;
	BSTArray<EquippedItem>& equipped = p->currentProcess->middleHigh->equippedItems;
	if (equipped.size() == 0 || !equipped[0].item.instanceData || 
		((TESObjectWEAP::InstanceData*)equipped[0].item.instanceData.get())->type != 9)
		return;

	if (pcam->currentState == pcam->cameraStates[CameraState::k3rdPerson]) {
		pcam->SetState(pcam->cameraStates[CameraState::kFirstPerson].get());
		timer_sightEnter = *ptr_engineTime;
		aiming = true;
		p->SetInIronSightsImpl(true);
		if (PUBGMode && pcam->fovAdjustPerSec != 0) {
			float FOVOffset = FPFOV->GetFloat() - TPAimFOV->GetFloat();
			pcam->fovAdjustTarget += FOVOffset;
			if ((pcam->fovAdjustTarget - pcam->fovAdjustCurrent) * pcam->fovAdjustPerSec < 0) {
				pcam->fovAdjustPerSec *= -1.0f;
			}
			pcam->fovAdjustCurrent = pcam->fovAdjustTarget;
			*(float*)((uintptr_t)pcam->cameraStates[CameraState::kFirstPerson].get() + 0x78) = 1.0f;
		}
	}
}

void SightExit(bool switchCam = true) {
	if (!p->currentProcess)
		return;
	BSTArray<EquippedItem>& equipped = p->currentProcess->middleHigh->equippedItems;
	if (equipped.size() == 0 || !equipped[0].item.instanceData ||
		((TESObjectWEAP::InstanceData*)equipped[0].item.instanceData.get())->type != 9)
		return;

	if (switchCam) {
		pcam->SetState(pcam->cameraStates[CameraState::k3rdPerson].get());
	}
	timer_sightExit = *ptr_engineTime;
	aiming = false;
}

using std::unordered_map;
class AnimationGraphEventWatcher {
public:
	typedef BSEventNotifyControl (AnimationGraphEventWatcher::* FnProcessEvent)(BSAnimationGraphEvent& evn, BSTEventSource<BSAnimationGraphEvent>* dispatcher);

	BSEventNotifyControl HookedProcessEvent(BSAnimationGraphEvent& evn, BSTEventSource<BSAnimationGraphEvent>* src) {
		if (!PUBGMode) {
			if (evn.animEvent == std::string("sightedStateEnter")) {
				SightEnter();
			}
			else if (aiming) {
				if ((p->gunState != 6 && p->gunState != 8) && *ptr_engineTime - timer_sightEnter > 5000) {
					SightExit();
				}
			}
		}
		else {
			if (aiming) {
				if (p->gunState == 0x4) { //Reloading
					SightExit();
				}
				else if (p->gunState == 0 && pcam->currentState == pcam->cameraStates[CameraState::kFirstPerson]) {
					if (p->currentProcess) {
						p->SetInIronSightsImpl(false);
						p->currentProcess->PlayIdle(p, 0x35, FPSighted);
					}
				}
			}
		}
		if (aiming) {
			if (*ptr_engineTime - timer_sightEnter < 10000) {
				pcam->fovAdjustCurrent = pcam->fovAdjustTarget;
				*(float*)((uintptr_t)pcam->cameraStates[CameraState::kFirstPerson].get() + 0x78) = 1.0f;
			}
			if (p->gunState == 0x7) { //Grenade throw?
				SightExit();
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
	static unordered_map<uint64_t, FnProcessEvent> fnHash;
};
unordered_map<uint64_t, AnimationGraphEventWatcher::FnProcessEvent> AnimationGraphEventWatcher::fnHash;

class InputEventReceiverOverride : public BSInputEventReceiver {
public:
	typedef void (InputEventReceiverOverride::* FnPerformInputProcessing)(const InputEvent* a_queueHead);

	void ProcessButtonEvent(const ButtonEvent* evn) {
		if (waitSprint && p->moveMode & 32 && *ptr_engineTime - timer_sightExit > 5000) {
			pcam->SetState(pcam->cameraStates[CameraState::k3rdPerson].get());
			waitSprint = false;
		}

		if (evn->eventType != INPUT_EVENT_TYPE::kButton || (evn->heldDownSecs > tapTimeout && !aiming)) {
			if (evn->next)
				ProcessButtonEvent((ButtonEvent*)evn->next);
			return;
		}

		uint32_t id = evn->idCode;
		if (evn->device == INPUT_DEVICE::kMouse)
			id += 256;

		uint32_t zoomKey = defaultZoomKey;
		uint32_t sprintKey = defaultSprintKey;
		if (!performanceMode) {
			ControlMap* cm = ControlMap::GetSingleton();
			if (cm) {
				BSTArray<ControlMap::UserEventMapping>& mouseMap =
					cm->controlMaps[(int)UserEvents::INPUT_CONTEXT_ID::kMainGameplay]->deviceMappings[(int)INPUT_DEVICE::kMouse];
				for (auto it = mouseMap.begin(); it != mouseMap.end(); ++it) {
					if (it->inputKey != 255) {
						if (it->eventID == std::string("SecondaryAttack")) {
							zoomKey = it->inputKey + 256;
						}
						else if (it->eventID == std::string("Sprint")) {
							sprintKey = it->inputKey + 256;
						}
					}
				}
				BSTArray<ControlMap::UserEventMapping>& keyboardMap =
					cm->controlMaps[(int)UserEvents::INPUT_CONTEXT_ID::kMainGameplay]->deviceMappings[(int)INPUT_DEVICE::kKeyboard];
				for (auto it = keyboardMap.begin(); it != keyboardMap.end(); ++it) {
					if (it->inputKey != 255) {
						if (it->eventID == std::string("SecondaryAttack")) {
							zoomKey = it->inputKey;
						}
						else if (it->eventID == std::string("Sprint")) {
							sprintKey = it->inputKey;
						}
					}
				}
			}
		}

		if (!aiming) {
			if (id == zoomKey && evn->value == 0) {
				SightEnter();
			}
		}
		else {
			if ((id == zoomKey && evn->value == 0) || id == sprintKey) {
				if (pcam->currentState == pcam->cameraStates[CameraState::kFirstPerson]) {
					if (id == sprintKey) {
						waitSprint = true;
						SightExit(false);
					}
					else {
						SightExit();
					}
				}
			}
		}

		if (evn->next)
			ProcessButtonEvent((ButtonEvent*)evn->next);
	}

	void HookedPerformInputProcessing(const InputEvent* a_queueHead) {
		if (PUBGMode && !UI::GetSingleton()->menuMode && a_queueHead) {
			ProcessButtonEvent((ButtonEvent*)a_queueHead);
		}
		FnPerformInputProcessing fn = fnHash.at(*(uint64_t*)this);
		if (fn) {
			(this->*fn)(a_queueHead);
		}
	}

	void HookSink() {
		uint64_t vtable = *(uint64_t*)this;
		auto it = fnHash.find(vtable);
		if (it == fnHash.end()) {
			FnPerformInputProcessing fn = SafeWrite64Function(vtable, &InputEventReceiverOverride::HookedPerformInputProcessing);
			fnHash.insert(std::pair<uint64_t, FnPerformInputProcessing>(vtable, fn));
		}
	}

	void UnHookSink() {
		uint64_t vtable = *(uint64_t*)this;
		auto it = fnHash.find(vtable);
		if (it == fnHash.end())
			return;
		SafeWrite64Function(vtable, it->second);
		fnHash.erase(it);
	}

protected:
	static unordered_map<uint64_t, FnPerformInputProcessing> fnHash;
};
unordered_map<uint64_t, InputEventReceiverOverride::FnPerformInputProcessing> InputEventReceiverOverride::fnHash;

void InitializePlugin() {
	p = PlayerCharacter::GetSingleton();
	((AnimationGraphEventWatcher*)((uint64_t)p + 0x38))->HookSink();
	pcam = PlayerCamera::GetSingleton();
	((InputEventReceiverOverride*)((uint64_t)pcam + 0x38))->HookSink();
	_MESSAGE("PlayerCharacter %llx", p);
	_MESSAGE("PlayerCamera %llx", pcam);
	for (auto it = INISettingCollection::GetSingleton()->settings.begin(); it != INISettingCollection::GetSingleton()->settings.end(); ++it) {
		if (strcmp((*it)->_key, "f3rdPersonAimFOV:Camera") == 0) {
			TPAimFOV = *it;
			_MESSAGE("Setting %s", (*it)->_key);
		}
		else if (strcmp((*it)->_key, "fDefault1stPersonFOV:Display") == 0) {
			FPFOV = *it;
			_MESSAGE("Setting %s", (*it)->_key);
		}
	}
	FPSighted = (TESIdleForm*)TESForm::GetFormByID(0x4D32);
}

void LoadConfigs() {
	ini.LoadFile("Data\\F4SE\\Plugins\\PUBGAiming.ini");
	PUBGMode = std::stoi(ini.GetValue("General", "PUBGMode", "1")) > 0;
	tapTimeout = std::stof(ini.GetValue("General", "TapTimeout", "0.12"));
	performanceMode = std::stoi(ini.GetValue("General", "PerformanceMode", "1")) > 0;
	defaultZoomKey = std::stoi(ini.GetValue("General", "DefaultZoomKey", "0x101"), 0, 16);
	defaultSprintKey = std::stoi(ini.GetValue("General", "DefaultSprintKey", "0xa0"), 0, 16);
}

void ResetValues() {
	aiming = false;
	waitSprint = false;
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
			ResetValues();
			LoadConfigs();
		}
	});

	return true;
}
