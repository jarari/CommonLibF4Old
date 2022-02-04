#include <Windows.h>
#include <unordered_map>
#include <vector>
#include "MathUtils.h"
#include "Havok.h"
#define MATH_PI 3.14159265358979323846
#define FO4TOHAVOK 0.0142875f
using namespace RE;
using std::unordered_map;
using std::vector;
using namespace RE;

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

TESForm* GetFormFromMod(std::string modname, uint32_t formid) {
	if (!modname.length() || !formid)
		return nullptr;
	TESDataHandler* dh = TESDataHandler::GetSingleton();
	TESFile* modFile = nullptr;
	for (auto it = dh->files.begin(); it != dh->files.end(); ++it) {
		TESFile* f = *it;
		if (strcmp(f->filename, modname.c_str()) == 0) {
			modFile = f;
			break;
		}
	}
	if (!modFile)
		return nullptr;
	uint8_t modIndex = modFile->compileIndex;
	uint32_t id = formid;
	if (modIndex < 0xFE) {
		id |= ((uint32_t)modIndex) << 24;
	}
	else {
		uint16_t lightModIndex = modFile->smallFileCompileIndex;
		if (lightModIndex != 0xFFFF) {
			id |= 0xFE000000 | (uint32_t(lightModIndex) << 12);
		}
	}
	return TESForm::GetFormByID(id);
}

PlayerCharacter* p;
PlayerCamera* pcam;
PlayerControls* pc; 
float toRad = (float)(MATH_PI / 180.0);
bool isDriving = false;
TESObjectREFR* vehicle;
TESIdleForm* driveIdle;
float maxSpeed = 45.0f;
NiPoint3 controlDir = NiPoint3();

bool moveup = false;
int move = 0;
int turn = 0;

void EnterVehicle(std::monostate, TESObjectREFR* v) {
	_MESSAGE("Vehicle entered %llx", v);
	vehicle = v;
	isDriving = true;
	AIProcess* process = p->currentProcess;
	if (process) {
		process->PlayIdle(p, 0x35, driveIdle);
		NiPointer<bhkCharacterController> con = p->currentProcess->middleHigh->charController;
		con->context.currentState = hknpCharacterState::hknpCharacterStateType::kFlying;
		pcam->SetState(pcam->cameraStates[CameraState::kFree].get());
		*(bool*)((uintptr_t)pc->movementHandler + 0x8) = false;
		*(bool*)((uintptr_t)pc->readyWeaponHandler + 0x8) = false;
		*(bool*)((uintptr_t)pc->jumpHandler + 0x8) = false;
		*(bool*)((uintptr_t)pc->attackHandler + 0x8) = false;
		*(bool*)((uintptr_t)pc->sneakHandler + 0x8) = false;
		*(bool*)((uintptr_t)pc->toggleRunHandler + 0x8) = false;
	}
}

void ExitVehicle(std::monostate) {
}

class CameraUpdateHook {
public:
	typedef void (CameraUpdateHook::* FnUpdate)(BSTSmartPointer<TESCameraState>& a_nextState);

	void Update(BSTSmartPointer<TESCameraState>& a_nextState) {
		if (!isDriving)
			return;

		if (pcam->currentState != pcam->cameraStates[CameraState::kFree]) {
			pcam->SetState(pcam->cameraStates[CameraState::kFree].get());
		}

		NiNode* node = (NiNode*)vehicle->Get3D();
		if (node && node->collisionObject) {

			if (p->currentProcess) {
				bhkCharacterController* con = p->currentProcess->middleHigh->charController.get();
				if (con) {
					uintptr_t charProxy = *(uintptr_t*)((uintptr_t)con + 0x470);
					if (charProxy) {
						hkTransform* charProxyTransform = (hkTransform*)(charProxy + 0x40);
						charProxyTransform->m_translation = vehicle->data.location * FO4TOHAVOK;
					}
					NiPoint3 right = Normalize(CrossProduct(con->forwardVec, con->up));
					//NiPoint3 pos, eyeDir;
					//p->GetEyeVector(pos, eyeDir, true);
					NiPoint3 eyeDir = ToUpVector(pcam->cameraRoot->world.rotate);
					//pcam->currentState->camera->cameraRoot->world.translate = vehicle->data.location + eyeDir * -500.0f;
					*(NiPoint3*)((uintptr_t)pcam->cameraStates[CameraState::kFree].get() + 0x28) = vehicle->data.location + eyeDir * -500.0f;

					controlDir = eyeDir * maxSpeed;
					hknpWorld* world = ((bhkNPCollisionObject*)node->collisionObject.get())->spSystem->physicsSystem->m_world;
					if (move != 0) {
						ImpulseData id = ImpulseData(eyeDir * (float)move, maxSpeed, world);
						ApplyHavokImpulse(node, id);
					}
					if (moveup) {
						ImpulseData id = ImpulseData(NiPoint3(0, 0, 1), 400.0f, world);
						ApplyHavokImpulse(node, id);
					}
				}
			}
		}
		FnUpdate fn = fnHash.at(*(uint64_t*)this + 0x58);
		if (fn) {
			(this->*fn)(a_nextState);
		}
	}

	void HookSink() {
		uint64_t vtable = *(uint64_t*)this + 0x58;
		auto it = fnHash.find(vtable);
		if (it == fnHash.end()) {
			FnUpdate fn = SafeWrite64Function(vtable, &CameraUpdateHook::Update);
			fnHash.insert(std::pair<uint64_t, FnUpdate>(vtable, fn));
		}
	}

protected:
	static unordered_map<uint64_t, FnUpdate> fnHash;
};
unordered_map<uint64_t, CameraUpdateHook::FnUpdate> CameraUpdateHook::fnHash;

class VehicleInputHandler {
public:
	typedef void (VehicleInputHandler::* FnPerformInputProcessing)(const InputEvent* a_queueHead);

	void HandleMultipleButtonEvent(const ButtonEvent* evn) {
		if (!isDriving)
			return;

		if (evn->eventType != INPUT_EVENT_TYPE::kButton) {
			if (evn->next)
				HandleMultipleButtonEvent((ButtonEvent*)evn->next);
			return;
		}
		uint32_t id = evn->idCode;
		if (evn->device == INPUT_DEVICE::kMouse)
			id += 256;

		if (evn->value) {
			if (evn->idCode == 0x57) {
				move = 1;
			}
			else if (evn->idCode == 0x53) {
				move = -1;
			}
			else if (evn->idCode == 0x20) {
				moveup = true;
			}
		}
		else {
			if (evn->idCode == 0x57) {
				if (move == 1)
					move = 0;
			}
			else if (evn->idCode == 0x53) {
				if (move == -1)
					move = 0;
			}
			else if (evn->idCode == 0x20) {
				moveup = false;
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
			FnPerformInputProcessing fn = SafeWrite64Function(vtable, &VehicleInputHandler::HookedPerformInputProcessing);
			fnHash.insert(std::pair<uint64_t, FnPerformInputProcessing>(vtable, fn));
		}
	}

protected:
	static unordered_map<uint64_t, FnPerformInputProcessing> fnHash;
};
unordered_map<uint64_t, VehicleInputHandler::FnPerformInputProcessing> VehicleInputHandler::fnHash;

bool RegisterFuncs(BSScript::IVirtualMachine* a_vm) {
	a_vm->BindNativeMethod("NFSFallout", "EnterVehicle", EnterVehicle, false);
	a_vm->BindNativeMethod("NFSFallout", "ExitVehicle", ExitVehicle, false);
	return true;
}

void InitializePlugin() {
	p = PlayerCharacter::GetSingleton();
	pc = PlayerControls::GetSingleton();
	pcam = PlayerCamera::GetSingleton();
	((CameraUpdateHook*)pcam->cameraStates[CameraState::kFree].get())->HookSink();
	((VehicleInputHandler*)((uint64_t)pcam + 0x38))->HookSink();
	driveIdle = (TESIdleForm*)TESForm::GetFormByID(0x20AC3C);
	_MESSAGE("PlayerCharacter %llx", p);
	_MESSAGE("PlayerControls %llx", pc);
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
	const F4SE::PapyrusInterface* papyrus = F4SE::GetPapyrusInterface();
	bool succ = papyrus->Register(RegisterFuncs);
	if (succ) {
		logger::warn("Papyrus functions registered."sv);
	}
	const F4SE::MessagingInterface* message = F4SE::GetMessagingInterface();
	message->RegisterListener([](F4SE::MessagingInterface::Message* msg) -> void {
		if (msg->type == F4SE::MessagingInterface::kGameDataReady) {
			InitializePlugin();
		}
	});

	return true;
}
