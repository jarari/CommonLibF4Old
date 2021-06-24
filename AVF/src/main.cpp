#include <Windows.h>
char tempbuf[8192] = { 0 };
char* _MESSAGE(const char* fmt, ...) {
	va_list args;

	va_start(args, fmt);
	vsprintf_s(tempbuf, sizeof(tempbuf), fmt, args);
	va_end(args);

	return tempbuf;
}

class VelocityData {
public:
	float x, y, z, duration, stepX, stepY, stepZ;
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
const float VelocityData::stepTime = 0.016667f;
using std::unordered_map;
unordered_map<Actor*, VelocityData> velMap;
unordered_map<Actor*, VelocityData> queueMap;

template<class Ty>
Ty SafeWrite64(uintptr_t addr, Ty data) {
	DWORD oldProtect;
	size_t len = sizeof(data);

	VirtualProtect((void*)addr, len, PAGE_EXECUTE_READWRITE, &oldProtect);
	Ty olddata = *(Ty*)addr;
	memcpy((void*)addr, &data, len);
	VirtualProtect((void*)addr, len, oldProtect, &oldProtect);
	return olddata;
}

float ApplyVelocity(Actor* a, hkVector4f v, bool modifyState = false) {
	if (!a->currentProcess || !a->currentProcess->middleHigh)
		return 0;
	NiPointer<bhkCharacterController> con = a->currentProcess->middleHigh->charController;
	if (!con.get())
		return 0;
	//logger::warn(_MESSAGE("Actor %llx Controller %llx ApplyVelocity", a, con.get()));
	con->velocityMod = v;
	*(uint64_t*)((uint64_t)con.get() + 0x300) = *(uint64_t*)((uint64_t)con.get() + 0x300) & ~((uint64_t)0xFF00) | (uint64_t)0x8700; //Jetpack flag?
	*(float*)((uint64_t)con.get() + 0x308) = con->stepInfo.deltaTime.storage;

	if (con->context.currentState == hknpCharacterState::hknpCharacterStateType::kOnGround) {
		NiPoint3A pos = a->data.location;
		pos.z += 40.0f;
		a->data.location = pos;
		a->UpdateActor3DPosition();
	}

	if (modifyState) {
		con->context.currentState = hknpCharacterState::hknpCharacterStateType::kSwimming;
	}
	return con->stepInfo.deltaTime.storage;
}

class CharacterMoveEventWatcher : public BSTEventSink<bhkCharacterMoveFinishEvent> {
public:
	typedef BSEventNotifyControl (CharacterMoveEventWatcher::* FnProcessEvent)(bhkCharacterMoveFinishEvent& evn, BSTEventSource<bhkCharacterMoveFinishEvent>* dispatcher);

	BSEventNotifyControl ProcessEvent(bhkCharacterMoveFinishEvent& evn, BSTEventSource<bhkCharacterMoveFinishEvent>* src) {
		Actor* a = (Actor*)((uint64_t)this - 0x150);
		if (a->loadedData) {
			auto result = velMap.find(a);
			if (result != velMap.end()) {
				VelocityData& data = result->second;
				auto qresult = queueMap.find(a);
				if (qresult != queueMap.end()) {
					VelocityData& queueData = qresult->second;
					data.x = queueData.x;
					data.y = queueData.y;
					data.z = queueData.z;
					data.duration = queueData.duration;
					data.stepX = queueData.stepX;
					data.stepY = queueData.stepY;
					data.stepZ = queueData.stepZ;
					queueMap.erase(a);
				}
				if (data.duration < 0) {
					velMap.erase(result);
					FnProcessEvent p_fn = UnHookIfNeeded();
					return p_fn ? (this->*p_fn)(evn, src) : BSEventNotifyControl::kContinue;
				}
				else {
					float deltaTime = ApplyVelocity(a, hkVector4f(data.x, data.y, data.z), true);
					data.x += data.stepX * deltaTime / VelocityData::stepTime;
					data.y += data.stepY * deltaTime / VelocityData::stepTime;
					data.z += data.stepZ * deltaTime / VelocityData::stepTime;
					data.duration -= deltaTime;
				}
			}
		}
		else {
			logger::warn(_MESSAGE("Actor %llx is dead or not loaded", a));
			velMap.erase(a);
			FnProcessEvent p_fn = UnHookIfNeeded();
			return p_fn ? (this->*p_fn)(evn, src) : BSEventNotifyControl::kContinue;
		}

		FnProcessEvent fn = fnHash.at(*(uint64_t*)this);
		return fn ? (this->*fn)(evn, src) : BSEventNotifyControl::kContinue;
	}

	void HookSink() {
		uint64_t vtable = *(uint64_t*)this;
		auto it = fnHash.find(vtable);
		if (it == fnHash.end()) {
			FnProcessEvent fn = SafeWrite64(vtable + 0x8, &CharacterMoveEventWatcher::ProcessEvent);
			fnHash.insert(std::pair<uint64_t, FnProcessEvent>(vtable, fn));
		}
	}

	void UnHookSink() {
		uint64_t vtable = *(uint64_t*)this;
		auto it = fnHash.find(vtable);
		if (it == fnHash.end())
			return;
		SafeWrite64(vtable + 0x8, it->second);
		fnHash.erase(it);
	}

	FnProcessEvent UnHookIfNeeded() {
		bool needsToBeUnhooked = true;
		uint64_t vtable = *(uint64_t*)this;
		for (auto it = velMap.begin(); it != velMap.end(); ++it) {
			if (vtable == *(uint64_t*)((uint64_t)(it->first) + 0x150)) {
				needsToBeUnhooked = false;
				break;
			}
		}
		auto it = fnHash.find(vtable);
		if (needsToBeUnhooked) {
			SafeWrite64(vtable + 0x8, it->second);
			FnProcessEvent fn = it->second;
			fnHash.erase(it);
			//logger::warn(_MESSAGE("vtable %llx needs to be unhooked.", vtable));
			return fn;
		}
		return it->second;
	}

	static void UnHookAll() {
		for (auto it = fnHash.begin(); it != fnHash.end(); ++it) {
			SafeWrite64(it->first + 0x8, it->second);
		}
		fnHash.clear();
	}

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

extern "C" DLLEXPORT void F4SEAPI SetVelocity(std::monostate, Actor* a, float x, float y, float z, float dur, float x2, float y2, float z2) {
	if (!a || !a->loadedData)
		return;
	//logger::warn(_MESSAGE("SetVelocity %llx %f %f %f %f", a, x, y, z, dur));
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
		velMap.insert(std::pair<Actor*, VelocityData>(a, data));

		CharacterMoveEventWatcher* cme = (CharacterMoveEventWatcher*)((uint64_t)a + 0x150);
		cme->HookSink();
	}
}

void VelocityMapCleanup() {
	CharacterMoveEventWatcher::UnHookAll();
	velMap.clear();
	queueMap.clear();
}

bool RegisterFuncs(BSScript::IVirtualMachine* a_vm)
{
	a_vm->BindNativeMethod("ActorVelocityFramework", "GetActorForward", GetActorForward, false);
	a_vm->BindNativeMethod("ActorVelocityFramework", "GetActorUp", GetActorUp, false);
	a_vm->BindNativeMethod("ActorVelocityFramework", "GetActorEye", GetActorEye, false);
	a_vm->BindNativeMethod("ActorVelocityFramework", "SetVelocity", SetVelocity, false);
	return true;
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

	*path /= "ActorVelocityFramework.log"sv;
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
	a_info->name = "ActorVelocityFramework";
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
		logger::warn("Papyrus functions registered."sv);
	}
	const F4SE::MessagingInterface* message = F4SE::GetMessagingInterface();
	message->RegisterListener([](F4SE::MessagingInterface::Message* msg) -> void {
		if (msg->type == F4SE::MessagingInterface::kPreLoadGame) {
			VelocityMapCleanup();
		}
	});
	return true;
}
