#include <SimpleIni.h>
#include <unordered_map>
#include <Windows.h>
#define MATH_PI 3.14159265358979323846
using namespace RE;

#pragma region Utilities

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

struct Quaternion {
public:
	float x, y, z, w;
	Quaternion(float _x, float _y, float _z, float _w);
	float Norm();
	NiMatrix3 ToRotationMatrix33();
};

Quaternion::Quaternion(float _x, float _y, float _z, float _w) {
	x = _x;
	y = _y;
	z = _z;
	w = _w;
}

float Quaternion::Norm() {
	return x * x + y * y + z * z + w * w;
}

void SetMatrix33(float a, float b, float c, float d, float e, float f, float g, float h, float i, NiMatrix3& mat) {
	mat.entry[0].pt[0] = a;
	mat.entry[0].pt[1] = b;
	mat.entry[0].pt[2] = c;
	mat.entry[1].pt[0] = d;
	mat.entry[1].pt[1] = e;
	mat.entry[1].pt[2] = f;
	mat.entry[2].pt[0] = g;
	mat.entry[2].pt[1] = h;
	mat.entry[2].pt[2] = i;
}

NiMatrix3 GetRotationMatrix33(float pitch, float yaw, float roll) {
	NiMatrix3 m_yaw;
	SetMatrix33(cos(yaw), -sin(yaw), 0,
				sin(yaw), cos(yaw), 0,
				0, 0, 1,
				m_yaw);
	NiMatrix3 m_roll;
	SetMatrix33(1, 0, 0,
				0, cos(roll), -sin(roll),
				0, sin(roll), cos(roll),
				m_roll);
	NiMatrix3 m_pitch;
	SetMatrix33(cos(pitch), 0, sin(pitch),
				0, 1, 0,
				-sin(pitch), 0, cos(pitch),
				m_pitch);
	return m_yaw * m_pitch * m_roll;
}

NiMatrix3 GetRotationMatrix33(NiPoint3 axis, float angle) {
	float x = axis.x * sin(angle / 2.0f);
	float y = axis.y * sin(angle / 2.0f);
	float z = axis.z * sin(angle / 2.0f);
	float w = cos(angle / 2.0f);
	Quaternion q = Quaternion(x, y, z, w);
	return q.ToRotationMatrix33();
}

//From https://android.googlesource.com/platform/external/jmonkeyengine/+/59b2e6871c65f58fdad78cd7229c292f6a177578/engine/src/core/com/jme3/math/Quaternion.java
NiMatrix3 Quaternion::ToRotationMatrix33() {
	float norm = Norm();
	// we explicitly test norm against one here, saving a division
	// at the cost of a test and branch.  Is it worth it?
	float s = (norm == 1.0f) ? 2.0f : (norm > 0.0f) ? 2.0f / norm : 0;

	// compute xs/ys/zs first to save 6 multiplications, since xs/ys/zs
	// will be used 2-4 times each.
	float xs = x * s;
	float ys = y * s;
	float zs = z * s;
	float xx = x * xs;
	float xy = x * ys;
	float xz = x * zs;
	float xw = w * xs;
	float yy = y * ys;
	float yz = y * zs;
	float yw = w * ys;
	float zz = z * zs;
	float zw = w * zs;

	// using s=2/norm (instead of 1/norm) saves 9 multiplications by 2 here
	NiMatrix3 mat;
	SetMatrix33(1 - (yy + zz), (xy - zw), (xz + yw),
					   (xy + zw), 1 - (xx + zz), (yz - xw),
					   (xz - yw), (yz + xw), 1 - (xx + yy),
					   mat);

	return mat;
}

//Sarrus rule
float Determinant(NiMatrix3 mat) {
	return mat.entry[0].pt[0] * mat.entry[1].pt[1] * mat.entry[2].pt[2]
		+ mat.entry[0].pt[1] * mat.entry[1].pt[2] * mat.entry[2].pt[0]
		+ mat.entry[0].pt[2] * mat.entry[1].pt[0] * mat.entry[2].pt[1]
		- mat.entry[0].pt[2] * mat.entry[1].pt[1] * mat.entry[2].pt[0]
		- mat.entry[0].pt[1] * mat.entry[1].pt[0] * mat.entry[2].pt[2]
		- mat.entry[0].pt[0] * mat.entry[1].pt[2] * mat.entry[2].pt[1];
}

NiMatrix3 Inverse(NiMatrix3 mat) {
	float det = Determinant(mat);
	if (det == 0) {
		NiMatrix3 idmat;
		idmat.MakeIdentity();
		return idmat;
	}
	float a = mat.entry[0].pt[0];
	float b = mat.entry[0].pt[1];
	float c = mat.entry[0].pt[2];
	float d = mat.entry[1].pt[0];
	float e = mat.entry[1].pt[1];
	float f = mat.entry[1].pt[2];
	float g = mat.entry[2].pt[0];
	float h = mat.entry[2].pt[1];
	float i = mat.entry[2].pt[2];
	NiMatrix3 invmat;
	SetMatrix33(e * i - f * h, -(b * i - c * h), b * f - c * e,
				-(d * i - f * g), a * i - c * g, -(a * f - c * d),
				d * h - e * g, -(a * h - b * g), a * e - b * d,
				invmat);
	return invmat * (1.0f / det);
}

//(Rotation Matrix)^-1 * (World pos - Local Origin)
NiPoint3 WorldToLocal(NiPoint3 wpos, NiPoint3 lorigin, NiMatrix3 rot) {
	NiPoint3 lpos = wpos - lorigin;
	NiMatrix3 invrot = Inverse(rot);
	return invrot * lpos;
}

NiPoint3 LocalToWorld(NiPoint3 lpos, NiPoint3 lorigin, NiMatrix3 rot) {
	return rot * lpos + lorigin;
}

NiPoint3 CrossProduct(NiPoint3 a, NiPoint3 b) {
	NiPoint3 ret;
	ret[0] = a[1] * b[2] - a[2] * b[1];
	ret[1] = a[2] * b[0] - a[0] * b[2];
	ret[2] = a[0] * b[1] - a[1] * b[0];
	return ret;
}

float Length(NiPoint3 p) {
	return sqrt(p.x * p.x + p.y * p.y + p.z * p.z);
}

void Normalize(NiPoint3& p) {
	float l = Length(p);
	if (l == 0) {
		p.x = 0;
		p.y = 0;
		p.z = 0;
	}
	p.x /= l;
	p.y /= l;
	p.z /= l;
}

class RE::NiCloningProcess {
public:
	BSTHashMap<void*, void*>	unk00;	// 00
	void* unk30;	// 30
	uint64_t		unk38;		// 38
	uint64_t		unk40;		// 40
	void* unk48;	// 48 - DEADBEEF
	void* unk50;	// 50 - bucket
	uint64_t		unk58;		// 58
	uint32_t		unk60;		// 60 - copytype? 0, 1, 2
	uint32_t		unk64;		// 64
};

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

#pragma endregion


#pragma region Variables

class CharacterMoveFinishEventWatcher;

REL::Relocation<float*> ptr_engineTime{ REL::ID(599343) };
CSimpleIniA ini(true, true, false);
PlayerCharacter* p;
PlayerCamera* pcam;
PlayerControls* pc;
CharacterMoveFinishEventWatcher* PCWatcher;
ActorValueInfo* rightAttackCondition;
ActorValueInfo* leftAttackCondition;
float rotX, rotY;
float rotLimitX = 10.0f;
float rotLimitY = 5.0f;
float rotDivX = 10.0f;
float rotDivY = 20.0f;
float rotReturnDiv = 1.5f;
float rotReturnStep = 0.1f;
float lastRun;
float toRad = (float)(MATH_PI / 180.0);

#pragma endregion

#pragma region Events

class SwayHandler : public PlayerInputHandler {
public:
	explicit constexpr SwayHandler(PlayerControlsData& a_data) noexcept :
		PlayerInputHandler(a_data) {
	};

	void CalculateXY(float x, float y) {
		rotX = max(min(rotX + x / rotDivX, rotLimitX), -rotLimitX);
		rotY = max(min(rotY + y / rotDivY, rotLimitY), -rotLimitY);
	}

	virtual void OnMouseMoveEvent(const MouseMoveEvent* evn) {
		CalculateXY((float)evn->mouseInputX, (float)evn->mouseInputY);
	}

	virtual void OnThumbstickEvent(const ThumbstickEvent* evn) {
		CalculateXY(evn->xValue, evn->yValue);
	}
};

class CharacterMoveFinishEventWatcher {
public:
	typedef BSEventNotifyControl (CharacterMoveFinishEventWatcher::* FnProcessEvent)(bhkCharacterMoveFinishEvent& evn, BSTEventSource<bhkCharacterMoveFinishEvent>* dispatcher);
	BSEventNotifyControl HookedProcessEvent(bhkCharacterMoveFinishEvent& evn, BSTEventSource<bhkCharacterMoveFinishEvent>* src) {
		Actor* a = (Actor*)((uintptr_t)this - 0x150);
		if (a->Get3D()) {
			NiAVObject* node = a->Get3D();

			float conditionalMultiplier = 1.0f;
			if (a->GetActorValue(*leftAttackCondition) == 0) {
				conditionalMultiplier -= 0.3f;
			}
			if (a->GetActorValue(*rightAttackCondition) == 0) {
				conditionalMultiplier -= 0.3f;
			}
			if (a->gunState == 0x8 || a->gunState == 0x6) {
				conditionalMultiplier *= 3.0f;
			}
			float deltaTime = (*ptr_engineTime - lastRun) / 0.01667f;
			float step = rotReturnStep * conditionalMultiplier * deltaTime;
			rotX /= max(pow(rotReturnDiv * conditionalMultiplier, 1.0f / deltaTime), 1);
			rotY /= max(pow(rotReturnDiv * conditionalMultiplier, 1.0f / deltaTime), 1);
			if (abs(rotX) * (abs(rotX) - step) <= 0) {
				rotX = 0;
			}
			if (abs(rotY) * (abs(rotY) - step) <= 0) {
				rotY = 0;
			}

			NiNode* chestInserted = (NiNode*)node->GetObjectByName("ChestInserted");
			if (!chestInserted) {
				NiNode* chest = (NiNode*)node->GetObjectByName("Chest");
				if (chest) {
					NiNode* parent = chest->parent;
					NiCloningProcess cp = NiCloningProcess();
					cp.unk60 = 2;
					chestInserted = parent->CreateClone(cp)->IsNode();
					chestInserted->local.translate = NiPoint3();
					for (int i = 0; i < chestInserted->children.size(); ++i) {
						NiPointer<NiAVObject> detached;
						chestInserted->DetachChildAt(0, detached);
						if (detached.get()) {
							detached->DecRefCount();
						}
					}
					chestInserted->collisionObject = nullptr;
					chestInserted->local.rotate.MakeIdentity();
					chestInserted->world.rotate.MakeIdentity();
					parent->AttachChild(chestInserted, true);
					parent->DetachChild(chest);
					chestInserted->AttachChild(chest, true);
					chest->PostAttachUpdate();
					chestInserted->name = "ChestInserted";
				}
			}
			else {
				NiPoint3 pos, dir;
				a->GetEyeVector(pos, dir, true);
				NiPoint3 up = LocalToWorld(NiPoint3(0, 0, 1), NiPoint3(), chestInserted->world.rotate);
				NiMatrix3 rot = GetRotationMatrix33(up, -rotX * toRad);
				NiPoint3 right = LocalToWorld(NiPoint3(1, 0, 0), NiPoint3(), chestInserted->world.rotate);
				rot = rot * GetRotationMatrix33(right, rotY * toRad);
				chestInserted->local.rotate = rot;
			}

			if (abs(rotX) * (abs(rotX) - step) > 0) {
				if (rotX > 0) {
					rotX -= step;
				}
				else {
					rotX += step;
				}
			}
			if (abs(rotY) * (abs(rotY) - step) > 0) {
				if (rotY > 0) {
					rotY -= step;
				}
				else {
					rotY += step;
				}
			}

			lastRun = *ptr_engineTime;
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

#pragma endregion

#pragma region Initializers

void InitializePlugin() {
	p = PlayerCharacter::GetSingleton();
	pcam = PlayerCamera::GetSingleton();
	_MESSAGE("PlayerCharacter %llx PlayerCamera %llx", p, pcam);
	pc = PlayerControls::GetSingleton();
	SwayHandler* sh = new SwayHandler(pc->data);
	pc->RegisterHandler((PlayerInputHandler*)sh);
	uint64_t PCVtable = REL::Relocation<uint64_t>{ PlayerCharacter::VTABLE[13] }.address();
	PCWatcher->HookSink(PCVtable);
	leftAttackCondition = (ActorValueInfo*)TESForm::GetFormByID(0x00036E);
	rightAttackCondition = (ActorValueInfo*)TESForm::GetFormByID(0x00036F);
	_MESSAGE("Hook to event");
}

void LoadConfigs() {
}

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
