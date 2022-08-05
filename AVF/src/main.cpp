#include <Windows.h>
#include <unordered_map>
#include <vector>
#include <MathUtils.h>
#include <Havok.h>
#define HAVOKtoFO4 69.99124f
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

class VelocityData {
public:
	float x, y, z, duration, stepX, stepY, stepZ, lastRun;
	bool gravity = false;
	bool additive = false;
	const static float stepTime;
	VelocityData() {
		x = 0.0f;
		y = 0.0f;
		z = 0.0f;
		duration = 0.0f;
		stepX = 0.0f;
		stepY = 0.0f;
		stepZ = 0.0f;
	}
};

REL::Relocation<float*> ptr_engineTime{ REL::ID(599343) };
const float VelocityData::stepTime = 0.016667f;
unordered_map<Actor*, VelocityData> velMap;
unordered_map<Actor*, VelocityData> queueMap;
PlayerCharacter* p;
BSSpinLock mapLock;
Setting* charGravity;

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

bool IsOnGround(bhkCharacterController* con) {
	return (con->flags & 0x100) == 0x100 || con->context.currentState == hknpCharacterState::hknpCharacterStateType::kOnGround;
}

bool IsOnGround(Actor* a) {
	if (!a->currentProcess || !a->currentProcess->middleHigh)
		return false;
	NiPointer<bhkCharacterController> con = a->currentProcess->middleHigh->charController;
	if (!con.get())
		return false;
	return IsOnGround(con.get());
}

float ApplyVelocity(Actor* a, VelocityData& vd, bool modifyState = false) {
	if (!a->currentProcess || !a->currentProcess->middleHigh)
		return 0;
	NiPointer<bhkCharacterController> con = a->currentProcess->middleHigh->charController;
	if (!con.get())
		return 0;
	float deltaTime = *ptr_engineTime - vd.lastRun;
	if (vd.additive) {
		uintptr_t charProxy = *(uintptr_t*)((uintptr_t)con.get() + 0x470);
		if (charProxy) {
			con->context.currentState = hknpCharacterState::hknpCharacterStateType::kInAir;
			//con->velocityMod = hkVector4f(vd.x, vd.y, vd.z) * HAVOKtoFO4;
			if (IsOnGround(a)) {
				hkTransform* charProxyTransform = (hkTransform*)(charProxy + 0x40);
				charProxyTransform->m_translation.z += 0.1f;
			}
			else {
				hkVector4f* charProxyVel = (hkVector4f*)(charProxy + 0xA0);
				charProxyVel->x += vd.x;
				charProxyVel->y += vd.y;
				charProxyVel->z += vd.z;
				vd.x = 0;
				vd.y = 0;
				vd.z = 0;
				con->flags &= 0xFFFFF8FF;
			}
			//_MESSAGE("Actor %llx Controller %llx x %f y % f z %f onGround %d", a, con.get(), vd.x, vd.y, vd.z, (con->flags & 0x100) == 0x100);
		}
	}
	else {
		if (vd.gravity) {
			if (!IsOnGround(con.get())) {
				vd.z -= con->gravity * charGravity->GetFloat() * 9.81f * deltaTime / VelocityData::stepTime;
			}
		}
		//_MESSAGE("Actor %llx Controller %llx x %f y % f z %f onGround %d", a, con.get(), vd.x, vd.y, vd.z, (con->flags & 0x100) == 0x100);
		con->velocityMod = hkVector4f(vd.x, vd.y, vd.z);
		con->velocityTime = con->stepInfo.deltaTime.storage;
		vd.x += vd.stepX * deltaTime / VelocityData::stepTime;
		vd.y += vd.stepY * deltaTime / VelocityData::stepTime;
		vd.z += vd.stepZ * deltaTime / VelocityData::stepTime;
		con->flags = con->flags & ~((uint32_t)0xFF00) | (uint32_t)0x8700;
		if (modifyState) {
			con->context.currentState = hknpCharacterState::hknpCharacterStateType::kInAir;
		}
		else {
			if (con->context.currentState == hknpCharacterState::hknpCharacterStateType::kOnGround) {
				uintptr_t charProxy = *(uintptr_t*)((uintptr_t)con.get() + 0x470);
				if (charProxy) {
					hkTransform* charProxyTransform = (hkTransform*)(charProxy + 0x40);
					charProxyTransform->m_translation.z += 0.1f;
				}
			}
		}
	}
	vd.duration -= deltaTime;

	vd.lastRun = *ptr_engineTime;
	return deltaTime;
}

class CharacterMoveEventWatcher {
public:
	typedef BSEventNotifyControl (CharacterMoveEventWatcher::* FnProcessEvent)(bhkCharacterMoveFinishEvent& evn, BSTEventSource<bhkCharacterMoveFinishEvent>* dispatcher);

	BSEventNotifyControl HookedProcessEvent(bhkCharacterMoveFinishEvent& evn, BSTEventSource<bhkCharacterMoveFinishEvent>* src) {
		Actor* a = (Actor*)((uintptr_t)this - 0x150);
		if (a && a->loadedData) {
			mapLock.lock();
			auto result = velMap.find(a);
			if (result != velMap.end()) {
				if (a->Get3D() && !a->IsDead(true)) {
					VelocityData& data = result->second;
					auto qresult = queueMap.find(a);
					if (qresult != queueMap.end()) {
						VelocityData& queueData = qresult->second;
						if (queueData.additive) {
							data.x += queueData.x;
							data.y += queueData.y;
							data.z += queueData.z;
						}
						else {
							data.x = queueData.x;
							data.y = queueData.y;
							data.z = queueData.z;
						}
						data.duration = queueData.duration;
						data.stepX = queueData.stepX;
						data.stepY = queueData.stepY;
						data.stepZ = queueData.stepZ;
						queueMap.erase(a);
					}
					if (data.duration <= 0) {
						int32_t iSyncJumpState = 0;
						a->GetGraphVariableImplInt("iSyncJumpState", iSyncJumpState);
						if (iSyncJumpState > 0) {
							if (IsOnGround(a)) {
								a->NotifyAnimationGraphImpl("jumpLand");
							}
							else {
								NiPointer<bhkCharacterController> con = a->currentProcess->middleHigh->charController;
								con->context.currentState = hknpCharacterState::hknpCharacterStateType::kInAir;
							}
						}
						velMap.erase(result);
					}
					else {
						ApplyVelocity(a, data, true);
					}
				}
				else {
					logger::warn(_MESSAGE("Actor %llx is dead or not loaded", a));
					velMap.erase(a);
				}
			}
			mapLock.unlock();
		}
		FnProcessEvent fn = fnHash.at(*(uint64_t*)this);
		return fn ? (this->*fn)(evn, src) : BSEventNotifyControl::kContinue;
	}

	void HookSink(uintptr_t vtable) {
		auto it = fnHash.find(vtable);
		if (it == fnHash.end()) {
			FnProcessEvent fn = SafeWrite64Function(vtable + 0x8, &CharacterMoveEventWatcher::HookedProcessEvent);
			fnHash.insert(std::pair<uint64_t, FnProcessEvent>(vtable, fn));
		}
	}

	void UnHookSink(uintptr_t vtable) {
		auto it = fnHash.find(vtable);
		if (it == fnHash.end())
			return;
		SafeWrite64Function(vtable + 0x8, it->second);
		fnHash.erase(it);
	}

	F4_HEAP_REDEFINE_NEW(CharacterMoveEventWatcher);
protected:
	static unordered_map<uint64_t, FnProcessEvent> fnHash;
};
unordered_map<uint64_t, CharacterMoveEventWatcher::FnProcessEvent> CharacterMoveEventWatcher::fnHash;

std::vector<float> GetActorForward(std::monostate, Actor* a) {
	std::vector<float> result = std::vector<float>({ 0, 0, 0 });
	if (!a->currentProcess || !a->currentProcess->middleHigh)
		return result;
	NiPointer<bhkCharacterController> con = a->currentProcess->middleHigh->charController;
	if (con.get()) {
		hkVector4f fwd = con->forwardVec;
		fwd.Normalize();
		result[0] = -fwd.x;
		result[1] = -fwd.y;
		result[2] = fwd.z;
	}
	return result;
}

std::vector<float> GetActorUp(std::monostate, Actor* a) {
	std::vector<float> result = std::vector<float>({ 0, 0, 0 });
	if (!a->currentProcess || !a->currentProcess->middleHigh)
		return result;
	NiPointer<bhkCharacterController> con = a->currentProcess->middleHigh->charController;
	if (con.get()) {
		hkVector4f up = con->up;
		up.Normalize();
		result[0] = up.x;
		result[1] = up.y;
		result[2] = up.z;
	}
	return result;
}

std::vector<float> GetActorEye(std::monostate, Actor* a, bool includeCamOffset) {
	std::vector<float> result = std::vector<float>({ 0, 0, 0 });
	NiPoint3 pos, dir;
	a->GetEyeVector(pos, dir, includeCamOffset);
	result[0] = dir.x;
	result[1] = dir.y;
	result[2] = dir.z;
	return result;
}

std::vector<float> GetActorVelocity(std::monostate, Actor* a) {
	std::vector<float> result = std::vector<float>({ 0, 0, 0 });
	if (!a->currentProcess || !a->currentProcess->middleHigh)
		return result;
	NiPointer<bhkCharacterController> con = a->currentProcess->middleHigh->charController;
	if (con.get()) {
		result[0] = con->velocityMod.x;
		result[1] = con->velocityMod.x;
		result[2] = con->velocityMod.z;
	}
	return result;
}

extern "C" DLLEXPORT void F4SEAPI SetVelocity(std::monostate, Actor * a, float x, float y, float z, float dur, float x2, float y2, float z2, bool grav = false) {
	if (!a || !a->Get3D())
		return;
	//logger::warn(_MESSAGE("SetVelocity %llx %f %f %f %f", a, x, y, z, dur));
	mapLock.lock();
	auto it = velMap.find(a);
	if (it != velMap.end()) {
		//logger::warn(_MESSAGE("Actor found on the map. Inserting queue"));
		VelocityData data = it->second;
		data.x = x;
		data.y = y;
		data.z = z;
		data.duration = dur;
		data.stepX = (x2 - x) / (dur / VelocityData::stepTime);
		data.stepY = (y2 - y) / (dur / VelocityData::stepTime);
		data.stepZ = (z2 - z) / (dur / VelocityData::stepTime);
		data.gravity = grav;
		data.lastRun = *ptr_engineTime;
		queueMap.insert(std::pair<Actor*, VelocityData>(a, data));
	}
	else {
		//logger::warn(_MESSAGE("Actor not found on the map. Creating data"));
		VelocityData data = VelocityData();
		data.x = x;
		data.y = y;
		data.z = z;
		data.duration = dur;
		data.stepX = (x2 - x) / (dur / VelocityData::stepTime);
		data.stepY = (y2 - y) / (dur / VelocityData::stepTime);
		data.stepZ = (z2 - z) / (dur / VelocityData::stepTime);
		data.gravity = grav;
		data.lastRun = *ptr_engineTime;
		velMap.insert(std::pair<Actor*, VelocityData>(a, data));
	}
	mapLock.unlock();
}

extern "C" DLLEXPORT void F4SEAPI AddVelocity(std::monostate, Actor * a, float x, float y, float z) {
	if (!a || !a->Get3D())
		return;
	mapLock.lock();
	auto it = velMap.find(a);
	if (it != velMap.end()) {
		//logger::warn(_MESSAGE("Actor found on the map. Inserting queue"));
		VelocityData data = it->second;
		data.x = x / HAVOKtoFO4;
		data.y = y / HAVOKtoFO4;
		data.z = z / HAVOKtoFO4;
		data.duration = 0.1f;
		data.additive = true;
		data.lastRun = *ptr_engineTime;
		queueMap.insert(std::pair<Actor*, VelocityData>(a, data));
	}
	else {
		//logger::warn(_MESSAGE("Actor not found on the map. Creating data"));
		VelocityData data = VelocityData();
		data.x = x / HAVOKtoFO4;
		data.y = y / HAVOKtoFO4;
		data.z = z / HAVOKtoFO4;
		data.duration = 0.1f;
		data.additive = true;
		data.lastRun = *ptr_engineTime;
		velMap.insert(std::pair<Actor*, VelocityData>(a, data));
	}
	mapLock.unlock();
}

bool IsOnGroundPapyrus(std::monostate, Actor* a) {
	return IsOnGround(a);
}

void SetPositionQuick(std::monostate, Actor* a, float x, float y, float z) {
	if (a) {
		if (a->currentProcess) {
			bhkCharacterController* con = a->currentProcess->middleHigh->charController.get();
			if (con) {
				uintptr_t charProxy = *(uintptr_t*)((uintptr_t)con + 0x470);
				if (charProxy) {
					hkTransform* charProxyTransform = (hkTransform*)(charProxy + 0x40);
					charProxyTransform->m_translation.x = x / HAVOKtoFO4;
					charProxyTransform->m_translation.y = y / HAVOKtoFO4;
					charProxyTransform->m_translation.z = z / HAVOKtoFO4;
					//logger::warn(_MESSAGE("(SetPositionQuick) Orig %f %f %f Havok %f %f %f", x, y, z, charProxyTransform->m_translation.x, charProxyTransform->m_translation.y, charProxyTransform->m_translation.z));
				}
			}
		}
		else {
			logger::warn(_MESSAGE("(SetPositionQuick) Failed to run on Actor %llx", a->formID));
		}
	}
	else {
		logger::warn(_MESSAGE("(SetPositionQuick) Actor is null"));
	}
}

std::vector<float> GetActorAngle(std::monostate, Actor* a) {
	std::vector<float> result = std::vector<float>({ 0, 0, 0 });
	result[0] = a->data.angle.x;
	result[1] = a->data.angle.y;
	result[2] = a->data.angle.z;
	return result;
}

void SetAngleQuick(std::monostate, Actor* a, float x, float y, float z) {
	if (a) {
		if (a->Get3D()) {
			a->data.angle.x = x;
			a->data.angle.y = y;
			a->data.angle.z = z;
		}
		else {
			logger::warn(_MESSAGE("(SetAngleQuick) Failed to run on Actor %llx", a->formID));
		}
	}
	else {
		logger::warn(_MESSAGE("(SetAngleQuick) Actor is null"));
	}
}

void VelocityMapCleanup() {
	mapLock.lock();
	velMap.clear();
	queueMap.clear();
	mapLock.unlock();
}

bool RegisterFuncs(BSScript::IVirtualMachine* a_vm) {
	a_vm->BindNativeMethod("ActorVelocityFramework", "GetActorForward", GetActorForward, false);
	a_vm->BindNativeMethod("ActorVelocityFramework", "GetActorUp", GetActorUp, false);
	a_vm->BindNativeMethod("ActorVelocityFramework", "GetActorEye", GetActorEye, false);
	a_vm->BindNativeMethod("ActorVelocityFramework", "GetActorAngle", GetActorAngle, false);
	a_vm->BindNativeMethod("ActorVelocityFramework", "GetActorVelocity", GetActorVelocity, false);
	a_vm->BindNativeMethod("ActorVelocityFramework", "SetPositionQuick", SetPositionQuick, false);
	a_vm->BindNativeMethod("ActorVelocityFramework", "SetAngleQuick", SetAngleQuick, false);
	a_vm->BindNativeMethod("ActorVelocityFramework", "SetVelocity", SetVelocity, true);
	a_vm->BindNativeMethod("ActorVelocityFramework", "AddVelocity", AddVelocity, true);
	a_vm->BindNativeMethod("ActorVelocityFramework", "IsOnGround", IsOnGroundPapyrus, false);
	return true;
}

void InitializePlugin() {
	uint64_t PCVtable = REL::Relocation<uint64_t>{PlayerCharacter::VTABLE[13]}.address();
	uint64_t ActorVtable = REL::Relocation<uint64_t>{ Actor::VTABLE[13] }.address();
	CharacterMoveEventWatcher* PCWatcher = new CharacterMoveEventWatcher();
	CharacterMoveEventWatcher* ActorWatcher = new CharacterMoveEventWatcher();
	PCWatcher->HookSink(PCVtable);
	ActorWatcher->HookSink(ActorVtable);
	p = PlayerCharacter::GetSingleton();
	//WorldUpdateWatcher* world = (WorldUpdateWatcher * )(**ptr_bhkWorldM);
	//world->HookSink();
	for (auto it = INISettingCollection::GetSingleton()->settings.begin(); it != INIPrefSettingCollection::GetSingleton()->settings.end(); ++it) {
		if (strcmp((*it)->_key, "fInAirFallingCharGravityMult:Havok") == 0) {
			charGravity = *it;
			_MESSAGE("Setting %s found", (*it)->_key);
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
		else if (msg->type == F4SE::MessagingInterface::kPreLoadGame) {
			VelocityMapCleanup();
		}
		else if (msg->type == F4SE::MessagingInterface::kNewGame) {
			VelocityMapCleanup();
		}
	});

	return true;
}
