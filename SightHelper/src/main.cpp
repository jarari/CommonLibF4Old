#include <Windows.h>
#include <unordered_map>
#include <vector>
#include "MathUtils.h"
using namespace RE;
using std::unordered_map;
using std::vector;

char tempbuf[8192] = { 0 };
char* _MESSAGE(const char* fmt, ...) {
	va_list args;

	va_start(args, fmt);
	vsnprintf(tempbuf, sizeof(tempbuf), fmt, args);
	va_end(args);
	spdlog::log(spdlog::level::warn, tempbuf);

	return tempbuf;
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

bool IsInADS(Actor* a) {
	return (a->gunState == 0x8 || a->gunState == 0x6);
}

float GetActorScale(Actor* a) {
	typedef float (*_GetPlayerScale)(Actor*);
	REL::Relocation<_GetPlayerScale> func{ REL::ID(911188) };
	return func(a);
}

PlayerCharacter* p;
PlayerCamera* pcam;
ConsoleLog* clog;
NiPoint3 lastCalculated;
vector<std::string> helperNodeList = { "_SightHelper", "ReticleNode" };

void PrintConsole(const char* c) {
	clog->AddString("(Sight Helper) ");
	clog->AddString(c);
	clog->AddString("\n");
}

void GetEquippedWeaponMods(TESObjectWEAP* currwep) {
	if (!p->inventoryList) {
		return;
	}
	for (auto invitem = p->inventoryList->data.begin(); invitem != p->inventoryList->data.end(); ++invitem) {
		if (invitem->object->formType == ENUM_FORM_ID::kWEAP) {
			TESObjectWEAP* wep = (TESObjectWEAP * )(invitem->object);
			if (invitem->stackData->IsEquipped() && wep == currwep) {
				if (invitem->stackData->extra) {
					BGSObjectInstanceExtra* extraData = (BGSObjectInstanceExtra * )invitem->stackData->extra->GetByType(EXTRA_DATA_TYPE::kObjectInstance);
					if (extraData) {
						auto data = extraData->values;
						if (data && data->buffer) {
							uintptr_t buf = (uintptr_t)(data->buffer);
							for (uint32_t i = 0; i < data->size / 0x8; i++) {
								BGSMod::Attachment::Mod* omod = (BGSMod::Attachment::Mod * )TESForm::GetFormByID(*(uint32_t*)(buf + i * 0x8));
								_MESSAGE("Mod : %s", omod->fullName.c_str());
							}
						}
					}
				}
			}
		}
	}
}

void CalculateZoomData() {
	NiNode* node = (NiNode*)p->Get3D(true);
	if (node) {
		NiNode* helper = nullptr;
		int i = 0;
		while (helper == nullptr && i < helperNodeList.size()) {
			helper = (NiNode*)node->GetObjectByName(helperNodeList.at(i));
			++i;
		}
		NiNode* camera = (NiNode*)node->GetObjectByName("Camera");
		bhkCharacterController* con = nullptr;
		if (p->currentProcess) {
			con = p->currentProcess->middleHigh->charController.get();
		}
		if (helper && camera && con) {
			NiPoint3 pos, dir;
			p->GetEyeVector(pos, dir, true);
			NiPoint3 right = Normalize(CrossProduct(con->forwardVec, con->up));
			NiPoint3 up = Normalize(CrossProduct(right, dir));
			NiPoint3 diff = helper->world.translate - camera->world.translate;
			diff = diff - Normalize(dir) * DotProduct(diff, dir);
			float x = - DotProduct(diff, right);
			float z = - DotProduct(diff, up);
			lastCalculated = NiPoint3(x, 0, z);
			NiPoint3 zoomData = NiPoint3();
			if (p->currentProcess && p->currentProcess->middleHigh) {
				BSTArray<EquippedItem> equipped = p->currentProcess->middleHigh->equippedItems;
				if (equipped.size() != 0 && equipped[0].item.instanceData) {
					TESObjectWEAP* weap = (TESObjectWEAP* )equipped[0].item.object;
					TESObjectWEAP::InstanceData* instance = (TESObjectWEAP::InstanceData*)equipped[0].item.instanceData.get();
					if (instance->type == 9) {
						_MESSAGE("Weapon : %s", weap->fullName.c_str());
						GetEquippedWeaponMods(weap);
						if (instance->zoomData) {
							zoomData = instance->zoomData->zoomData.cameraOffset;
						}
					}
				}
			}
			char consolebuf[1024] = { 0 };
			snprintf(consolebuf, sizeof(consolebuf), "Calculated Difference %f %f", x, z);
			_MESSAGE(consolebuf); 
			PrintConsole(consolebuf);
			snprintf(consolebuf, sizeof(consolebuf), "Current Zoom Data %f %f", zoomData.x, zoomData.z);
			_MESSAGE(consolebuf);
			PrintConsole(consolebuf);
		}
		else {
			PrintConsole("Helper node not found.");
		}
	}
}

void ApplyZoomData() {
	bool zoomDataFound = false;
	if (p->currentProcess && p->currentProcess->middleHigh) {
		BSTArray<EquippedItem> equipped = p->currentProcess->middleHigh->equippedItems;
		if (equipped.size() != 0 && equipped[0].item.instanceData) {
			TESObjectWEAP::InstanceData* instance = (TESObjectWEAP::InstanceData*)equipped[0].item.instanceData.get();
			if (instance->type == 9 && instance->zoomData) {
				zoomDataFound = true;
				instance->zoomData->zoomData.cameraOffset = lastCalculated;
				PrintConsole("Applied new zoom data.");
			}
		}
	}
	if (!zoomDataFound) {
		PrintConsole("Current weapon has no zoom data.");
	}
}

class SightHelperInputHandler : public BSInputEventReceiver {
public:
	typedef void (SightHelperInputHandler::* FnPerformInputProcessing)(const InputEvent* a_queueHead);

	void HandleMultipleButtonEvent(const ButtonEvent* evn) {
		if (evn->eventType != INPUT_EVENT_TYPE::kButton) {
			if (evn->next)
				HandleMultipleButtonEvent((ButtonEvent*)evn->next);
			return;
		}
		uint32_t id = evn->idCode;
		if (evn->device == INPUT_DEVICE::kMouse)
			id += 256;

		if (pcam->currentState == pcam->cameraStates[CameraStates::kFirstPerson] && evn->heldDownSecs == 0 && evn->value == 1) {
			if (id == 0x6D) {
				if (IsInADS(p)) {
					if (GetActorScale(p) == 1) {
						CalculateZoomData();
					}
					else {
						PrintConsole("Please change player scale to 1.");
					}
				}
				else {
					PrintConsole("You must ADS first.");
				}
			}
			else if (id == 0x6B) {
				ApplyZoomData();
			}
		}

		if (evn->next)
			HandleMultipleButtonEvent((ButtonEvent*)evn->next);
	}

	void HookedPerformInputProcessing(const InputEvent* a_queueHead) {
		if (a_queueHead) {
			HandleMultipleButtonEvent((ButtonEvent*)a_queueHead);
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
			FnPerformInputProcessing fn = SafeWrite64Function(vtable, &SightHelperInputHandler::HookedPerformInputProcessing);
			fnHash.insert(std::pair<uint64_t, FnPerformInputProcessing>(vtable, fn));
		}
	}

protected:
	static unordered_map<uint64_t, FnPerformInputProcessing> fnHash;
};
unordered_map<uint64_t, SightHelperInputHandler::FnPerformInputProcessing> SightHelperInputHandler::fnHash;

void InitializePlugin() {
	p = PlayerCharacter::GetSingleton();
	pcam = PlayerCamera::GetSingleton();
	((SightHelperInputHandler*)((uint64_t)pcam + 0x38))->HookSink();
	clog = ConsoleLog::GetSingleton();
	_MESSAGE("PlayerCharacter %llx", p);
	_MESSAGE("PlayerCamera %llx", pcam);
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
		}
	});
	return true;
}
