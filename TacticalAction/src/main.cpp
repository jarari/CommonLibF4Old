#include <Windows.h>
#include <SimpleIni.h>
#include <chrono>
char tempbuf[8192] = { 0 };
char* _MESSAGE(const char* fmt, ...) {
	va_list args;

	va_start(args, fmt);
	vsnprintf(tempbuf, sizeof(tempbuf), fmt, args);
	va_end(args);
	spdlog::log(spdlog::level::warn, tempbuf);

	return tempbuf;
}

void Dump(void* mem, unsigned int size) {
	char* p = static_cast<char*>(mem);
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
Ty SafeWrite64(uintptr_t addr, Ty data) {
	DWORD oldProtect;
	size_t len = sizeof(data);

	VirtualProtect((void*)addr, len, PAGE_EXECUTE_READWRITE, &oldProtect);
	Ty olddata = *(Ty*)addr;
	memcpy((void*)addr, &data, len);
	VirtualProtect((void*)addr, len, oldProtect, &oldProtect);
	return olddata;
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

using namespace std::chrono;
CSimpleIniA ini(true, false, false);
PlayerCharacter* p;
PlayerControls* pc;
PlayerCamera* pcam;
TESIdleForm* toounroll_Front;
TESIdleForm* toounroll_Left;
TESIdleForm* toounroll_Right;
TESIdleForm* toounroll_Back;
TESIdleForm* toounsliding;
TESIdleForm* weapDraw;
ActorValueInfo* actionPoint;
std::vector<float> movementHeldSecs = { 0.0f, 0.0f, 0.0f, 0.0f };
bool canRoll = true;
bool canSlide = true;
int rollState = 0;
int slideState = 0;
TESIdleForm* queueRoll;
TESIdleForm* queueSlide;
bool wasFP = false;
bool wasSneaking = false;
uint64_t timeActionEnded;
bool enableDodgeKey = true;
uint32_t dodgeKey = 0xa0;
float apCost = 25.0f;
float heldDownDelay = 0.2f;
bool enableDoubleTap = true;
float keyTimeout = 0.1f;
int numKeyPress = 2;
int keyPressedCount = 0;
bool enableSlideKey = true;
uint32_t slidekey = 0xa0;
std::chrono::system_clock::duration lastKeyTime;
uint32_t lastKey;
REL::Relocation<uint64_t*> ptr_engineTime{ REL::ID(1280610) };

using std::unordered_map;
class AnimationGraphEventWatcher : public BSTEventSink<BSAnimationGraphEvent> {
public:
	typedef BSEventNotifyControl (AnimationGraphEventWatcher::* FnProcessEvent)(BSAnimationGraphEvent& evn, BSTEventSource<BSAnimationGraphEvent>* dispatcher);

	BSEventNotifyControl HookedProcessEvent(BSAnimationGraphEvent& evn, BSTEventSource<BSAnimationGraphEvent>* src) {
		if (canRoll) {
			if (evn.argument == std::string("WPNmocamoca")) {
				queueRoll = nullptr;
				canRoll = false;
				canSlide = false;
				rollState = 1;
				p->ModActorValue(ACTOR_VALUE_MODIFIER::Damage, *actionPoint, -apCost);
				if (wasSneaking) {
					p->SetSneaking(true);
				}
			}
			if (queueRoll) {
				p->SetSneaking(false);
				if (p->currentProcess) {
					p->currentProcess->PlayIdle(p, 0x35, queueRoll);
					//_MESSAGE("Processing queued roll evn %s", evn.animEvent.c_str());
				}
			}
		}
		if (rollState) {
			if (rollState == 1 && evn.animEvent == std::string("AnimObjUnequip")) {
				rollState = 2;
				if (wasFP) {
					wasFP = false;
					pcam->SetState(pcam->cameraStates[CameraState::kFirstPerson].get());
					//_MESSAGE("Return FP");
				}
				if (!wasSneaking) {
					canRoll = true;
					canSlide = true;
					rollState = 0;
				}
				timeActionEnded = *ptr_engineTime;
			}
			else if (rollState == 2 && wasSneaking && *ptr_engineTime - timeActionEnded > 5000) {
				if (p->stance == 0x1) {
					wasSneaking = false;
					canRoll = true;
					canSlide = true;
					rollState = 0;
				}
				else {
					p->SetSneaking(true);
				}
			}
		}

		if (canSlide) {
			if (evn.argument == std::string("WPNSlidingtoSneak")) {
				queueSlide = nullptr;
				canRoll = false;
				canSlide = false;
				slideState = 1;
				p->ModActorValue(ACTOR_VALUE_MODIFIER::Damage, *actionPoint, -apCost);
				p->SetSneaking(true);
				p->SetWeaponState(WEAPON_STATE::kDrawn);
			}
			if (queueSlide) {
				p->SetSneaking(false);
				if (p->currentProcess) {
					p->currentProcess->PlayIdle(p, 0x35, queueSlide);
					//_MESSAGE("Processing queued roll evn %s", evn.animEvent.c_str());
				}
			}
		} 

		if (slideState) {
			if (slideState == 1 && evn.animEvent == std::string("AnimObjUnequip")) {
				slideState = 2;
				pcam->SetState(pcam->cameraStates[CameraState::kFirstPerson].get());
				timeActionEnded = *ptr_engineTime;
			}
			else if (slideState == 2) {
				if (pcam->currentState == pcam->cameraStates[CameraState::kFirstPerson]){
					p->SetWeaponState(WEAPON_STATE::kSheathed);
					p->currentProcess->PlayIdle(p, 0x35, weapDraw);
					slideState = 3;
				}
			}
			else if (slideState == 3) {
				if (p->stance == 0x1) {
					canRoll = true;
					canSlide = true;
					slideState = 0;
				} else {
					p->SetSneaking(true);
				}
			}
		}

		FnProcessEvent fn = fnHash.at(*(uint64_t*)this);
		return fn ? (this->*fn)(evn, src) : BSEventNotifyControl::kContinue;
	}



	void HookSink() {
		uint64_t vtable = *(uint64_t*)this;
		auto it = fnHash.find(vtable);
		if (it == fnHash.end()) {
			FnProcessEvent fn = SafeWrite64(vtable + 0x8, &AnimationGraphEventWatcher::HookedProcessEvent);
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

protected:
	static unordered_map<uint64_t, FnProcessEvent> fnHash;
};
unordered_map<uint64_t, AnimationGraphEventWatcher::FnProcessEvent> AnimationGraphEventWatcher::fnHash;

void DoRoll(int dir) {
	if (!queueRoll && canRoll && p->GetActorValue(*actionPoint) >= apCost) {
		if (p->currentProcess) {
			TESIdleForm* roll = toounroll_Front;
			//_MESSAGE("Roll");
			switch (dir) {
				case 1:
					roll = toounroll_Left;
					//_MESSAGE("Left");
					break;
				case 2:
					roll = toounroll_Right;
					//_MESSAGE("Right");
					break;
				case 3:
					roll = toounroll_Back;
					//_MESSAGE("Back");
					break;
			}
			if (pcam->currentState == pcam->cameraStates[CameraState::kFirstPerson]) {
				wasFP = true;
				queueRoll = roll;
				pcam->SetState(pcam->cameraStates[CameraState::k3rdPerson].get());
				//_MESSAGE("Force 3rd person");
			}
			if (p->stance == 0x1) {	//Sneaking
				wasSneaking = true;
				queueRoll = roll;
				p->SetSneaking(false);
			}
			if(!queueRoll) {
				p->currentProcess->PlayIdle(p, 0x35, roll);
			}
		}
	}
}

//0x100 = Sprinting
void DoSlide(int dir) {
	if (p->moveMode & 0x100 && !queueSlide && canSlide && p->GetActorValue(*actionPoint) >= apCost) {
		if (p->currentProcess) {
			//_MESSAGE("Slide");
			if (pcam->currentState == pcam->cameraStates[CameraState::kFirstPerson]) {
				queueSlide = toounsliding;
				pcam->SetState(pcam->cameraStates[CameraState::k3rdPerson].get());
				//_MESSAGE("Force 3rd person");
			}
			if (p->stance == 0x1) {	 //Sneaking
				queueSlide = toounsliding;
				p->SetSneaking(false);
			}
			if (p->weaponState != WEAPON_STATE::kDrawn) {
				queueSlide = toounsliding;
				p->currentProcess->PlayIdle(p, 0x35, weapDraw);
			}
			if (!queueSlide) {
				p->currentProcess->PlayIdle(p, 0x35, toounsliding);
			}
		}
	}
}



void StrafeTapRoll(uint32_t id, int dir) {
	if (lastKey != id || !canRoll) {
		lastKey = id;
		keyPressedCount = 1;
		lastKeyTime = system_clock::now().time_since_epoch();
		return;
	}
	if (duration_cast<milliseconds>(system_clock::now().time_since_epoch() - lastKeyTime).count() < keyTimeout * 1000.0f) {
		++keyPressedCount;
		if (keyPressedCount >= numKeyPress) {
			lastKey = id;
			keyPressedCount = 0;
			DoRoll(dir);
		}
	}
	else {
		keyPressedCount = 1;
	}
	lastKeyTime = system_clock::now().time_since_epoch();
}

class TacticalDodgeHandler : public BSInputEventReceiver {
public:
	typedef void (TacticalDodgeHandler::* FnPerformInputProcessing)(const InputEvent* a_queueHead);

	void HandleMultipleButtonEvent(const ButtonEvent* evn) {
		if (evn->eventType != INPUT_EVENT_TYPE::kButton) {
			if (evn->next)
				HandleMultipleButtonEvent((ButtonEvent*)evn->next);
			return;
		}
		uint32_t id = evn->idCode;
		if (evn->device == INPUT_DEVICE::kMouse)
			id += 256;

		if (evn->strUserEvent == std::string_view("Forward")) {
			if (evn->value == 1.0f) {
				movementHeldSecs[0] = evn->heldDownSecs;
			}
			else {
				movementHeldSecs[0] = 0.0f;
			}
		}
		else if (evn->strUserEvent == std::string_view("StrafeLeft")) {
			if (evn->value == 1.0f) {
				movementHeldSecs[1] = evn->heldDownSecs;
			}
			else {
				movementHeldSecs[1] = 0.0f;
			}
		}
		else if (evn->strUserEvent == std::string_view("StrafeRight")) {
			if (evn->value == 1.0f) {
				movementHeldSecs[2] = evn->heldDownSecs;
			}
			else {
				movementHeldSecs[2] = 0.0f;
			}
		}
		else if (evn->strUserEvent == std::string_view("Back")) {
			if (evn->value == 1.0f) {
				movementHeldSecs[3] = evn->heldDownSecs;
			}
			else {
				movementHeldSecs[3] = 0.0f;
			}
		}

		if (enableDodgeKey && canRoll && id == dodgeKey && evn->value == 0.0f && evn->heldDownSecs < heldDownDelay) {
			int dir = 0;
			float minHeldSecs = movementHeldSecs[0] ? movementHeldSecs[0] : 999999.0f;
			for (int i = 1; i < 4; ++i) {
				if (movementHeldSecs[i] && movementHeldSecs[i] < minHeldSecs) {
					dir = i;
					minHeldSecs = movementHeldSecs[i];
				}
			}
			DoRoll(dir);
		}

		if (enableDoubleTap && canRoll) {
			if (evn->value == 1.0f && evn->heldDownSecs == 0.0f) {
				if (evn->strUserEvent == std::string_view("Forward")) {
					StrafeTapRoll(id, 0);
				}
				else if (evn->strUserEvent == std::string_view("StrafeLeft")) {
					StrafeTapRoll(id, 1);
				}
				else if (evn->strUserEvent == std::string_view("StrafeRight")) {
					StrafeTapRoll(id, 2);
				}
				else if (evn->strUserEvent == std::string_view("Back")) {
					StrafeTapRoll(id, 3);
				}
			}
		}

		if (enableSlideKey && canSlide && id == slidekey && evn->value == 1.0f) {
			int dir = 0;
			float minHeldSecs = movementHeldSecs[0] ? movementHeldSecs[0] : 999999.0f;
			for (int i = 1; i < 4; ++i) {
				if (movementHeldSecs[i] && movementHeldSecs[i] < minHeldSecs) {
					dir = i;
					minHeldSecs = movementHeldSecs[i];
				}
			}
			DoSlide(dir);
		}

		if (evn->next)
			HandleMultipleButtonEvent((ButtonEvent*)evn->next);
	}

	void HookedPerformInputProcessing(const InputEvent* a_queueHead) {
		if (!UI::GetSingleton()->menuMode && a_queueHead) {
			if (enableDodgeKey || enableSlideKey)
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
			FnPerformInputProcessing fn = SafeWrite64Function(vtable, &TacticalDodgeHandler::HookedPerformInputProcessing);
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
unordered_map<uint64_t, TacticalDodgeHandler::FnPerformInputProcessing> TacticalDodgeHandler::fnHash;

void InitializePlugin() {
	p = PlayerCharacter::GetSingleton();
	((AnimationGraphEventWatcher*)((uint64_t)p + 0x38))->HookSink();
	pcam = PlayerCamera::GetSingleton();
	((TacticalDodgeHandler*)((uint64_t)pcam + 0x38))->HookSink();
	_MESSAGE("PlayerCharacter %llx", p);
	_MESSAGE("UI %llx", UI::GetSingleton());
	_MESSAGE("PlayerCamera %llx", pcam);
	toounroll_Front = (TESIdleForm*)GetFormFromMod(std::string("tooun_Animationpack.esl"), 0x82D);
	toounroll_Left = (TESIdleForm*)GetFormFromMod(std::string("tooun_Animationpack.esl"), 0x82B);
	toounroll_Right = (TESIdleForm*)GetFormFromMod(std::string("tooun_Animationpack.esl"), 0x82A);
	toounroll_Back = (TESIdleForm*)GetFormFromMod(std::string("tooun_Animationpack.esl"), 0x82C);
	toounsliding = (TESIdleForm*)GetFormFromMod(std::string("tooun_Animationpack.esl"), 0x800);
	weapDraw = (TESIdleForm*)TESForm::GetFormByID(0x17ADC);
	actionPoint = (ActorValueInfo*)TESForm::GetFormByID(0x2D5);
}

void LoadConfigs() {
	ini.LoadFile("Data\\F4SE\\Plugins\\TacticalAction.ini");
	enableDodgeKey = std::stoi(ini.GetValue("General", "EnableDodgeKey", "1")) > 0;
	dodgeKey = std::stoi(ini.GetValue("General", "DodgeKey", "0xa0"), 0, 16);
	apCost = std::stof(ini.GetValue("General", "APCost", "25.0"));
	heldDownDelay = std::stof(ini.GetValue("General", "HeldDownDelay", "0.2"));
	enableDoubleTap = std::stoi(ini.GetValue("General", "EnableDoubleTap", "1")) > 0;
	keyTimeout = std::stof(ini.GetValue("General", "KeyTimeout", "0.2"));
	numKeyPress = std::stoi(ini.GetValue("General", "NumKeyPress", "2"));
	enableSlideKey = std::stoi(ini.GetValue("General", "EnableSlideKey", "1")) > 0;
	slidekey = std::stoi(ini.GetValue("General", "Slidekey", "0xa2"), 0, 16);
}

void ResetWASD() {
	for (int i = 0; i < 4; ++i) {
		movementHeldSecs[i] = 0.0f;
	}
	lastKey = 9999;
	keyPressedCount = 0;
	lastKeyTime = system_clock::now().time_since_epoch();
	queueRoll = nullptr;
	queueSlide = nullptr;
	wasFP = false;
	wasSneaking = false;
	canRoll = true;
	canSlide = true;
	rollState = 0;
	slideState = 0;
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

	*path /= "TacticalAction.log"sv;
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
	a_info->name = "TacticalAction";
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

	const F4SE::MessagingInterface* message = F4SE::GetMessagingInterface();
	message->RegisterListener([](F4SE::MessagingInterface::Message* msg) -> void {
		if (msg->type == F4SE::MessagingInterface::kGameDataReady) {
			InitializePlugin();
			LoadConfigs();
		}
		else if (msg->type == F4SE::MessagingInterface::kPostLoadGame) {
			LoadConfigs();
			ResetWASD();
		}
		else if (msg->type == F4SE::MessagingInterface::kNewGame) {
			LoadConfigs();
			ResetWASD();
		}
	});
	return true;
}
