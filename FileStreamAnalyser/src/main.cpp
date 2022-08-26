#include <Utilities.h>
using namespace RE;
using std::unordered_map;
using std::queue;

PlayerCharacter* p;
ConsoleLog* clog;
REL::Relocation<uintptr_t> ptr_PCUpdateMainThread{ REL::ID(633524), 0x22D };
REL::Relocation<uintptr_t> ptr_MainRenderAEnd{ REL::ID(1316702), 0xD0 };
uintptr_t PCUpdateMainThreadOrig;
uintptr_t MainRenderAEndOrig;
uintptr_t MemFSVtable = 0;
uintptr_t LooseFSVtable = 0;
uintptr_t LooseFASVtable = 0;
const static float stutterThreshold = 0.04f;
const static int maxQueue = 5;
const static int maxSample = 10;
static int sampleCount = 0;
static float deltaAvg = 0.f;
static float lastRun;
static queue<std::pair<std::string, float>> lastFileLoaded;
static queue<float> deltaQueue;

char consolebuf[8192] = { 0 };
void PrintConsole(const char* fmt, ...) {
	if (clog) {
		va_list args;

		va_start(args, fmt);
		vsnprintf(consolebuf, sizeof(consolebuf), fmt, args);
		va_end(args);
		clog->AddString(consolebuf);
		clog->AddString("\n");
	}
}

class FileStreamWatcher {
public:
	typedef uint32_t (FileStreamWatcher::* FnDoOpen)();
	uint32_t HookedDoOpen() {
		std::string filename;
		uintptr_t vtable = *(uint64_t*)this;
		if (vtable == MemFSVtable || vtable == LooseFSVtable) {
			F4::BSResource::Stream* stream = (F4::BSResource::Stream*)this;
			filename = std::string(stream->prefix) + std::string(stream->name);
		}
		else {
			F4::BSResource::AsyncStream* stream = (F4::BSResource::AsyncStream*)this;
			filename = std::string(stream->prefix) + std::string(stream->name);
		}
		lastFileLoaded.push(std::pair<std::string, float>(filename, *F4::ptr_engineTime));
		if (lastFileLoaded.size() > maxQueue) {
			lastFileLoaded.pop();
		}
		FnDoOpen fn = fnHash.at(vtable);
		return fn ? (this->*fn)() : 1;
	}

	void HookSink(uint64_t vtable) {
		auto it = fnHash.find(vtable);
		if (it == fnHash.end()) {
			FnDoOpen fn = SafeWrite64Function(vtable + 0x8, &FileStreamWatcher::HookedDoOpen);
			fnHash.insert(std::pair<uint64_t, FnDoOpen>(vtable, fn));
		}
	}

	F4_HEAP_REDEFINE_NEW(FileStreamWatcher);
protected:
	static std::unordered_map<uint64_t, FnDoOpen> fnHash;
};
std::unordered_map<uint64_t, FileStreamWatcher::FnDoOpen> FileStreamWatcher::fnHash;

void HookedMainRenderAEnd(void* ptr) {
	float curtime = *F4::ptr_engineTime;

	float deltaTime = curtime - lastRun;
	deltaQueue.push(deltaTime);
	deltaAvg = deltaAvg * sampleCount + deltaTime;
	if (sampleCount < maxSample) {
		++sampleCount;
	}
	else {
		deltaAvg -= deltaQueue.front();
		deltaQueue.pop();
	}
	deltaAvg /= sampleCount;

	if (deltaTime - deltaAvg >= stutterThreshold) {
		PrintConsole("Stutter occured at %f secs. Tick Avg : %f. Tick Hit : %f", curtime, round(1.f / deltaAvg), round(1.f / deltaTime));
		_MESSAGE("Stutter occured at %f secs. Tick Avg : %f. Tick Hit : %f", curtime, round(1.f / deltaAvg), round(1.f / deltaTime));
		for (int i = 0; i < lastFileLoaded.size(); ++i) {
			std::pair<std::string, float> fileInfo = lastFileLoaded.front();
			PrintConsole("File %s loaded on %f secs", fileInfo.first.c_str(), fileInfo.second);
			_MESSAGE("File %s loaded on %f secs", fileInfo.first.c_str(), fileInfo.second);
			lastFileLoaded.pop();
		}
	}
	lastRun = curtime;

	typedef void (*FnUpdate)(void*);
	FnUpdate fn = (FnUpdate)MainRenderAEndOrig;
	if (fn)
		(*fn)(ptr);
}

void InitializePlugin() {
	p = PlayerCharacter::GetSingleton();
	clog = ConsoleLog::GetSingleton();
	FileStreamWatcher* FSWatcher = new FileStreamWatcher();
	MemFSVtable = REL::Relocation<uint64_t>{ VTABLE::BSResource__MemoryFileStream[0] }.address();
	FSWatcher->HookSink(MemFSVtable);
	LooseFSVtable = REL::Relocation<uint64_t>{ VTABLE::BSResource____LooseFileStream[0] }.address();
	FSWatcher->HookSink(LooseFSVtable);
	LooseFASVtable = REL::Relocation<uint64_t>{ VTABLE::BSResource____LooseFileAsyncStream[0] }.address();
	FSWatcher->HookSink(LooseFASVtable);
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
	F4SE::AllocTrampoline(8 * 8);

	return true;
}

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Load(const F4SE::LoadInterface* a_f4se)
{
	F4SE::Init(a_f4se);

	F4SE::Trampoline& trampoline = F4SE::GetTrampoline();
	//PCUpdateMainThreadOrig = trampoline.write_call<5>(ptr_MainRenderAEnd.address(), &HookedUpdate);
	MainRenderAEndOrig = (uintptr_t)SafeWrite64Function(ptr_MainRenderAEnd.address(), &HookedMainRenderAEnd);

	const F4SE::MessagingInterface* message = F4SE::GetMessagingInterface();
	message->RegisterListener([](F4SE::MessagingInterface::Message* msg) -> void {
		if (msg->type == F4SE::MessagingInterface::kGameDataReady) {
			InitializePlugin();
		}
	});

	return true;
}
