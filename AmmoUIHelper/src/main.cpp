#include <Windows.h>
using namespace RE;

class BSTGlobalEvent {
public:
	virtual ~BSTGlobalEvent();

	template <typename T>
	class EventSource {
	public:
		virtual ~EventSource();

		// void ** _vtbl;                           // 00
		uint64_t unk08;              // 08
		BSTEventSource<T> src;    // 10
	};

	// void ** _vtbl;                               // 00
	uint64_t    unk08;                                // 08
	uint64_t    unk10;                                // 10
	BSTArray<EventSource<void*>*> eventSources;       // 18
};

REL::Relocation<BSTGlobalEvent**> g_globalEvents{ REL::ID(1424022) };

const static std::string ammoUIName{ "F4ammouiEXPANDEDMenu" };
const F4SE::TaskInterface* g_task;

PlayerCharacter* p;
TESGlobal* enableAmmoUI;
bool HUDHooked = false;
bool ammoEventHooked = false;
bool queueUpdate = false;
bool isRegistered = false;
std::vector<std::string> hideMenuList = { "BarterMenu", "ContainerMenu", "CookingMenu", "CreditsMenu", "DialogueMenu", "ExamineMenu",
											"LockpickingMenu", "LooksMenu", "MessageBoxMenu", "PauseMenu", "PipboyMenu", "PromptMenu",
											"SPECIALMenu", "TerminalHolotapeMenu", "TerminalMenu", "VATSMenu", "WorkshopMenu",
											"F4QMWMenu" };
int hideCount = 0;
int PACount = 0;
REL::Relocation<uint64_t*> ptr_engineTime{ REL::ID(1280610) };
uint64_t updateRequestTime;
uint32_t lastAmmoCount = 0;

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
	const char* ptr = static_cast<const char*>(mem);
	unsigned char* up = (unsigned char*)ptr;
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

const char* GetObjectClassNameImpl(const char* result, void* objBase) {
	using namespace RTTI;
	void** obj = (void**)objBase;
	CompleteObjectLocator** vtbl = (CompleteObjectLocator**)obj[0];
	CompleteObjectLocator* rtti = vtbl[-1];
	RVA<TypeDescriptor> typeDesc = rtti->typeDescriptor;

	// starts with ,?
	const char* _name = typeDesc->mangled_name();
	if ((_name[0] == '.') && (_name[1] == '?')) {
		// is at most 100 chars long
		for (uint32_t i = 0; i < 100; i++) {
			if (_name[i] == 0) {
				// remove the .?AV
				return _name + 4;
				break;
			}
		}
	}
	return result;
}

const char* GetObjectClassName(void* objBase) {
	const char* result = "<no rtti>";
	__try {
		result = GetObjectClassNameImpl(result, objBase);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		// return the default
	}

	return result;
}

bool CheckPA() {
	if (!p->extraList) {
		return false;
	}
	return p->extraList->HasType(EXTRA_DATA_TYPE::kPowerArmor);;
}

#pragma endregion

/*class HUDMenuOverride : public IMenu {
public:
	typedef UI_MESSAGE_RESULTS (HUDMenuOverride::* FnProcessMessage)(UIMessage& a_message);

	UI_MESSAGE_RESULTS HookedProcessMessage(UIMessage& a_message) {
		UIMessageQueue* msgQ = UIMessageQueue::GetSingleton();
		if (msgQ && enableAmmoUI->value) {
			msgQ->AddMessage(ammoUIName, a_message.type.get());
			if (a_message.type.get() == UI_MESSAGE_TYPE::kShow || a_message.type.get() == UI_MESSAGE_TYPE::kReshow) {
				_MESSAGE("PowerArmorHUDMenu Show");
			}
			else if (a_message.type.get() == UI_MESSAGE_TYPE::kHide || a_message.type.get() == UI_MESSAGE_TYPE::kForceHide) {
				_MESSAGE("PowerArmorHUDMenu Hide");
			}
		}
		FnProcessMessage fn = fnHash.at(*(uint64_t*)this);
		return fn ? (this->*fn)(a_message) : IMenu::ProcessMessage(a_message);
	}

	void Hook() {
		uint64_t vtable = *(uint64_t*)this;
		auto it = fnHash.find(vtable);
		if (it == fnHash.end()) {
			FnProcessMessage fn = SafeWrite64Function(vtable + 0x18, &HUDMenuOverride::HookedProcessMessage);
			fnHash.insert(std::pair<uint64_t, FnProcessMessage>(vtable, fn));
		}
	}

	void UnHook() {
		uint64_t vtable = *(uint64_t*)this;
		auto it = fnHash.find(vtable);
		if (it == fnHash.end())
			return;
		SafeWrite64Function(vtable + 0x18, it->second);
		fnHash.erase(it);
	}

protected:
	static std::unordered_map<uint64_t, FnProcessMessage> fnHash;
};
std::unordered_map<uint64_t, HUDMenuOverride::FnProcessMessage> HUDMenuOverride::fnHash;*/

class AmmoUI : public IMenu {
private:
	static AmmoUI* instance;
public:
	/*AmmoUI() {
		instance = this;
		instance->menuFlags = (UI_MENU_FLAGS)0;
		instance->UpdateFlag(UI_MENU_FLAGS::kAllowSaving, true);
		instance->UpdateFlag(UI_MENU_FLAGS::kDontHideCursorWhenTopmost, true);
		instance->depthPriority = 5;
		instance->inputEventHandlingEnabled = false;
		BSScaleformManager* sfm = BSScaleformManager::GetSingleton();
		bool succ = sfm->LoadMovieEx(*instance, "Interface/ammoUItoounEXPANDED.swf", "root1", BSScaleformManager::ScaleModeType::kNoBorder, 0.2f);
		if (succ) {
			_MESSAGE("AmmoUI Created %llx", instance);
			instance->menuObj.SetMember("menuFlags", Scaleform::GFx::Value(instance->menuFlags.underlying()));
			instance->menuObj.SetMember("movieFlags", Scaleform::GFx::Value(3));
			instance->menuObj.SetMember("extendedFlags", Scaleform::GFx::Value(3));
		}
		else {
			_MESSAGE("AmmoUI swf load failed");
		}
	}

	virtual ~AmmoUI() override {
		IMenu::~IMenu();
	};*/

	static bool IsHoldingGun() {
		if (!p->currentProcess)
			return false;
		BSTArray<EquippedItem>& equipped = p->currentProcess->middleHigh->equippedItems;
		if (equipped.size() == 0 || !equipped[0].item.instanceData ||
			((TESObjectWEAP::InstanceData*)equipped[0].item.instanceData.get())->type != 9)
			return false;

		return true;
	}

	static void SetAmmoCount(uint32_t current, uint32_t total, int ammostate) {
		if (!IsHoldingGun())
			return;

		UI* ui = UI::GetSingleton();
		Scaleform::Ptr<IMenu> hud = ui->GetMenu(BSFixedString("HUDMenu"));
		Scaleform::Ptr<IMenu> ammoUI = ui->GetMenu(ammoUIName);
		if (hud.get() && ammoUI.get() && ui->GetMenuOpen(BSFixedString("HUDMenu")) && ui->GetMenuOpen(ammoUIName)) {
			/*Scaleform::GFx::Value ammoCount1;
			hud->uiMovie->GetVariable(&ammoCount1, "root1.RightMeters_mc.AmmoCount_mc.ClipCount_tf.text");
			int ac1Int;
			std::string_view ac1 = ammoCount1.GetString();
			std::from_chars(ac1.data(), ac1.data() + ac1.size(), ac1Int);
			ac1 = std::to_string(ac1Int);
			Scaleform::GFx::Value ammoCount2;
			hud->uiMovie->GetVariable(&ammoCount2, "root1.RightMeters_mc.AmmoCount_mc.ReserveCount_tf.text");
			std::string_view ac2 = ammoCount2.GetString();*/
			Scaleform::GFx::Value explosiveCount;
			hud->uiMovie->GetVariable(&explosiveCount, "root1.RightMeters_mc.ExplosiveAmmoCount_mc.AvailableCount_tf.text");
			std::string_view ec = explosiveCount.GetString();
			std::string_view as = std::to_string(ammostate);

			//size_t bufLength = ac1.size() + ac2.size() + ec.size() + as.size() + 3;
			char buf[24];
			sprintf_s(buf, sizeof(buf), "%d,%d,%s,%s", current, total, ec.data(), as.data());
			if (ammoUI->uiMovie) {
				Scaleform::GFx::Value ammoUICount;
				ammoUI->uiMovie->asMovieRoot->CreateString(&ammoUICount, buf);
				ammoUI->uiMovie->asMovieRoot->SetVariable("root1.AmmoCount", ammoUICount, Scaleform::GFx::Movie::SetVarType::kPermanent);
			}
			else {
				_MESSAGE("AmmoUI Movie not found!");
			}
			lastAmmoCount = current;
		}
	}

	static void SetAmmoCount(int ammostate) {
		if (!IsHoldingGun())
			return;

		UI* ui = UI::GetSingleton();
		Scaleform::Ptr<IMenu> hud = ui->GetMenu(BSFixedString("HUDMenu"));
		Scaleform::Ptr<IMenu> ammoUI = ui->GetMenu(ammoUIName);
		if (hud.get() && ammoUI.get() && ui->GetMenuOpen(BSFixedString("HUDMenu")) && ui->GetMenuOpen(ammoUIName)) {
			Scaleform::GFx::Value ammoCount1;
			hud->uiMovie->GetVariable(&ammoCount1, "root1.RightMeters_mc.AmmoCount_mc.ClipCount_tf.text");
			int ac1Int;
			std::string_view ac1 = ammoCount1.GetString();
			std::from_chars(ac1.data(), ac1.data() + ac1.size(), ac1Int);
			ac1 = std::to_string(ac1Int);
			Scaleform::GFx::Value ammoCount2;
			hud->uiMovie->GetVariable(&ammoCount2, "root1.RightMeters_mc.AmmoCount_mc.ReserveCount_tf.text");
			std::string_view ac2 = ammoCount2.GetString();
			Scaleform::GFx::Value explosiveCount;
			hud->uiMovie->GetVariable(&explosiveCount, "root1.RightMeters_mc.ExplosiveAmmoCount_mc.AvailableCount_tf.text");
			std::string_view ec = explosiveCount.GetString();
			std::string_view as = std::to_string(ammostate);

			//size_t bufLength = ac1.size() + ac2.size() + ec.size() + as.size() + 3;
			char buf[24];
			sprintf_s(buf, sizeof(buf), "%s,%s,%s,%s", ac1.data(), ac2.data(), ec.data(), as.data());
			if (ammoUI->uiMovie) {
				Scaleform::GFx::Value ammoUICount;
				ammoUI->uiMovie->asMovieRoot->CreateString(&ammoUICount, buf);
				ammoUI->uiMovie->asMovieRoot->SetVariable("root1.AmmoCount", ammoUICount, Scaleform::GFx::Movie::SetVarType::kPermanent);
			}
			else {
				_MESSAGE("AmmoUI Movie not found!");
			}
		}
	}

	static AmmoUI* GetSingleton() {
		return instance;
	}

	static void Register() {
		HMODULE hF4SE = GetModuleHandleA("f4se_1_10_163.dll");
		if (!hF4SE) {
			_MESSAGE("F4SE 1.10.163 not found");
		}
		else {
			//f4se_1_10_163.dll + 0x58410
			_MESSAGE("F4SE Base %llx, fn %llx", hF4SE, ((uintptr_t)hF4SE + 0x58410));
			_MESSAGE("Registering %s", ammoUIName.c_str());
			_MESSAGE("UI %llx", UI::GetSingleton());
			struct MenuData {
				bool m_none = false;
				BSTSmartPointer<BSScript::Struct> m_struct;
				std::map<BSFixedString, BSScript::Variable> m_data;
				MenuData(BSTSmartPointer<BSScript::Struct> _d, int menuFlags, int movieFlags, int extendedFlags, int depth) {
					m_struct = _d;
					for (auto it = m_struct->type->varNameIndexMap.begin(); it != m_struct->type->varNameIndexMap.end(); ++it) {
						if (it->first == std::string("menuFlags")) {
							m_struct->variables[it->second] = menuFlags;
						}
						else if (it->first == std::string("movieFlags")) {
							m_struct->variables[it->second] = movieFlags;
						}
						else if (it->first == std::string("extendedFlags")) {
							m_struct->variables[it->second] = extendedFlags;
						}
						else if (it->first == std::string("depth")) {
							m_struct->variables[it->second] = depth;
						}
						m_data.insert(std::pair<BSFixedString, BSScript::Variable>(it->first, m_struct->variables[it->second]));
					}
					_MESSAGE("MenuData %llx struct %llx", this, m_struct.get());
				};
			};
			const auto game = GameVM::GetSingleton();
			const auto vm = game ? game->GetVM() : nullptr;
			BSTSmartPointer<BSScript::Struct> menuData;
			bool succ1 = vm->CreateStruct(BSFixedString("UI#MenuData"), menuData);
			if (succ1) {
				MenuData arg{ menuData, 0x806842, 3, 3, 5 };
				typedef bool (*F4SERegisterCustomMenu)(void*, BSFixedString menuName, BSFixedString menuPath, BSFixedString rootPath, MenuData menuData);
				F4SERegisterCustomMenu fn = (F4SERegisterCustomMenu)((uintptr_t)hF4SE + 0x58410);
				bool succ2 = fn(nullptr, ammoUIName, BSFixedString("ammoUItoounEXPANDED"), BSFixedString("root1"), arg);
				if (succ2) {
					_MESSAGE("F4SE processed");
					isRegistered = true;
				}
				else {
					_MESSAGE("F4SE failed");
				}
			}
		}
	}

	/*static void Register() {
		UI* ui = UI::GetSingleton();
		if (ui) {
			ui->RegisterMenu("F4ammouiEXPANDEDMenu", [](const UIMessage&)->IMenu* { 
				return new AmmoUI(); 
			});
			_MESSAGE("AmmoUI Registered");
		}
	}

	virtual UI_MESSAGE_RESULTS ProcessMessage(UIMessage& a_message) override {
		switch (*a_message.type) {
			case UI_MESSAGE_TYPE::kShow:
				_MESSAGE("Show");
				return UI_MESSAGE_RESULTS::kHandled;
			case UI_MESSAGE_TYPE::kHide:
				_MESSAGE("Hide");
				return UI_MESSAGE_RESULTS::kHandled;
			default:
				return IMenu::ProcessMessage(a_message);
		}
	}

	virtual void PostDisplay() override {
		if (uiMovie) {
			uiMovie->Capture();
		}
	}*/
};
AmmoUI* AmmoUI::instance = nullptr;

struct PlayerAmmoCountEvent {
	uint32_t current;
	uint32_t total;
};

struct PlayerAmmoCountEvent;
class AmmoEventWatcher : public BSTEventSink<PlayerAmmoCountEvent> {
	virtual BSEventNotifyControl ProcessEvent(const PlayerAmmoCountEvent& evn, BSTEventSource<PlayerAmmoCountEvent>* src) override {
		if (lastAmmoCount > evn.current) {
			AmmoUI::SetAmmoCount(evn.current, evn.total, 1);
		}
		else {
			AmmoUI::SetAmmoCount(evn.current, evn.total, 0);
		}
		return BSEventNotifyControl::kContinue;
	}

public:
	static BSTEventSource<PlayerAmmoCountEvent>* GetEventSource() {
		for (auto it = (*g_globalEvents.get())->eventSources.begin(); it != (*g_globalEvents.get())->eventSources.end(); ++it) {
			const char* name = GetObjectClassName(*it) + 15;
			if (strstr(name, "PlayerAmmoCountEvent") == name) {
				return (BSTEventSource<PlayerAmmoCountEvent>*)&((*it)->src);
			}
		}
		return nullptr;
	}
	F4_HEAP_REDEFINE_NEW(AmmoEventWatcher);
};

class MenuWatcher : public BSTEventSink<MenuOpenCloseEvent> {
	virtual BSEventNotifyControl ProcessEvent(const MenuOpenCloseEvent& evn, BSTEventSource<MenuOpenCloseEvent>* src) override {
		UIMessageQueue* msgQ = UIMessageQueue::GetSingleton();
		if (evn.menuName == BSFixedString("HUDMenu")) {
			UI* ui = UI::GetSingleton();
			/*if (!HUDHooked) {
				Scaleform::Ptr<IMenu> hud = ui->GetMenu(BSFixedString("HUDMenu"));
				if (hud.get()) {
					((HUDMenuOverride*)hud.get())->Hook();
					_MESSAGE("HUD Flag %llx depth %d", hud->menuFlags, hud->depthPriority);
					UIMessageQueue* msgQ = UIMessageQueue::GetSingleton();
					if (msgQ) {
						if (evn.opening && enableAmmoUI->value) {
							msgQ->AddMessage(ammoUIName, RE::UI_MESSAGE_TYPE::kShow);
							_MESSAGE("HUD Hook & Open");
						}
						else {
							msgQ->AddMessage(ammoUIName, RE::UI_MESSAGE_TYPE::kHide);
							_MESSAGE("HUD Hook & Hide");
						}
					}
					HUDHooked = true;
				}
			}*/
			if (enableAmmoUI->value) {
				if (evn.opening) {
					if (!ui->GetMenu(ammoUIName).get() && !ui->GetMenuOpen(ammoUIName)) {
						AmmoUI::Register();
					}
				}
			}
		}
		else if (evn.menuName == ammoUIName) {
			UI* ui = UI::GetSingleton();
			Scaleform::Ptr<IMenu> ammoUI = ui->GetMenu(ammoUIName);
			_MESSAGE("AmmoUI %llx opening %d hideCount %d", ammoUI.get(), evn.opening, hideCount);
			if (evn.opening) {
				AmmoUI::SetAmmoCount(99);
			}
		}
		else if (evn.menuName == BSFixedString("LoadingMenu") && !evn.opening) {
			_MESSAGE("Load complete. Resetting hideCount");
			hideCount = 0;
			UI* ui = UI::GetSingleton();
			for (auto it = hideMenuList.begin(); it != hideMenuList.end(); ++it) {
				if (ui->GetMenuOpen(*it)) {
					++hideCount;
					_MESSAGE("%s ++", (*it).c_str());
				}
			}
			PACount = CheckPA();
			_MESSAGE("Final hideCount %d PACount %d", hideCount, PACount);
			if (!ammoEventHooked) {
				AmmoEventWatcher* aew = new AmmoEventWatcher();
				AmmoEventWatcher::GetEventSource()->RegisterSink(aew);
				ammoEventHooked = true;
			}
		}
		if (enableAmmoUI->value) {
			for (auto it = hideMenuList.begin(); it != hideMenuList.end(); ++it) {
				if (evn.menuName == *it) {
					if (evn.opening) {
						++hideCount;
						_MESSAGE("%s ++", evn.menuName.c_str());
					}
					else {
						--hideCount;
						_MESSAGE("%s --", evn.menuName.c_str());
					}
					break;
				}
			}
			if (isRegistered && msgQ) {
				if (hideCount + PACount > 0) {
					msgQ->AddMessage(ammoUIName, RE::UI_MESSAGE_TYPE::kHide);
				}
				else {
					msgQ->AddMessage(ammoUIName, RE::UI_MESSAGE_TYPE::kShow);
				}
			}
		}
		return BSEventNotifyControl::kContinue;
	}
public:
	F4_HEAP_REDEFINE_NEW(MenuWatcher);
};

class PlayerDeathWatcher : public BSTEventSink<BGSActorDeathEvent> {
	virtual BSEventNotifyControl ProcessEvent(const BGSActorDeathEvent& evn, BSTEventSource<BGSActorDeathEvent>* src) override {
		_MESSAGE("Player Death");
		if (enableAmmoUI->value) {
			UIMessageQueue* msgQ = UIMessageQueue::GetSingleton();
			if (isRegistered && msgQ) {
				msgQ->AddMessage(ammoUIName, RE::UI_MESSAGE_TYPE::kHide);
				++hideCount;
			}
		}
		return BSEventNotifyControl::kContinue;
	}
public:
	F4_HEAP_REDEFINE_NEW(PlayerDeathWatcher);
};

class AnimationGraphEventWatcher : public BSTEventSink<BSAnimationGraphEvent> {
public:
	typedef BSEventNotifyControl (AnimationGraphEventWatcher::* FnProcessEvent)(BSAnimationGraphEvent& evn, BSTEventSource<BSAnimationGraphEvent>* dispatcher);

	BSEventNotifyControl HookedProcessEvent(BSAnimationGraphEvent& evn, BSTEventSource<BSAnimationGraphEvent>* src) {
		//_MESSAGE("evn %s argu %s PACheck %d", evn.animEvent.c_str(), evn.argument.c_str(), CheckPA());
		if (enableAmmoUI->value) {
			if (queueUpdate && *ptr_engineTime - updateRequestTime > 2000) {
				AmmoUI::SetAmmoCount(1);
				queueUpdate = false;
			}
			if (evn.animEvent == std::string("weaponFire") && evn.argument == std::string("2")) {
				queueUpdate = true;
				updateRequestTime = *ptr_engineTime;
			}
			else if (evn.animEvent == std::string("GraphDeleting") || evn.animEvent == std::string("FurnitureOff")) {
				PACount = CheckPA();
				UIMessageQueue* msgQ = UIMessageQueue::GetSingleton();
				if (isRegistered && msgQ) {
					if (hideCount + PACount > 0) {
						msgQ->AddMessage(ammoUIName, RE::UI_MESSAGE_TYPE::kHide);
					}
					else {
						msgQ->AddMessage(ammoUIName, RE::UI_MESSAGE_TYPE::kShow);
					}
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
	static std::unordered_map<uint64_t, FnProcessEvent> fnHash;
};
std::unordered_map<uint64_t, AnimationGraphEventWatcher::FnProcessEvent> AnimationGraphEventWatcher::fnHash;

void InitializePlugin() {
	p = PlayerCharacter::GetSingleton();
	((AnimationGraphEventWatcher*)((uint64_t)p + 0x38))->HookSink();
	enableAmmoUI = (TESGlobal*)GetFormFromMod("ammoUI_by_tooun.esl", 0x11801);
	MenuWatcher* mw = new MenuWatcher();
	UI::GetSingleton()->GetEventSource<MenuOpenCloseEvent>()->RegisterSink(mw);
	PlayerDeathWatcher* pdw = new PlayerDeathWatcher();
	((BSTEventSource<BGSActorDeathEvent>*)p)->RegisterSink(pdw);
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

	g_task = F4SE::GetTaskInterface();

	const F4SE::MessagingInterface* message = F4SE::GetMessagingInterface();
	message->RegisterListener([](F4SE::MessagingInterface::Message* msg) -> void {
		if (msg->type == F4SE::MessagingInterface::kGameDataReady) {
			InitializePlugin();
		}
	});

	return true;
}
