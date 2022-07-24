#include <regex>
#include <set>
#include <typeinfo>
using namespace RE;

char tempbuf[8192] = { 0 };
char* _MESSAGE(const char* fmt, ...) {
	va_list args;

	va_start(args, fmt);
	vsprintf_s(tempbuf, sizeof(tempbuf), fmt, args);
	va_end(args);

	return tempbuf;
}

#pragma region Morph Additon

#pragma endregion

#pragma region Papyrus

// Functions below are mostly from the ApplyMaterialSwap function in F4SE.
// Their method works amazing on most of objects, but it omits the skin tint data 
// because they are casting shaderMaterialBase as BSLightingShaderMaterialBase, not BSLightingShaderMaterialSkinTint

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

std::vector<float> GetSkinTint(std::monostate, Actor* refr, BSFixedString material) {
	std::vector<float> result = std::vector<float>({ 0, 0, 0, 0, 0 });
	NiAVObject* rootNode[2];
	rootNode[0] = refr ? refr->Get3D(false) : nullptr;
	rootNode[1] = refr ? refr->Get3D(true) : nullptr;

	int maxRoots = 2;
	if (rootNode[0] == rootNode[1])
		maxRoots = 1;

	for (int i = 0; i < maxRoots; ++i) {
		if (!rootNode[i])
			continue;

		Visit(rootNode[i], [&](NiAVObject* object) {
			if (result[4] == 1)
				return false;
			int64_t geometry = (int64_t)object->IsGeometry();
			if (geometry) {
				NiPointer<BSShaderProperty> shaderProperty(*(BSShaderProperty**)(geometry + 0x138));
				if (shaderProperty.get()) {
					const char* shaderPath = shaderProperty->name.c_str();
					std::string fullPath(shaderPath ? shaderPath : "");
					if (fullPath.length() == 0)
						return false;

					fullPath = std::regex_replace(fullPath, std::regex("/+|\\\\+"), "\\");									   // Replace multiple slashes or forward slashes with one backslash
					fullPath = std::regex_replace(fullPath, std::regex("^\\\\+"), "");										   // Remove all backslashes from the front
					fullPath = std::regex_replace(fullPath, std::regex(".*?materials\\\\", std::regex_constants::icase), "");  // Remove everything before and including the materials path
					BSFixedString fixedPath(fullPath.c_str());

					if (fixedPath == material) {
						float colorR = *(float*)((int64_t)shaderProperty->material + 0xC0);
						float colorG = *(float*)((int64_t)shaderProperty->material + 0xC4);
						float colorB = *(float*)((int64_t)shaderProperty->material + 0xC8);
						float colorA = *(float*)((int64_t)shaderProperty->material + 0xCC);
						result[0] = colorR;
						result[1] = colorG;
						result[2] = colorB;
						result[3] = colorA;
						result[4] = 1;
						//logger::warn(_MESSAGE("matching material found %f %f %f %f \nat property %llx", colorR, colorG, colorB, colorA, shaderProperty.get()));
					}
				}
			}
			return false;
		});
	}
	if (result[4] == 0) {
		TESNPC* npc = refr->GetNPC();
		result[0] = (float)npc->bodyTintColorR / 255.0f;
		result[1] = (float)npc->bodyTintColorG / 255.0f;
		result[2] = (float)npc->bodyTintColorB / 255.0f;
		result[3] = (float)npc->bodyTintColorA / 255.0f;
		result[4] = 1;
	}
	//logger::warn(_MESSAGE("GetSkinTint called result : %f %f %f %f %f", result[0], result[1], result[2], result[3], result[4]));
	return result;
}

void ApplySkinTint(std::monostate, Actor* refr, BSFixedString material, float r, float g, float b, float a) {
	//logger::warn(_MESSAGE("ApplySkinTint called"));

	TESNPC* npc = refr->GetNPC();
	npc->bodyTintColorR = (int8_t)(r * 255);
	npc->bodyTintColorG = (int8_t)(g * 255);
	npc->bodyTintColorB = (int8_t)(b * 255);
	npc->bodyTintColorA = (int8_t)(a * 255);
	//logger::warn(_MESSAGE("npc %llx", npc));

	NiAVObject* rootNode[2];
	rootNode[0] = refr ? refr->Get3D(false) : nullptr;
	rootNode[1] = refr ? refr->Get3D(true) : nullptr;

	int maxRoots = 2;
	if (rootNode[0] == rootNode[1])
		maxRoots = 1;

	for (int i = 0; i < maxRoots; ++i) {
		if (!rootNode[i])
			continue;

		Visit(rootNode[i], [&](NiAVObject* object) {
			int64_t geometry = (int64_t)object->IsGeometry();
			if (geometry) {
				NiPointer<BSShaderProperty> shaderProperty(*(BSShaderProperty**)(geometry + 0x138));
				if (shaderProperty.get()) {
					const char* shaderPath = shaderProperty->name.c_str();
					std::string fullPath(shaderPath ? shaderPath : "");
					if (fullPath.length() == 0)
						return false;

					fullPath = std::regex_replace(fullPath, std::regex("/+|\\\\+"), "\\");									   // Replace multiple slashes or forward slashes with one backslash
					fullPath = std::regex_replace(fullPath, std::regex("^\\\\+"), "");										   // Remove all backslashes from the front
					fullPath = std::regex_replace(fullPath, std::regex(".*?materials\\\\", std::regex_constants::icase), "");  // Remove everything before and including the materials path
					BSFixedString fixedPath(fullPath.c_str());

					if (fixedPath == material) {
						*(float*)((int64_t)shaderProperty->material + 0xC0) = r;
						*(float*)((int64_t)shaderProperty->material + 0xC4) = g;
						*(float*)((int64_t)shaderProperty->material + 0xC8) = b;
						*(float*)((int64_t)shaderProperty->material + 0xCC) = a;
						shaderProperty->lastRenderPassState = 0x7FFFFFFF; //Passing this flag will force the update
					}
				}
			}
			return false;
		});
	}
	refr->AddChange(CHANGE_TYPES::kNPCFace);
}

bool RegisterFuncs(RE::BSScript::IVirtualMachine* a_vm) {
	a_vm->BindNativeMethod("NanakoFramework", "ApplySkinTint", ApplySkinTint, true, true);
	a_vm->BindNativeMethod("NanakoFramework", "GetSkinTint", GetSkinTint, true);
	return true;
}

#pragma endregion

#pragma region Looksmenu

#pragma endregion

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Query(const F4SE::QueryInterface* a_f4se, F4SE::PluginInfo* a_info)
{
#ifndef NDEBUG
	auto sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
#else
	auto path = logger::log_directory();
	if (!path) {
		return false;
	}

	*path /= "NanakoFramework.log"sv;
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
	a_info->name = "NanakoFramework";
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

	logger::critical("Custom Nanako Framework plugin loaded. This plugin is designed to assist Papyrus scripts in Custom Nanako Framework."sv);
	const F4SE::PapyrusInterface* papyrus = F4SE::GetPapyrusInterface();
	bool succ = papyrus->Register(RegisterFuncs);
	if (succ) {
		logger::warn("Papyrus functions registered.");
	}
	return true;
}
