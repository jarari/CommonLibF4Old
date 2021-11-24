#include <regex>
using namespace RE;
using std::string;
using std::regex;
using std::smatch;

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

void InitializePlugin() {
	TESDataHandler* dh = TESDataHandler::GetSingleton();
	BSTArray<TESObjectARMO*> armors = dh->GetFormArray<TESObjectARMO>();
	regex pattern_vistag("\\[([A-Za-z]+)(?!\\])(.*?)\\]");
	regex pattern_slottag("\\[([0-9,]+)\\]");
	for (auto it = armors.begin(); it != armors.end(); ++it) {
		TESObjectARMO* armor = *it;
		_MESSAGE("Armor : %s FormID : %llx", armor->fullName.c_str(), armor->formID);
		string fullname{ armor->GetFullName() };
		smatch visresult;
		smatch slotresult;
		
		bool shouldInsert = true;
		__int64 insertionIndex = 0;

		if (regex_search(fullname, visresult, pattern_vistag)) {
			_MESSAGE("VIS tag found. Suffix %s", visresult.suffix().str().c_str());
			if (regex_search(visresult.suffix().str(), pattern_slottag)) {
				shouldInsert = false;
				_MESSAGE("Slot tag found. Skipping...");
			}
			else if (visresult.position(0) == 0) {
				insertionIndex = visresult[0].length();
			}
		}
		else if (regex_search(fullname, slotresult, pattern_slottag) && slotresult.position(0) == 0) {
			shouldInsert = false;
			_MESSAGE("Slot tag found. Skipping...");
		}

		if (shouldInsert) {
			string slottag = "[";
			uint32_t bipedSlots = 0;
			for (auto aait = armor->modelArray.begin(); aait != armor->modelArray.end(); ++aait) {
				bipedSlots |= aait->armorAddon->bipedModelData.bipedObjectSlots;
			}
			if (bipedSlots & 0x1) {
				slottag += "30,";
			}
			if (bipedSlots & 0x2) {
				slottag += "31,";
			}
			if (bipedSlots & 0x4) {
				slottag += "32,";
			}
			if (bipedSlots & 0x8) {
				slottag += "33,";
			}
			if (bipedSlots & 0x10) {
				slottag += "34,";
			}
			if (bipedSlots & 0x20) {
				slottag += "35,";
			}
			if (bipedSlots & 0x40) {
				slottag += "36,";
			}
			if (bipedSlots & 0x80) {
				slottag += "37,";
			}
			if (bipedSlots & 0x100) {
				slottag += "38,";
			}
			if (bipedSlots & 0x200) {
				slottag += "39,";
			}
			if (bipedSlots & 0x400) {
				slottag += "40,";
			}
			if (bipedSlots & 0x800) {
				slottag += "41,";
			}
			if (bipedSlots & 0x1000) {
				slottag += "42,";
			}
			if (bipedSlots & 0x2000) {
				slottag += "43,";
			}
			if (bipedSlots & 0x4000) {
				slottag += "44,";
			}
			if (bipedSlots & 0x8000) {
				slottag += "45,";
			}
			if (bipedSlots & 0x10000) {
				slottag += "46,";
			}
			if (bipedSlots & 0x20000) {
				slottag += "47,";
			}
			if (bipedSlots & 0x40000) {
				slottag += "48,";
			}
			if (bipedSlots & 0x80000) {
				slottag += "49,";
			}
			if (bipedSlots & 0x100000) {
				slottag += "50,";
			}
			if (bipedSlots & 0x200000) {
				slottag += "51,";
			}
			if (bipedSlots & 0x400000) {
				slottag += "52,";
			}
			if (bipedSlots & 0x800000) {
				slottag += "53,";
			}
			if (bipedSlots & 0x1000000) {
				slottag += "54,";
			}
			if (bipedSlots & 0x2000000) {
				slottag += "55,";
			}
			if (bipedSlots & 0x4000000) {
				slottag += "56,";
			}
			if (bipedSlots & 0x8000000) {
				slottag += "57,";
			}
			if (bipedSlots & 0x10000000) {
				slottag += "58,";
			}
			if (bipedSlots & 0x20000000) {
				slottag += "59,";
			}
			if (bipedSlots & 0x40000000) {
				slottag += "60,";
			}
			if (bipedSlots & 0x80000000) {
				slottag += "61,";
			}
			slottag = slottag.substr(0, slottag.length() - 1);
			if (slottag.length() > 0) {
				slottag += "]";
				if (insertionIndex == 0) {
					(*it)->fullName = slottag + " " + fullname;
				}
				else {
					(*it)->fullName = fullname.substr(0, insertionIndex) + slottag + visresult.suffix().str();
				}
				_MESSAGE("New Name : %s", (*it)->fullName.c_str());
			}
			else {
				_MESSAGE("Failed to generate slot tag. Skipping...");
			}
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

	const F4SE::MessagingInterface* message = F4SE::GetMessagingInterface();
	message->RegisterListener([](F4SE::MessagingInterface::Message* msg) -> void {
		if (msg->type == F4SE::MessagingInterface::kGameDataReady) {
			InitializePlugin();
		}
	});

	return true;
}
