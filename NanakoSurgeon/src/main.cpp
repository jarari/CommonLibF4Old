#include <fstream>
#include <wtypes.h>
#include "nlohmann/json.hpp"
using namespace RE;
using std::ifstream;
using std::unordered_map;
using std::pair;
using std::vector;
using std::string;

class ProcessLists;

struct TranslationData {
	string bonename;
	string bonenameorig;
	NiPoint3 translation;
	float scale;
	bool ignoreXYZ;
	bool insertion;
	bool tponly;
	TranslationData(string _b, string _balt, NiPoint3 _t, float _s, bool _ig, bool _i, bool _tp) {
		bonename = _b;
		bonenameorig = _balt;
		translation = _t;
		scale = _s;
		ignoreXYZ = _ig;
		insertion = _i;
		tponly = _tp;
	}
};

REL::Relocation<uint64_t*> ptr_engineTime{ REL::ID(1280610) };
REL::Relocation<ProcessLists*> ptr_processLists{ REL::ID(474742) };
PlayerCharacter* p;
unordered_map<TESForm*, vector<TranslationData>> customRaceFemaleTranslations;
unordered_map<TESForm*, vector<TranslationData>> customRaceMaleTranslations;
unordered_map<TESForm*, vector<TranslationData>> customNPCTranslations;
unordered_map<NiNode*, NiNode*> parentMap;

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
	char* c = static_cast<char*>(mem);
	unsigned char* up = (unsigned char*)c;
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

NiNode* CreateBone(const char* name) {
	NiNode* newbone = new NiNode(0);
	newbone->name = name;
	_MESSAGE("%s created.", name);
	return newbone;
}

NiNode* InsertBone(NiNode* node, const char* name) {
	NiNode* parent = node->parent;
	NiNode* inserted = CreateBone(name);
	if (parent) {
		parent->DetachChild(node);
		parent->AttachChild(inserted, true);
		inserted = parent->GetObjectByName(name)->IsNode();
		parentMap.insert(pair<NiNode*, NiNode*>(node, parent));
	}
	if (inserted) {
		inserted->local.translate = NiPoint3();
		inserted->local.rotate.MakeIdentity();
		inserted->AttachChild(node, true);
		if (node->parent == inserted) {
			_MESSAGE("%s (%llx) inserted to %s (%llx).", name, inserted, node->name.c_str(), node);
			return inserted;
		}
	}
	return nullptr;
}

void CheckHierarchy(NiNode* original, NiNode* inserted) {
	if (original->parent != inserted) {
		_MESSAGE("Reparenting %s to %s", original->name.c_str(), inserted->name.c_str());
		NiNode* parent = original->parent;
		parent->DetachChild(original);
		parent->AttachChild(inserted, true);
		inserted->local.translate = NiPoint3();
		inserted->local.rotate.MakeIdentity();
		inserted->AttachChild(original, true);
	}
}

bool Visit(NiAVObject* obj, const std::function<bool(NiAVObject*)>& functor) {
	if (functor(obj))
		return true;

	NiPointer<NiNode> node(obj->IsNode());
	if (node) {
		for (auto it = node->children.begin(); it != node->children.end(); ++it) {
			NiPointer<NiAVObject> object(*it);
			if (object) {
				if (Visit(object.get(), functor))
					return true;
			}
		}
	}

	return false;
}

void ApplyBoneData(NiAVObject* node, TranslationData& data) {
	NiNode* bone = (NiNode * )node->GetObjectByName(data.bonename);
	if (bone) {
		if (!data.ignoreXYZ) {
			bone->local.translate = data.translation;
		}
		bone->local.scale = data.scale;
		if (data.insertion) {
			NiNode* boneorig = (NiNode*)node->GetObjectByName(data.bonenameorig);
			if (boneorig) {
				auto parentit = parentMap.find(boneorig);
				if (parentit != parentMap.end()) {
					CheckHierarchy(boneorig, bone);
					CheckHierarchy(bone, parentit->second);
				}
			}
		}
		//_MESSAGE("Bone %s x %f y %f z %f scale %f", data.bonename.c_str(), data.translation.x, data.translation.y, data.translation.z, data.scale);
	}
	else {
		if (data.insertion) {
			NiNode* boneorig = (NiNode*)node->GetObjectByName(data.bonenameorig);
			if (boneorig) {
				bone = InsertBone(boneorig, data.bonename.c_str());
			}
		}
	}
}

void DoSurgery(Actor* a) {
	int found = 0;
	TESNPC* npc = a->GetNPC();
	if (!npc) {
		_MESSAGE("Actor %llx - Couldn't find NPC data", a->formID);
		return;
	}
	//_MESSAGE("NPC %s", npc->GetFullName());
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
		if (node) {
			if (found == 2) {
				for (auto it = npcit->second.begin(); it != npcit->second.end(); ++it) {
					if (a != p || !it->tponly || node == a->Get3D(false)) {
						ApplyBoneData(node, *it);
					}
				}
			}
			else if (found == 1) {
				//_MESSAGE("Actor %llx Race %llx", a, a->race);
				for (auto it = raceit->second.begin(); it != raceit->second.end(); ++it) {
					if (a != p || !it->tponly || node == a->Get3D(false)) {
						ApplyBoneData(node, *it);
					}
				}
			}
		}
	}
}

void RegisterBoneData(auto iter, TESForm* form, unordered_map<TESForm*, vector<TranslationData>>& map) {
	for (auto boneit = iter.value().begin(); boneit != iter.value().end(); ++boneit) {
		float x = 0;
		float y = 0;
		float z = 0;
		float scale = 1;
		bool ignoreXYZ = false;
		bool insertion = false;
		bool tponly = false;
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
			else if (valit.key() == "IgnoreXYZ") {
				ignoreXYZ = valit.value().get<bool>();
			}
			else if (valit.key() == "Insertion") {
				insertion = valit.value().get<bool>();
			}
			else if (valit.key() == "ThirdPersonOnly") {
				tponly = valit.value().get<bool>();
			}
		}
		string bonename = boneit.key();
		if (insertion) {
			bonename += "SurgeonInserted";
		}
		auto existit = map.find(form);
		if (existit == map.end()) {
			map.insert(pair<TESForm*, vector<TranslationData>>(form, vector<TranslationData> {
				TranslationData(bonename, boneit.key(), NiPoint3(x, y, z), scale, ignoreXYZ, insertion, tponly)
			}));
		}
		else {
			existit->second.push_back(TranslationData(bonename, boneit.key(), NiPoint3(x, y, z), scale, ignoreXYZ, insertion, tponly));
		}
		_MESSAGE("Registered Bone %s X %f Y %f Z %f Scale %f IgnoredXYZ %d Insertion %d ThirdPersonOnly %d", boneit.key().c_str(), x, y, z, scale, ignoreXYZ, insertion, tponly);
	}
}

void DoGlobalSurgery() {
	BSTArray<ActorHandle>* highActorHandles = (BSTArray<ActorHandle>*)(ptr_processLists.address() + 0x40);
	//_MESSAGE("Num highActorHandles %d.", highActorHandles->size());
	if (highActorHandles->size() > 0) {
		for (auto it = highActorHandles->begin(); it != highActorHandles->end(); ++it) {
			Actor* a = it->get().get();
			if(a && a->Get3D())
				DoSurgery(a);
		}
	}
}

class CharacterMoveFinishEventWatcher {
public:
	typedef BSEventNotifyControl (CharacterMoveFinishEventWatcher::* FnProcessEvent)(bhkCharacterMoveFinishEvent& evn, BSTEventSource<bhkCharacterMoveFinishEvent>* dispatcher);

	BSEventNotifyControl HookedProcessEvent(bhkCharacterMoveFinishEvent& evn, BSTEventSource<bhkCharacterMoveFinishEvent>* src) {
		Actor* a = (Actor*)((uintptr_t)this - 0x150);
		DoSurgery(a);
		FnProcessEvent fn = fnHash.at(*(uint64_t*)this);
		return fn ? (this->*fn)(evn, src) : BSEventNotifyControl::kContinue;
	}

	static void HookSink(uint64_t vtable) {
		auto it = fnHash.find(vtable);
		if (it == fnHash.end()) {
			FnProcessEvent fn = SafeWrite64Function(vtable + 0x8, &CharacterMoveFinishEventWatcher::HookedProcessEvent);
			fnHash.insert(std::pair<uint64_t, FnProcessEvent>(vtable, fn));
			//_MESSAGE("Hooked bhkCharacterMoveFinishEvent %llx", vtable);
		}
	}

	static void UnHookSink(uint64_t vtable) {
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

struct TESObjectLoadedEvent {
	uint32_t formId;			//00
	uint8_t loaded;				//08
};

class ObjectLoadedEventSource : public BSTEventSource<TESObjectLoadedEvent> {
public:
	[[nodiscard]] static ObjectLoadedEventSource* GetSingleton() {
		REL::Relocation<ObjectLoadedEventSource*> singleton{ REL::ID(416662) };
		return singleton.get();
	}
};

class ObjectLoadWatcher : public BSTEventSink<TESObjectLoadedEvent> {
public:
	virtual BSEventNotifyControl ProcessEvent(const TESObjectLoadedEvent& evn, BSTEventSource<TESObjectLoadedEvent>* a_source) {
		TESForm* form = TESForm::GetFormByID(evn.formId);
		if (form && form->formType == ENUM_FORM_ID::kACHR) {
			Actor* a = static_cast<Actor*>(form);
			if (a->Get3D() && !evn.loaded) {
				Visit(a->Get3D(), [&](NiAVObject* obj) {
					NiNode* node = obj->IsNode();
					if(node)
						parentMap.erase(node);
					return false;
				});
			}
		}
		return BSEventNotifyControl::kContinue;
	}
	F4_HEAP_REDEFINE_NEW(ObjectLoadWatcher);
};

class CellAttachDetachWatcher : public BSTEventSink<CellAttachDetachEvent> {
public:
	virtual BSEventNotifyControl ProcessEvent(const CellAttachDetachEvent& evn, BSTEventSource<CellAttachDetachEvent>* a_source) {
		if (evn.type == CellAttachDetachEvent::EVENT_TYPE::kPostAttach) {
			//_MESSAGE("Cell Attached");
			DoGlobalSurgery();
		}
		return BSEventNotifyControl::kContinue;
	}
	F4_HEAP_REDEFINE_NEW(CellAttachDetachWatcher);
};

void InitializePlugin() {
	p = PlayerCharacter::GetSingleton();
	uint64_t PCVtable = REL::Relocation<uint64_t>{PlayerCharacter::VTABLE[13]}.address();
	uint64_t ActorVtable = REL::Relocation<uint64_t>{ Actor::VTABLE[13] }.address();
	CharacterMoveFinishEventWatcher::HookSink(PCVtable);
	CharacterMoveFinishEventWatcher::HookSink(ActorVtable);
	ObjectLoadWatcher* olw = new ObjectLoadWatcher();
	ObjectLoadedEventSource::GetSingleton()->RegisterSink(olw);
	CellAttachDetachWatcher* cadw = new CellAttachDetachWatcher();
	CellAttachDetachEventSource::CellAttachDetachEventSourceSingleton::GetSingleton().source.RegisterSink(cadw);
}

void LoadConfigs() {
	customRaceFemaleTranslations.clear();
	customRaceMaleTranslations.clear();
	customNPCTranslations.clear();
	ifstream reader;
	reader.open("Data\\F4SE\\Plugins\\NanakoSurgeon.json");
	nlohmann::json customData;
	reader >> customData;
	for (auto pluginit = customData.begin(); pluginit != customData.end(); ++pluginit) {
		for (auto formit = pluginit.value().begin(); formit != pluginit.value().end(); ++formit) {
			TESForm* form = GetFormFromMod(pluginit.key(), std::stoi(formit.key(), 0, 16));
			if (form) {
				if (form->GetFormType() == ENUM_FORM_ID::kNPC_) {
					if (form->formID == 0x7) {
						_MESSAGE("Player");
					}
					else {
						_MESSAGE("NPC %s", ((TESNPC*)form)->GetFullName());
					}
					RegisterBoneData(formit, form, customNPCTranslations);
				}
				else if (form->GetFormType() == ENUM_FORM_ID::kRACE) {
					for (auto sexit = formit.value().begin(); sexit != formit.value().end(); ++sexit) {
						if (sexit.key() == "Female") {
							_MESSAGE("Race %s Female", form->GetFormEditorID());
							RegisterBoneData(sexit, form, customRaceFemaleTranslations);
						}
						else if (sexit.key() == "Male") {
							_MESSAGE("Race %s Male", form->GetFormEditorID());
							RegisterBoneData(sexit, form, customRaceMaleTranslations);
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
			DoGlobalSurgery();
		}
		else if (msg->type == F4SE::MessagingInterface::kNewGame) {
			LoadConfigs();
		}
	});

	return true;
}
