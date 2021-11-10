#include <SimpleIni.h>
using namespace RE;
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

namespace F4 {

	bool PlaySound(BGSSoundDescriptorForm* sndr, NiPoint3 pos, NiAVObject* node) {
		struct Unk {
			uint32_t unk00 = 0xFFFFFFFF;
			uint32_t unk04 = 0x0;
		};
		typedef bool* func_t(Unk&, BGSSoundDescriptorForm*, NiPoint3, NiAVObject*);
		REL::Relocation<func_t> func{ REL::ID(376497) };
		Unk u;
		return func(u, sndr, pos, node);
	}

}

REL::Relocation<uint64_t*> ptr_engineTime{ REL::ID(1280610) };
CSimpleIniA ini(true, false, false);
vector<BGSSoundDescriptorForm*> soundList;
BGSSoundDescriptorForm* hitSound;
uint64_t lastHitSound;

bool playOnAliveOnly = true;
int soundType = 0;
uint8_t freqShift = 0;
uint8_t freqVariance = 0;
uint16_t staticAttenuation = 700;

struct _TESHitEvent {
	NiPoint3 hitPos; //0x0000 
	uint32_t unk_0C; //0x000C
	NiPoint3 normal; //0x0010
	uint32_t unk_1C; //0x001C
	NiPoint3 normal2; //0x0020 melee hits return 0,0,0
	uint32_t unk_2C; //0x002C
	bhkNPCollisionObject* colObj; //0x0030 
	char pad_0x0038[0x18]; //0x0038
	BGSAttackData* attackData; //0x0050
	TESObjectWEAP* weapon; //0x0058 
	TESObjectWEAP::InstanceData* weaponInstance; //0x0060 
	char pad_0x0068[0x18]; //0x0068
	TESForm* unkitem; //0x0080 
	char pad_0x0088[0x8]; //0x0088
	float N000005CF; //0x0090 
	float N00000831; //0x0094 
	float N000005D0; //0x0098 
	float N00000834; //0x009C 
	char pad_0x00A0[0x10]; //0x00A0
	float N000005E7; //0x00B0 
	float N0000082E; //0x00B4 
	char pad_0x00B8[0x8]; //0x00B8
	float N000005E9; //0x00C0 
	char pad_0x00C4[0x5C]; //0x00C4
	TESObjectREFR* victim; //0x0120 
	char pad_0x0128[0x38]; //0x0128
	TESObjectREFR* attacker; //0x0160 
};

class HitEventSource : public BSTEventSource<_TESHitEvent> {
public:
	[[nodiscard]] static HitEventSource* GetSingleton() {
		REL::Relocation<HitEventSource*> singleton{ REL::ID(989868) };
		return singleton.get();
	}
};

class HitEventSink : public BSTEventSink<_TESHitEvent> {
public:
	virtual BSEventNotifyControl ProcessEvent(const _TESHitEvent& a_event, BSTEventSource<_TESHitEvent>* a_source) override {
		if ((a_event.attackData || a_event.weapon)) {
			if (a_event.attacker == PlayerCharacter::GetSingleton() && a_event.victim && a_event.victim->formType == ENUM_FORM_ID::kACHR) {
				if (*ptr_engineTime - lastHitSound > 10) {
					if (!playOnAliveOnly || (playOnAliveOnly && !a_event.victim->IsDead(true))) {
						F4::PlaySound(hitSound, a_event.attacker->data.location, a_event.attacker->Get3D());
						lastHitSound = *ptr_engineTime;
					}
				}
			}
		}
		return BSEventNotifyControl::kContinue;
	}
	HitEventSink() {};
	virtual ~HitEventSink() {};
	F4_HEAP_REDEFINE_NEW(HitEventSink);
};

void InitializePlugin() {
	soundList.push_back((BGSSoundDescriptorForm*)GetFormFromMod(std::string("SimpleImpact.esp"), 0x800));
	soundList.push_back((BGSSoundDescriptorForm*)GetFormFromMod(std::string("SimpleImpact.esp"), 0x801));
	soundList.push_back((BGSSoundDescriptorForm*)GetFormFromMod(std::string("SimpleImpact.esp"), 0x802));
	soundList.push_back((BGSSoundDescriptorForm*)GetFormFromMod(std::string("SimpleImpact.esp"), 0x803));
	soundList.push_back((BGSSoundDescriptorForm*)GetFormFromMod(std::string("SimpleImpact.esp"), 0x804));
	soundList.push_back((BGSSoundDescriptorForm*)GetFormFromMod(std::string("SimpleImpact.esp"), 0x805));
	soundList.push_back((BGSSoundDescriptorForm*)GetFormFromMod(std::string("SimpleImpact.esp"), 0x806));
	hitSound = soundList[0];
	for (auto it = soundList.begin(); it != soundList.end(); ++it) {
		_MESSAGE("HitSound %llx", *it);;
	}
}

void LoadConfigs() {
	ini.LoadFile("Data\\F4SE\\Plugins\\SimpleImpact.ini");
	soundType = min(max(std::stoi(ini.GetValue("HitSound", "SoundType", "1")), 1), 7);
	freqShift = (uint8_t)std::stoi(ini.GetValue("HitSound", "FrequencyShift", "0"));
	freqVariance = (uint8_t)std::stoi(ini.GetValue("HitSound", "FrequencyVariance", "0"));
	staticAttenuation = (uint16_t)std::stoi(ini.GetValue("HitSound", "StaticAttenuation", "7")) * 100;

	/* BGSSoundDescriptor
	* 0x8 BGSSoundCategory*
	* 0x18~0x28 BSTArray?
	* 0x38 int8 Frequency Shift
	* 0x39 int8 Frequency Variance
	* 0x3A int16 Priority
	* 0x3C int16 Static Attenuation * 100
	* 0x48 BGSSoundOutput
	*/

	hitSound = soundList[soundType - 1];
	*(uint8_t*)((uintptr_t)hitSound->impl + 0x38) = freqShift;
	*(uint8_t*)((uintptr_t)hitSound->impl + 0x39) = freqVariance;
	*(uint16_t*)((uintptr_t)hitSound->impl + 0x3C) = staticAttenuation;

	playOnAliveOnly = std::stoi(ini.GetValue("HitSound", "AliveOnly", "1")) > 0;

	_MESSAGE("Selected SoundType %d", soundType);;
	_MESSAGE("HitSound %llx", hitSound);
	_MESSAGE("FrequencyShift %d", freqShift);;
	_MESSAGE("FrequencyVariance %d", freqVariance);;
	_MESSAGE("StaticAttenuation %d", staticAttenuation);;
	_MESSAGE("AliveOnly %d", playOnAliveOnly);;
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

			HitEventSource* src = HitEventSource::GetSingleton();
			_MESSAGE("HitEventSource %llx", src);
			HitEventSink* sink = new HitEventSink();
			src->RegisterSink(sink);
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
