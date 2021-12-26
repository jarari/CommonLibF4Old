#include <Windows.h>
#include <array>
#include <unordered_map>
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

bool IsInPowerArmor(Actor* a) {
	if (!a->extraList) {
		return false;
	}
	return a->extraList->HasType(EXTRA_DATA_TYPE::kPowerArmor);;
}

const static std::string UIName{ "BingleBodyPartsUI" };
PlayerCharacter* p;
std::vector<std::string> hideMenuList = { "BarterMenu", "ContainerMenu", "CookingMenu", "CreditsMenu", "DialogueMenu", "ExamineMenu", "LevelUpMenu",
											"LockpickingMenu", "LooksMenu", "MessageBoxMenu", "PauseMenu", "PipboyMenu", "BookMenu",
											"SPECIALMenu", "TerminalHolotapeMenu", "TerminalMenu", "VATSMenu", "WorkshopMenu", "SitWaitMenu", "SleepWaitMenu",
											"F4QMWMenu" };
int hideCount = 0;
bool isInPA = false;
bool isInteracting = false;
bool isMenuOpen = false;
bool isLoading = false;
IMenu* HUDMenu;
ActorValueInfo* perceptionCondition;
ActorValueInfo* enduranceCondition;
ActorValueInfo* rightAttackCondition;
ActorValueInfo* leftAttackCondition;
ActorValueInfo* rightMobilityCondition;
ActorValueInfo* leftMobilityCondition;
ActorValueInfo** BodyPartConditions[] = {
	&perceptionCondition,
	&enduranceCondition,
	&rightAttackCondition,
	&leftAttackCondition,
	&rightMobilityCondition,
	&leftMobilityCondition
};
REL::Relocation<float*> ptr_engineTime{ REL::ID(599343) };
float lastUpdated = 0;

class BodyPartsUI : public IMenu {
private:
	static BodyPartsUI* instance;
	int partIndex = 0;
public:

	enum BodyParts {
		Head,
		Torso,
		RArm,
		LArm,
		RLeg,
		LLeg
	};

	BodyPartsUI() : IMenu() {
		if (instance) {
			delete(instance);
		}
		instance = this;
		instance->menuFlags = (UI_MENU_FLAGS)0;
		instance->UpdateFlag(UI_MENU_FLAGS::kAllowSaving, true);
		instance->UpdateFlag(UI_MENU_FLAGS::kDontHideCursorWhenTopmost, true);
		instance->UpdateFlag(UI_MENU_FLAGS::kAlwaysOpen, true);
		instance->depthPriority = 5;
		instance->inputEventHandlingEnabled = false;
		BSScaleformManager* sfm = BSScaleformManager::GetSingleton();
		bool succ = sfm->LoadMovieEx(*instance, "Interface/BodyPartsUI.swf", "root1", BSScaleformManager::ScaleModeType::kExactFit, 0.0f);
		if (succ) {
			_MESSAGE("BodyPartsUI Created %llx", instance);
			instance->menuObj.SetMember("menuFlags", Scaleform::GFx::Value(instance->menuFlags.underlying()));
			instance->menuObj.SetMember("movieFlags", Scaleform::GFx::Value(3));
			instance->menuObj.SetMember("extendedFlags", Scaleform::GFx::Value(3));
		}
		else {
			_MESSAGE("BodyPartsUI swf load failed");
		}
	}

	virtual void AdvanceMovie(float delta, uint64_t time) override {
		//_MESSAGE("AdvanceMovie called delta %f time %f", delta, time);
		BodyPartsUI* bpUI = BodyPartsUI::GetSingleton();
		if (bpUI) {
			float avdamage = p->GetModifier(ACTOR_VALUE_MODIFIER::Damage, **BodyPartConditions[partIndex]);
			float avperm = p->GetPermanentActorValue(**BodyPartConditions[partIndex]);
			float scale = (avperm + avdamage) / avperm;
			bpUI->UpdateBodypartCondition((BodyPartsUI::BodyParts)partIndex, scale);
			++partIndex;
			if (partIndex > 5)
				partIndex = 0;
		}
		__super::AdvanceMovie(delta, time);
	}
	
	virtual void PostDisplay() override {
		if (*ptr_engineTime - lastUpdated >= 0.2f) {
			BodyPartsUI* bpUI = BodyPartsUI::GetSingleton();
			if (bpUI) {
				//UIMessageQueue* msgQ = UIMessageQueue::GetSingleton();
				//_MESSAGE("hidecount %d PA %d interacting %d isMenuOpen %d", hideCount, isInPA, isInteracting, isMenuOpen);
				isInteracting = p->interactingState != INTERACTING_STATE::kNotInteracting;
				isInPA = IsInPowerArmor(p);
				if (hideCount + isInteracting > 0 && isMenuOpen) {
					//msgQ->AddMessage(UIName, RE::UI_MESSAGE_TYPE::kHide);
					bpUI->SetVisible(false);
					isMenuOpen = false;
					//_MESSAGE("Hide (Interaction)");
				}
				else if (hideCount + isInPA + isInteracting == 0 && !isMenuOpen) {
					//msgQ->AddMessage(UIName, RE::UI_MESSAGE_TYPE::kShow);
					bpUI->SetVisible(true);
					isMenuOpen = true;
					//_MESSAGE("Show (Interaction)");
				}
			}
			lastUpdated = *ptr_engineTime;
		}
	}

	void SetVisible(bool enabled) {
		if (uiMovie && uiMovie->asMovieRoot) {
			//_MESSAGE("Update equipment slot %d equip %d", slot, equipped);
			std::array<Scaleform::GFx::Value, 1> args;
			args[0] = enabled;
			bool succ = uiMovie->asMovieRoot->Invoke("root1.CharacterManager.SetVisible", nullptr, args.data(), args.size());
			if (!succ) {
				_MESSAGE("Invoke failed");
			}
		}
	}

	void UpdateEquipment(int slot, bool equipped) {
		if (uiMovie && uiMovie->asMovieRoot) {
			//_MESSAGE("Update equipment slot %d equip %d", slot, equipped);
			std::array<Scaleform::GFx::Value, 2> args;
			args[0] = slot;
			args[1] = equipped;
			bool succ = uiMovie->asMovieRoot->Invoke("root1.CharacterManager.UpdateEquipment", nullptr, args.data(), args.size());
			if (!succ) {
				_MESSAGE("Invoke failed");
			}
		}
	}

	void UpdateBodypartCondition(BodyParts part, double scale) {
		if (uiMovie && uiMovie->asMovieRoot) {
			//_MESSAGE("Update condition part %d scale %f", part, scale);
			std::array<Scaleform::GFx::Value, 2> args;
			args[0] = (int)part;
			args[1] = scale;
			bool succ = uiMovie->asMovieRoot->Invoke("root1.CharacterManager.UpdateBodypartCondition", nullptr, args.data(), args.size());
			if (!succ) {
				_MESSAGE("Invoke failed");
			}
		}
	}

	void ResetEquipment() {
		if (uiMovie && uiMovie->asMovieRoot) {
			//_MESSAGE("Reset Equipment");
			std::array<Scaleform::GFx::Value, 0> args;
			bool succ = uiMovie->asMovieRoot->Invoke("root1.CharacterManager.ResetEquipment", nullptr, args.data(), args.size());
			if (!succ) {
				_MESSAGE("Invoke failed");
			}
			if (p->inventoryList) {
				for (auto item = p->inventoryList->data.begin(); item != p->inventoryList->data.end(); ++item) {
					if (item->stackData->IsEquipped()) {
						if (item->object->formType == ENUM_FORM_ID::kARMO) {
							TESObjectARMO* armor = static_cast<TESObjectARMO*>(item->object);
							if (armor->bipedModelData.bipedObjectSlots & 0x1) {
								UpdateEquipment(30, true);
							}
							else if (armor->bipedModelData.bipedObjectSlots & 0x8) {
								UpdateEquipment(33, true);
							}
							else if (armor->bipedModelData.bipedObjectSlots & 0x800) {
								UpdateEquipment(41, true);
							}
							else if (armor->bipedModelData.bipedObjectSlots & 0x1000) {
								UpdateEquipment(42, true);
							}
							else if (armor->bipedModelData.bipedObjectSlots & 0x2000) {
								UpdateEquipment(43, true);
							}
							else if (armor->bipedModelData.bipedObjectSlots & 0x4000) {
								UpdateEquipment(44, true);
							}
							else if (armor->bipedModelData.bipedObjectSlots & 0x8000) {
								UpdateEquipment(45, true);
							}
						}
					}
				}
			}
		}
	}

	static BodyPartsUI* GetSingleton() {
		return instance;
	}

};
BodyPartsUI* BodyPartsUI::instance = nullptr;

void RegisterMenu() {
	UI* ui = UI::GetSingleton();
	UIMessageQueue* msgQ = UIMessageQueue::GetSingleton();
	if (ui) {
		ui->RegisterMenu(UIName.c_str(), [](const UIMessage&)->IMenu* {
			BodyPartsUI* bpUI = BodyPartsUI::GetSingleton();
			if (!bpUI) {
				bpUI = new BodyPartsUI();
			}
			return bpUI;
		});
		msgQ->AddMessage(UIName, RE::UI_MESSAGE_TYPE::kShow);
	}
}

class MenuWatcher : public BSTEventSink<MenuOpenCloseEvent> {
	virtual BSEventNotifyControl ProcessEvent(const MenuOpenCloseEvent& evn, BSTEventSource<MenuOpenCloseEvent>* src) override {
		UI* ui = UI::GetSingleton();
		UIMessageQueue* msgQ = UIMessageQueue::GetSingleton();
		BodyPartsUI* bpUI = BodyPartsUI::GetSingleton();
		if (msgQ && !ui->GetMenuOpen(UIName)) {
			msgQ->AddMessage(UIName, RE::UI_MESSAGE_TYPE::kShow);
		}
		//_MESSAGE("Menu %s opening %d", evn.menuName.c_str(), evn.opening);
		if (evn.menuName == BSFixedString("LoadingMenu")) {
			if (evn.opening) {
				isLoading = true;
			}
			else {
				hideCount = 0;
				for (auto it = hideMenuList.begin(); it != hideMenuList.end(); ++it) {
					if (ui->GetMenuOpen(*it)) {
						++hideCount;
					}
				}
				if (bpUI)
					bpUI->ResetEquipment();
				isLoading = false;
			}
		}
		for (auto it = hideMenuList.begin(); it != hideMenuList.end(); ++it) {
			if (evn.menuName == *it) {
				if (evn.opening) {
					++hideCount;
					//_MESSAGE("%s ++ %d", evn.menuName.c_str(), hideCount);
				}
				else {
					--hideCount;
					//_MESSAGE("%s -- %d", evn.menuName.c_str(), hideCount);
				}
				break;
			}
		}
		if (bpUI) {
			if (hideCount + isInPA + isInteracting > 0 && isMenuOpen) {
				//msgQ->AddMessage(UIName, RE::UI_MESSAGE_TYPE::kHide);
				bpUI->SetVisible(false);
				isMenuOpen = false;
				//_MESSAGE("Hide");
			}
			else if(hideCount + isInPA + isInteracting == 0 && !isMenuOpen){
				//msgQ->AddMessage(UIName, RE::UI_MESSAGE_TYPE::kShow);
				bpUI->SetVisible(true);
				isMenuOpen = true;
				//_MESSAGE("Show");
			}
		}
		return BSEventNotifyControl::kContinue;
	}
};

struct TESObjectLoadedEvent {
	uint32_t formId;			//00
	uint8_t loaded;				//08
};

struct TESEquipEvent {
	Actor* a;					//00
	uint32_t formId;			//0C
	uint32_t unk08;				//08
	uint64_t flag;				//10 0x00000000ff000000 for unequip
};

class ObjectLoadedEventSource : public BSTEventSource<TESObjectLoadedEvent> {
public:
	[[nodiscard]] static ObjectLoadedEventSource* GetSingleton() {
		REL::Relocation<ObjectLoadedEventSource*> singleton{ REL::ID(416662) };
		return singleton.get();
	}
};

class EquipEventSource : public BSTEventSource<TESEquipEvent> {
public:
	[[nodiscard]] static EquipEventSource* GetSingleton() {
		REL::Relocation<EquipEventSource*> singleton{ REL::ID(485633) };
		return singleton.get();
	}
};

class EquipWatcher : public BSTEventSink<TESEquipEvent> {
public:
	virtual BSEventNotifyControl ProcessEvent(const TESEquipEvent& evn, BSTEventSource<TESEquipEvent>* a_source) {
		if (evn.a == p) {
			TESForm* item = TESForm::GetFormByID(evn.formId);
			if (item && item->formType == ENUM_FORM_ID::kARMO) {
				TESObjectARMO* armor = static_cast<TESObjectARMO*>(item);
				BodyPartsUI* bpUI = BodyPartsUI::GetSingleton();
				if (bpUI) {
					if (armor->bipedModelData.bipedObjectSlots & 0x1) {
						if (evn.flag == 0x00000000ff000000) {
							bpUI->UpdateEquipment(30, false);
						}
						else {
							bpUI->UpdateEquipment(30, true);
						}
					}
					else if (armor->bipedModelData.bipedObjectSlots & 0x8) {
						if (evn.flag == 0x00000000ff000000) {
							bpUI->UpdateEquipment(33, false);
						}
						else {
							bpUI->UpdateEquipment(33, true);
						}
					}
					else if (armor->bipedModelData.bipedObjectSlots & 0x800) {
						if (evn.flag == 0x00000000ff000000) {
							bpUI->UpdateEquipment(41, false);
						}
						else {
							bpUI->UpdateEquipment(41, true);
						}
					}
					else if (armor->bipedModelData.bipedObjectSlots & 0x1000) {
						if (evn.flag == 0x00000000ff000000) {
							bpUI->UpdateEquipment(42, false);
						}
						else {
							bpUI->UpdateEquipment(42, true);
						}
					}
					else if (armor->bipedModelData.bipedObjectSlots & 0x2000) {
						if (evn.flag == 0x00000000ff000000) {
							bpUI->UpdateEquipment(43, false);
						}
						else {
							bpUI->UpdateEquipment(43, true);
						}
					}
					else if (armor->bipedModelData.bipedObjectSlots & 0x4000) {
						if (evn.flag == 0x00000000ff000000) {
							bpUI->UpdateEquipment(44, false);
						}
						else {
							bpUI->UpdateEquipment(44, true);
						}
					}
					else if (armor->bipedModelData.bipedObjectSlots & 0x8000) {
						if (evn.flag == 0x00000000ff000000) {
							bpUI->UpdateEquipment(45, false);
						}
						else {
							bpUI->UpdateEquipment(45, true);
						}
					}
				}
			}
		}
		return BSEventNotifyControl::kContinue;
	}
};

class ObjectLoadWatcher : public BSTEventSink<TESObjectLoadedEvent> {
public:
	virtual BSEventNotifyControl ProcessEvent(const TESObjectLoadedEvent& evn, BSTEventSource<TESObjectLoadedEvent>* a_source) {
		if (!evn.loaded) {
			return BSEventNotifyControl::kContinue;
		}
		if (evn.formId == 0x7) {
			BodyPartsUI* bpUI = BodyPartsUI::GetSingleton();
			isInPA = IsInPowerArmor(p);
			if (bpUI)
				bpUI->ResetEquipment();
		}
		return BSEventNotifyControl::kContinue;
	}
};

/*class HookModActorValue {
public:
	typedef void (HookModActorValue::* FnModActorValue)(ACTOR_VALUE_MODIFIER a_modifier, const ActorValueInfo& a_info, float a_delta);

	void HookedModActorValue(ACTOR_VALUE_MODIFIER a_modifier, const ActorValueInfo& a_info, float a_delta) {
		if (a_modifier == ACTOR_VALUE_MODIFIER::Damage || a_modifier == ACTOR_VALUE_MODIFIER::Temp) {
			_MESSAGE("ModAV %llx %f", &a_info, a_delta);
			for (int i = 0; i < 6; ++i) {
				if (a_info.formID == (*BodyPartConditions[i])->formID) {
					BodyPartsUI* bpUI = BodyPartsUI::GetSingleton();
					float avdamage = p->GetModifier(ACTOR_VALUE_MODIFIER::Damage, **BodyPartConditions[i]);
					float avperm = p->GetPermanentActorValue(**BodyPartConditions[i]);
					float scale = (avperm - avdamage + a_delta) / avperm;
					if (bpUI)
						bpUI->UpdateBodypartCondition((BodyPartsUI::BodyParts)i, scale);
				}
			}
		}
		FnModActorValue fn = fnHash.at(*(uint64_t*)this);
		if (fn)
			(this->*fn)(a_modifier, a_info, a_delta);
	}

	static void HookSink(uint64_t vtable) {
		auto it = fnHash.find(vtable);
		if (it == fnHash.end()) {
			FnModActorValue fn = SafeWrite64Function(vtable + 0x30, &HookModActorValue::HookedModActorValue);
			fnHash.insert(std::pair<uint64_t, FnModActorValue>(vtable, fn));
		}
	}
protected:
	static std::unordered_map<uint64_t, FnModActorValue> fnHash;
};
std::unordered_map<uint64_t, HookModActorValue::FnModActorValue> HookModActorValue::fnHash;*/

void InitializePlugin() {
	p = PlayerCharacter::GetSingleton();
	RegisterMenu();
	MenuWatcher* mw = new MenuWatcher();
	UI::GetSingleton()->GetEventSource<MenuOpenCloseEvent>()->RegisterSink(mw);
	perceptionCondition = (ActorValueInfo*)TESForm::GetFormByID(0x00036C);
	enduranceCondition = (ActorValueInfo*)TESForm::GetFormByID(0x00036D);
	leftAttackCondition = (ActorValueInfo*)TESForm::GetFormByID(0x00036E);
	rightAttackCondition = (ActorValueInfo*)TESForm::GetFormByID(0x00036F);
	leftMobilityCondition = (ActorValueInfo*)TESForm::GetFormByID(0x000370);
	rightMobilityCondition = (ActorValueInfo*)TESForm::GetFormByID(0x000371);
	EquipWatcher* ew = new EquipWatcher();
	EquipEventSource::GetSingleton()->RegisterSink(ew);
	ObjectLoadWatcher* olw = new ObjectLoadWatcher();
	ObjectLoadedEventSource::GetSingleton()->RegisterSink(olw);
	//uint64_t ActorStateVtable = REL::Relocation<uint64_t>{ PlayerCharacter::VTABLE[9] }.address();
	//HookModActorValue::HookSink(ActorStateVtable);
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
