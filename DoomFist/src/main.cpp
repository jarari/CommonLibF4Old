#include <Windows.h>
char tempbuf[8192] = { 0 };
char* _MESSAGE(const char* fmt, ...) {
	va_list args;

	va_start(args, fmt);
	vsprintf_s(tempbuf, sizeof(tempbuf), fmt, args);
	va_end(args);

	return tempbuf;
}

ActiveEffect* GetActiveMagicEffect(Actor* a, TESForm* effectForm) {
	ActiveEffectList* aeList = a->GetActiveEffectList();
	if (!aeList)
		return nullptr;
	for (auto it = aeList->data.begin(); it != aeList->data.end(); ++it) {
		if (it->get() && it->get()->effect) {
			EffectSetting* avEffectSetting = *(EffectSetting**)((uint64_t)(it->get()->effect) + 0x10);
			if (avEffectSetting && avEffectSetting == effectForm) {
				return it->get();
			}
		}
	}
	return nullptr;
}

bool HasMagicEffectThenModifyDuration(GameScript::RefrOrInventoryObj thisObj, Actor* a, TESForm* effectForm, float f)
{
	if (!a || !effectForm)
		return false;
	ActiveEffect* ae = GetActiveMagicEffect(a, effectForm);
	if (!ae)
		return false;
	ae->elapsed = 0.0f;
	ae->duration = f;
	return true;
}

bool HasMagicEffectThenExtendDuration(std::monostate, Actor* a, TESForm* effectForm, float f) {
	if (!a || !effectForm)
		return false;
	ActiveEffect* ae = GetActiveMagicEffect(a, effectForm);
	if (!ae)
		return false;
	ae->duration += f;
	return true;
}

int32_t GetKnockState(std::monostate, Actor* a) {
	if (!a)
		return 0;
	return a->knockState;
}

// GunState
// 0001 = Reloading
// 0011 = Sighted
// 0111 = Firing
void ForceSightedState(GameScript::RefrOrInventoryObj thisObj, Actor* a, bool b) 
{
	if (b)
		a->gunState = 0x3;
	else
		a->gunState = 0;
	a->SetInIronSightsImpl(b);
}

void RemoveCloak(std::monostate, Actor* a, TESForm* effectForm) {
	if (!a || !effectForm)
		return;
	//logger::warn("RemoveCloak"sv);
	ActiveEffect* ae = GetActiveMagicEffect(a, effectForm);
	if (!ae)
		return;
	ae->elapsed = 50.0f;
	ae->duration = 0.1f;
}

void ModifyDuration(ActiveEffect& mgef, float f)
{
	mgef.elapsed = 0.0f;
	mgef.duration = f * mgef.magnitude;
}

/*std::vector<SpellItem*> cachedSpells;
std::vector<BGSKeyword*> cachedKeywords;
void CacheSpellsAndKeywords() {
	for (int i = 0; i < 8; i++) {
		auto it = cachedSpells.begin();
		cachedSpells.insert(it, 0);
		++it;
		auto kit = cachedKeywords.begin();
		cachedKeywords.insert(kit, 0);
		++kit;
	}
	const auto& [map, lock] = TESForm::GetAllForms();
	BSAutoReadLock l{ lock };
	if (map) {
		for (auto k = map->begin(); k != map->end(); ++k) {
			if (k->second->formType == ENUM_FORM_ID::kSPEL) {
				if (strcmp(((SpellItem*)(k->second))->fullName.c_str(), "DoomFist Rising Uppercut Damage") == 0) {
					cachedSpells[0] = (SpellItem*)k->second;
				}
				else if (strcmp(((SpellItem*)(k->second))->fullName.c_str(), "DoomFist Rising Uppercut Paralysis Double") == 0) {
					cachedSpells[1] = (SpellItem*)k->second;
				}
				else if (strcmp(((SpellItem*)(k->second))->fullName.c_str(), "DoomFist Rising Uppercut Cryo") == 0) {
					cachedSpells[2] = (SpellItem*)k->second;
				}
				else if (strcmp(((SpellItem*)(k->second))->fullName.c_str(), "DoomFist Rising Uppercut Fire") == 0) {
					cachedSpells[3] = (SpellItem*)k->second;
				}
				else if (strcmp(((SpellItem*)(k->second))->fullName.c_str(), "DoomFist Rocket Punch Damage") == 0) {
					cachedSpells[4] = (SpellItem*)k->second;
				}
				else if (strcmp(((SpellItem*)(k->second))->fullName.c_str(), "DoomFist Rocket Punch Explosion75") == 0) {
					cachedSpells[5] = (SpellItem*)k->second;
				}
				else if (strcmp(((SpellItem*)(k->second))->fullName.c_str(), "DoomFist Rocket Punch Electric") == 0) {
					cachedSpells[6] = (SpellItem*)k->second;
				}
				else if (strcmp(((SpellItem*)(k->second))->fullName.c_str(), "DoomFist Rocket Punch Radiation") == 0) {
					cachedSpells[7] = (SpellItem*)k->second;
				}
			}
			else if (k->second->formType == ENUM_FORM_ID::kKYWD) {
				if (strcmp(((BGSKeyword*)(k->second))->GetFormEditorID(), "dn_RisingUppercut_Default") == 0) {
					cachedKeywords[0] = (BGSKeyword*)k->second;
				}
				else if (strcmp(((BGSKeyword*)(k->second))->GetFormEditorID(), "dn_RisingUppercut_ParalysisDouble") == 0) {
					cachedKeywords[1] = (BGSKeyword*)k->second;
				}
				else if (strcmp(((BGSKeyword*)(k->second))->GetFormEditorID(), "dn_RisingUppercut_Cryo") == 0) {
					cachedKeywords[2] = (BGSKeyword*)k->second;
				}
				else if (strcmp(((BGSKeyword*)(k->second))->GetFormEditorID(), "dn_RisingUppercut_Fire") == 0) {
					cachedKeywords[3] = (BGSKeyword*)k->second;
				}
				else if (strcmp(((BGSKeyword*)(k->second))->GetFormEditorID(), "dn_RocketPunch_Default") == 0) {
					cachedKeywords[4] = (BGSKeyword*)k->second;
				}
				else if (strcmp(((BGSKeyword*)(k->second))->GetFormEditorID(), "dn_RocketPunch_Explosion75") == 0) {
					cachedKeywords[5] = (BGSKeyword*)k->second;
				}
				else if (strcmp(((BGSKeyword*)(k->second))->GetFormEditorID(), "dn_RocketPunch_Electric") == 0) {
					cachedKeywords[6] = (BGSKeyword*)k->second;
				}
				else if (strcmp(((BGSKeyword*)(k->second))->GetFormEditorID(), "dn_RocketPunch_Radiation") == 0) {
					cachedKeywords[7] = (BGSKeyword*)k->second;
				}
			}
		}
	}
}

//type 0 = Rising Uppercut, type 1 = Rocket Punch
SpellItem* GetProperSpell(std::monostate, Actor* a, int type) {
	BSTArray<EquippedItem> equipped = a->currentProcess->middleHigh->equippedItems;
	BGSKeywordForm* keywordForm = nullptr;
	for (auto it = equipped.begin(); it != equipped.end(); ++it) {
		if (it->equipIndex.index == 0 && it->item.instanceData.get()) {
			keywordForm = ((TESObjectWEAP::InstanceData*)it->item.instanceData.get())->keywords;
			break;
		}
	}

	if (!keywordForm)
		return nullptr;

	BGSKeyword** keywords = keywordForm->keywords;
	bool found = false;
	int i = type * 4;
	size_t imax = cachedKeywords.size() + (type - 1) * 4;
	while (!found && i < imax) {
		uint32_t j = 0;
		while (!found && j < keywordForm->numKeywords) {
			if (keywords[j] == cachedKeywords.at(i)) {
				found = true;
			}
			++j;
		}
		++i;
	}
	if (found) {
		return cachedSpells.at(--i);
	}
	return nullptr;
}*/

bool RegisterFuncs(BSScript::IVirtualMachine* a_vm)
{
	a_vm->BindNativeMethod("DoomFistBase", "HasMagicEffectThenModifyDuration", HasMagicEffectThenModifyDuration, true);
	a_vm->BindNativeMethod("DoomFistBase", "ForceSightedState", ForceSightedState, true);
	a_vm->BindNativeMethod("DoomFistBase", "RemoveCloak", RemoveCloak, true);
	a_vm->BindNativeMethod("DoomFistBase", "HasMagicEffectThenExtendDuration", HasMagicEffectThenExtendDuration, true);
	a_vm->BindNativeMethod("DoomFistBase", "GetKnockState", GetKnockState, true);
	//a_vm->BindNativeMethod("DoomFistMarkerBase", "GetProperSpell", GetProperSpell, true);
	a_vm->BindNativeMethod("DoomFistReloaderBase", "ModifyDuration", ModifyDuration, true);
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

	*path /= "DoomFist.log"sv;
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
	a_info->name = "DoomFist";
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
	return true;
}
