#include <fstream>
#include <wtypes.h>
#include "nlohmann/json.hpp"
using namespace RE;
using std::ifstream;
using std::unordered_map;
using std::pair;
using std::vector;
using std::string;

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

struct TranslationData {
	string bonename;
	NiPoint3 translation;
	float scale;
	TranslationData(string _b, NiPoint3 _t, float _s) {
		bonename = _b;
		translation = _t;
		scale = _s;
	}
};

class CharacterMoveFinishEventWatcher;

REL::Relocation<uint64_t*> ptr_engineTime{ REL::ID(1280610) };
PlayerCharacter* p;
unordered_map<TESForm*, vector<TranslationData>> customRaceFemaleTranslations;
unordered_map<TESForm*, vector<TranslationData>> customRaceMaleTranslations;
unordered_map<TESForm*, vector<TranslationData>> customNPCTranslations;
CharacterMoveFinishEventWatcher* PCWatcher;
CharacterMoveFinishEventWatcher* ActorWatcher;


class CharacterMoveFinishEventWatcher {
public:
	typedef BSEventNotifyControl (CharacterMoveFinishEventWatcher::* FnProcessEvent)(bhkCharacterMoveFinishEvent& evn, BSTEventSource<bhkCharacterMoveFinishEvent>* dispatcher);

	BSEventNotifyControl HookedProcessEvent(bhkCharacterMoveFinishEvent& evn, BSTEventSource<bhkCharacterMoveFinishEvent>* src) {
		Actor* a = (Actor*)((uintptr_t)this - 0x150);
		if (a->Get3D()) {
			int found = 0;
			TESNPC* npc = a->GetNPC();
			unordered_map<TESForm*, vector<TranslationData>>::iterator raceit;
			if (npc->GetSex() == 0) {
				raceit = customRaceMaleTranslations.find(a->race);
				if (raceit != customRaceMaleTranslations.end()) {
					found = 1;
				}
			}
			else {
				raceit = customRaceFemaleTranslations.find(a->race);
				if (raceit != customRaceFemaleTranslations.end()) {
					found = 1;
				}
			}
			auto npcit = customNPCTranslations.find(npc);
			if (npcit != customNPCTranslations.end()) {
				found = 2;
			}
			if (found > 0) {
				NiAVObject* node = a->Get3D();
				NiAVObject* bone;
				if (found == 2) {
					for (auto it = npcit->second.begin(); it != npcit->second.end(); ++it) {
						bone = node->GetObjectByName(it->bonename);
						if (bone) {
							bone->local.translate = it->translation;
							bone->local.scale = it->scale;
						}
					}
				}
				else if (found == 1) {
					//_MESSAGE("Actor %llx Race %llx", a, a->race);
					for (auto it = raceit->second.begin(); it != raceit->second.end(); ++it) {
						bone = node->GetObjectByName(it->bonename);
						//_MESSAGE("Bone %s", it->bonename.c_str());
						if (bone) {
							bone->local.translate = it->translation;
							bone->local.scale = it->scale;
							//_MESSAGE("X %f Y %f Z %f Scale %f", bone->local.translate.x, bone->local.translate.y, bone->local.translate.z, bone->local.scale);
						}
					}
				}
			}
		}
		FnProcessEvent fn = fnHash.at(*(uint64_t*)this);
		return fn ? (this->*fn)(evn, src) : BSEventNotifyControl::kContinue;
	}

	void HookSink(uint64_t vtable) {
		auto it = fnHash.find(vtable);
		if (it == fnHash.end()) {
			FnProcessEvent fn = SafeWrite64Function(vtable + 0x8, &CharacterMoveFinishEventWatcher::HookedProcessEvent);
			fnHash.insert(std::pair<uint64_t, FnProcessEvent>(vtable, fn));
		}
	}

	void UnHookSink(uint64_t vtable) {
		auto it = fnHash.find(vtable);
		if (it == fnHash.end())
			return;
		SafeWrite64Function(vtable + 0x8, it->second);
		fnHash.erase(it);
	}

	F4_HEAP_REDEFINE_NEW(CharacterMoveFinishEventWatcher);
protected:
	static std::unordered_map<uint64_t, FnProcessEvent> fnHash;
};
std::unordered_map<uint64_t, CharacterMoveFinishEventWatcher::FnProcessEvent> CharacterMoveFinishEventWatcher::fnHash;

class MenuWatcher : public BSTEventSink<MenuOpenCloseEvent> {
	virtual BSEventNotifyControl ProcessEvent(const MenuOpenCloseEvent& evn, BSTEventSource<MenuOpenCloseEvent>* src) override {
		if (evn.menuName == BSFixedString("LoadingMenu")) {
			uint64_t PCVtable = REL::Relocation<uint64_t>{ PlayerCharacter::VTABLE[13] }.address();
			uint64_t ActorVtable = REL::Relocation<uint64_t>{ Actor::VTABLE[13] }.address();
			if (!PCWatcher)
				PCWatcher = new CharacterMoveFinishEventWatcher();
			if (!ActorWatcher)
				ActorWatcher = new CharacterMoveFinishEventWatcher();
			if (evn.opening) {
				PCWatcher->UnHookSink(PCVtable);
				ActorWatcher->UnHookSink(ActorVtable);
				_MESSAGE("Unhook from event");
			}
			else {
				PCWatcher->HookSink(PCVtable);
				ActorWatcher->HookSink(ActorVtable);
				_MESSAGE("Hook to event");
			}
		}
		return BSEventNotifyControl::kContinue;
	}
public:
	F4_HEAP_REDEFINE_NEW(MenuWatcher);
};

void InitializePlugin() {
	p = PlayerCharacter::GetSingleton();
	MenuWatcher* mw = new MenuWatcher();
	UI::GetSingleton()->GetEventSource<MenuOpenCloseEvent>()->RegisterSink(mw);
}

void LoadConfigs() {
	customRaceFemaleTranslations.clear();
	customRaceMaleTranslations.clear();
	customNPCTranslations.clear();
	ifstream reader;
	reader.open("Data\\F4SE\\Plugins\\NanakoHeadRelocator.json");
	nlohmann::json customData;
	reader >> customData;
	for (auto pluginit = customData.begin(); pluginit != customData.end(); ++pluginit) {
		for (auto formit = pluginit.value().begin(); formit != pluginit.value().end(); ++formit) {
			TESForm* form = GetFormFromMod(pluginit.key(), std::stoi(formit.key(), 0, 16));
			if (form) {
				if (form->GetFormType() == ENUM_FORM_ID::kNPC_) {
					for (auto boneit = formit.value().begin(); boneit != formit.value().end(); ++boneit) {
						float x = 0;
						float y = 0;
						float z = 0;
						float scale = 1;
						for (auto valit = boneit.value().begin(); valit != boneit.value().end(); ++valit) {
							if (valit.key() == "X") {
								x = valit.value().get<float>();
							}
							else if (valit.key() == "Y") {
								y = valit.value().get<float>();
							}
							else if (valit.key() == "Z") {
								z = valit.value().get<float>();
							}
							else if (valit.key() == "Scale") {
								scale = valit.value().get<float>();
							}
						}
						auto existit = customNPCTranslations.find(form);
						if (existit == customNPCTranslations.end()) {
							customNPCTranslations.insert(pair<TESForm*, vector<TranslationData>>(form, vector<TranslationData> {
								TranslationData(boneit.key(), NiPoint3(x, y, z), scale)
							}));
						}
						else {
							existit->second.push_back(TranslationData(boneit.key(), NiPoint3(x, y, z), scale));
						}
						_MESSAGE("NPC %s Bone %s X %f Y %f Z %f Scale %f", ((TESNPC*)form)->GetFullName(), boneit.key().c_str(), x, y, z, scale);
					}
				}
				else if (form->GetFormType() == ENUM_FORM_ID::kRACE) {
					for (auto sexit = formit.value().begin(); sexit != formit.value().end(); ++sexit) {
						if (sexit.key() == "Female") {
							for (auto boneit = sexit.value().begin(); boneit != sexit.value().end(); ++boneit) {
								float x = 0;
								float y = 0;
								float z = 0;
								float scale = 1;
								for (auto valit = boneit.value().begin(); valit != boneit.value().end(); ++valit) {
									if (valit.key() == "X") {
										x = valit.value().get<float>();
									}
									else if (valit.key() == "Y") {
										y = valit.value().get<float>();
									}
									else if (valit.key() == "Z") {
										z = valit.value().get<float>();
									}
									else if (valit.key() == "Scale") {
										scale = valit.value().get<float>();
									}
								}
								auto existit = customRaceFemaleTranslations.find(form);
								if (existit == customRaceFemaleTranslations.end()) {
									customRaceFemaleTranslations.insert(pair<TESForm*, vector<TranslationData>>(form, vector<TranslationData> {
										TranslationData(boneit.key(), NiPoint3(x, y, z), scale)
									}));
								}
								else {
									existit->second.push_back(TranslationData(boneit.key(), NiPoint3(x, y, z), scale));
								}
								_MESSAGE("Race %s Female Bone %s X %f Y %f Z %f Scale %f", form->GetFormEditorID(), boneit.key().c_str(), x, y, z, scale);
							}
						}
						else if (sexit.key() == "Male") {
							for (auto boneit = sexit.value().begin(); boneit != sexit.value().end(); ++boneit) {
								float x = 0;
								float y = 0;
								float z = 0;
								float scale = 1;
								for (auto valit = boneit.value().begin(); valit != boneit.value().end(); ++valit) {
									if (valit.key() == "X") {
										x = valit.value().get<float>();
									}
									else if (valit.key() == "Y") {
										y = valit.value().get<float>();
									}
									else if (valit.key() == "Z") {
										z = valit.value().get<float>();
									}
									else if (valit.key() == "Scale") {
										scale = valit.value().get<float>();
									}
								}
								auto existit = customRaceMaleTranslations.find(form);
								if (existit == customRaceMaleTranslations.end()) {
									customRaceMaleTranslations.insert(pair<TESForm*, vector<TranslationData>>(form, vector<TranslationData> {
										TranslationData(boneit.key(), NiPoint3(x, y, z), scale)
									}));
								}
								else {
									existit->second.push_back(TranslationData(boneit.key(), NiPoint3(x, y, z), scale));
								}
								_MESSAGE("Race %s Male Bone %s X %f Y %f Z %f Scale %f", form->GetFormEditorID(), boneit.key().c_str(), x, y, z, scale);
							}
						}
					}
				}
			}
		}
	}
	reader.close();
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
		else if (msg->type == F4SE::MessagingInterface::kPostLoadGame) {
			LoadConfigs();
		}
		else if (msg->type == F4SE::MessagingInterface::kNewGame) {
			LoadConfigs();
		}
	});

	return true;
}
