#include <SimpleIni.h>
#include <unordered_map>
#include <Windows.h>
#include <vector>
#include "MathUtils.h"
#include "Havok.h"
using namespace F4SE;
#define MATH_PI 3.14159265358979323846
#define HAVOKTOFO4 69.99124f

using std::vector;
/*using RE::NiAVObject;
using RE::NiNode;
using RE::NiPointer;
using RE::NiPoint3;
using RE::NiMatrix3;
using RE::Actor;
using RE::PlayerCharacter;
using RE::PlayerCamera;
using RE::PlayerControls;
using RE::ActorValueInfo;
using RE::bhkCharacterController;
using RE::PlayerInputHandler;
using RE::PlayerControlsData;
using RE::MouseMoveEvent;
using RE::ThumbstickEvent;
using RE::UI;
using RE::CameraState;
using RE::bhkCharacterMoveFinishEvent;
using RE::BSTEventSource;
using RE::INTERACTING_STATE;
using RE::BSEventNotifyControl;
using RE::BSInputEventReceiver;
using RE::ButtonEvent;
using RE::InputEvent;
using RE::INPUT_DEVICE;
using RE::INPUT_EVENT_TYPE;
using RE::BSTEventSink;
using RE::TESForm;
using RE::GameSettingCollection;*/
using namespace RE;

#pragma region Variables

class CharacterMoveFinishEventWatcher;

REL::Relocation<float*> ptr_engineTime{ REL::ID(599343) };
REL::Relocation<uintptr_t> ptr_ADS_DistanceCheck{ REL::ID(1278889), 0x31 };
//REL::Relocation<hknpTriangleShape> ptr_havokRefTriangle{ REL::ID(801922) };
REL::Relocation<hknpShapeTagCodec*> ptr_shapeTagCodec{ REL::ID(322467) };
REL::Relocation<hknpCollisionFilter*> ptr_collisionFilter{ REL::ID(246130) };
REL::Relocation<bhkWorld**> ptr_bhkWorldM{ REL::ID(128691) };
CachedRaycastData* rayInterface;
vector<hkVector4f> cachedShape0;
vector<hkVector4f> cachedShape1;
CSimpleIniA ini(true, true, false);
PlayerCharacter* p;
PlayerCamera* pcam;
PlayerControls* pc;
NiNode* lastRoot;
BSBound* bbx;
CharacterMoveFinishEventWatcher* PCWatcher;
ActorValueInfo* rightAttackCondition;
ActorValueInfo* leftAttackCondition;
float rotX, rotY, rotZ, transZ;
float rotLimitX = 10.0f;
float rotLimitY = 5.0f;
float rotDivX = 10.0f;
float rotDivY = 20.0f;
float rotDivXPad = 4.0f;
float rotDivYPad = 8.0f;
float rotADSConditionMult = 5.0f;
float rotReturnDiv = 1.5f;
float rotReturnStep = 0.1f;
float rotReturnDivMin = 1.02f;
bool rotDisableInADS = false;
float lastRun;
float toRad = (float)(MATH_PI / 180.0);
int leanState = 0;
float leanWeight;
float leanTime;
float leanTimeCost = 1.0f;
float leanMax = 15.0f;
float leanMax3rd = 30.0f;
float leanCollisionThreshold = 0.08f;
bool toggleLean = false;
uint32_t leanLeft = 0x51;
uint32_t leanRight = 0x45;
bool leanADSOnly = false;
bool leanR6Style = false;
bool collisionDevMode = false;
bool buttonDevMode = false;
float lastHeight = 120.0f;
float heightDiffThreshold = 5.0f;
float heightBuffer = 16.0f;
bool dynamicHeight = false;

#pragma endregion

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
	char* ptr = static_cast<char*>(mem);
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

void NiSetParent(NiAVObject* node, NiNode* parent) {
	NiNode* oldParent = node->parent;
	if (oldParent) {
		oldParent->DetachChild(node);
	}
	node->parent = parent;
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

bool IsInPowerArmor(Actor* a) {
	if (!a->extraList) {
		return false;
	}
	return a->extraList->HasType(RE::EXTRA_DATA_TYPE::kPowerArmor);;
}

bool IsInADS(Actor* a) {
	return (a->gunState == 0x8 || a->gunState == 0x6);
}

bool IsFirstPerson() {
	return pcam->currentState == pcam->cameraStates[CameraState::kFirstPerson];
}

float GetActorScale(Actor* a) {
	typedef float (*_GetPlayerScale)(Actor*);
	REL::Relocation<_GetPlayerScale> func{ REL::ID(911188) };
	return func(a);
}

float Sign(float f) {
	if (f == 0)
		return 0;
	return abs(f) / f;
}

#pragma endregion

#pragma region Functions

void PreparePlayerSkeleton(NiNode* node) {
	NiAVObject* fpNode = p->Get3D(true);
	if (node == fpNode) {
		_MESSAGE("First person skeleton found.");
		NiNode* chestInserted = (NiNode*)fpNode->GetObjectByName("ChestInserted1st");
		NiNode* chest = (NiNode*)fpNode->GetObjectByName("Chest");
		if (!chestInserted) {
			if (chest) {
				chestInserted = InsertBone(chest, "ChestInserted1st");
			}
		}
		else {
			CheckHierarchy(chest, chestInserted);
		}

		NiNode* comInserted1st = (NiNode*)fpNode->GetObjectByName("COMInserted1st");
		NiNode* com = (NiNode*)fpNode->GetObjectByName("COM");
		if (!comInserted1st) {
			if (com) {
				comInserted1st = InsertBone(com, "COMInserted1st");
			}
		}
		else {
			CheckHierarchy(com, comInserted1st);
		}

		NiNode* cameraInserted1st = (NiNode*)fpNode->GetObjectByName("CameraInserted1st");
		NiNode* camera = (NiNode*)fpNode->GetObjectByName("Camera");
		if (!cameraInserted1st) {
			if (camera) {
				cameraInserted1st = InsertBone(camera, "CameraInserted1st");
			}
		}
		else {
			CheckHierarchy(camera, cameraInserted1st);
		}
	}
	NiAVObject* tpNode = p->Get3D(false);
	if (node == tpNode) {
		_MESSAGE("Third person skeleton found.");
		NiNode* comInserted = (NiNode*)tpNode->GetObjectByName("COMInserted");
		NiNode* com = (NiNode*)tpNode->GetObjectByName("COM");
		if (!comInserted) {
			if (com) {
				comInserted = InsertBone(com, "COMInserted");
			}
		}
		else {
			CheckHierarchy(com, comInserted);
		}

		NiNode* cameraInserted = (NiNode*)tpNode->GetObjectByName("CameraInserted");
		NiNode* camera = (NiNode*)tpNode->GetObjectByName("Camera");
		if (!cameraInserted) {
			if (camera) {
				cameraInserted = InsertBone(camera, "CameraInserted");
			}
		}
		else {
			CheckHierarchy(camera, cameraInserted);
		}

		NiNode* chestInserted = (NiNode*)tpNode->GetObjectByName("ChestInserted");
		NiNode* chest = (NiNode*)tpNode->GetObjectByName("Chest");
		if (!chestInserted) {
			if (chest) {
				chestInserted = InsertBone(chest, "ChestInserted");
			}
		}
		else {
			CheckHierarchy(chest, chestInserted);
		}

		NiNode* spine2Inserted = (NiNode*)tpNode->GetObjectByName("Spine2Inserted");
		NiNode* spine2 = (NiNode*)tpNode->GetObjectByName("SPINE2");
		if (!spine2Inserted) {
			if (spine2) {
				spine2Inserted = InsertBone(spine2, "Spine2Inserted");
			}
		}
		else {
			CheckHierarchy(spine2, spine2Inserted);
		}

		NiNode* spine1Inserted = (NiNode*)tpNode->GetObjectByName("Spine1Inserted");
		NiNode* spine1 = (NiNode*)tpNode->GetObjectByName("SPINE1");
		if (!spine1Inserted) {
			if (spine1) {
				spine1Inserted = InsertBone(spine1, "Spine1Inserted");
			}
		}
		else {
			CheckHierarchy(spine1, spine1Inserted);
		}

		NiNode* pelvisInserted = (NiNode*)tpNode->GetObjectByName("PelvisInserted");
		NiNode* pelvis = (NiNode*)tpNode->GetObjectByName("Pelvis");
		if (!pelvisInserted) {
			if (pelvis) {
				pelvisInserted = InsertBone(pelvis, "PelvisInserted");
			}
		}
		else {
			CheckHierarchy(pelvis, pelvisInserted);
		}

		NiNode* llegCalfInserted = (NiNode*)tpNode->GetObjectByName("LLeg_CalfInserted");
		NiNode* llegCalf = (NiNode*)tpNode->GetObjectByName("LLeg_Calf");
		if (!llegCalfInserted) {
			if (llegCalf) {
				llegCalfInserted = InsertBone(llegCalf, "LLeg_CalfInserted");
			}
		}
		else {
			CheckHierarchy(llegCalf, llegCalfInserted);
		}

		NiNode* rlegCalfInserted = (NiNode*)tpNode->GetObjectByName("RLeg_CalfInserted");
		NiNode* rlegCalf = (NiNode*)tpNode->GetObjectByName("RLeg_Calf");
		if (!rlegCalfInserted) {
			if (rlegCalf) {
				rlegCalfInserted = InsertBone(rlegCalf, "RLeg_CalfInserted");
			}
		}
		else {
			CheckHierarchy(rlegCalf, rlegCalfInserted);
		}
	}
}

void ResizeCollisionShapeX(CompoundShapeData* data, vector<hkVector4f>& cachedShape, float ratio, float bumperHalf) {
	uintptr_t polytopeShape = (uintptr_t)data->shape;
	uint16_t vertexSize = *(uint16_t*)(polytopeShape + 0x30);
	uint16_t vertexOffset = *(uint16_t*)(polytopeShape + 0x32);
	*(float*)((uintptr_t)data + 0x80) = -bumperHalf;
	*(float*)((uintptr_t)data + 0x90) = bumperHalf;
	if (cachedShape.size() == 0) {
		for (int i = 0; i < vertexSize; ++i) {
			cachedShape.push_back(*(hkVector4f*)(polytopeShape + 0x30 + vertexOffset + 0x10 * i));
		}
	}
	for (int i = 0; i < cachedShape.size(); ++i) {
		((hkVector4f*)(polytopeShape + 0x30 + vertexOffset + 0x10 * i))->x = cachedShape[i].x * ratio + bumperHalf;
	}
}

/*void castRay_Internal(hknpRayCastQuery& query, hknpShape* targetShape, hkTransform& targetShapeTransform, hknpCollisionQueryCollector* collector) {

	//hknpInplaceTriangleShape targetTriangle(0.0f);
	query.m_filter.storage = ptr_collisionFilter.get();
	hknpCollisionQueryContext queryContext(nullptr, nullptr);
	queryContext.m_shapeTagCodec = ptr_shapeTagCodec.get();
	hknpQueryFilterData targetShapeFilterData;

	hknpShapeQueryInfo targetShapeInfo;
	targetShapeInfo.m_shapeToWorld.storage = &targetShapeTransform;

	float frac = query.getFraction();
	__m128 _frac = _mm_load_ss(&frac);
	if (_mm_ucomigt_ss(collector->earlyOutThreshold.real, _frac)) {
		collector->earlyOutThreshold.real = _frac;
	}

	targetShape->CastRayImpl(&queryContext, query, targetShapeFilterData, targetShapeInfo, collector);
}

bool CastRay(Actor* a, hkVector4f origin, hkVector4f dir, float dist) {
	if (!a->currentProcess || !a->currentProcess->middleHigh)
		return false;
	NiPointer<bhkCharacterController> con = a->currentProcess->middleHigh->charController;
	if (!con.get())
		return false;
	hknpShape* targetShape = (hknpShape*)con->shapes[0]._ptr;
	hkTransform targetShapeTransform;
	targetShapeTransform.setIdentity();
	hknpRayCastQuery query{ origin, dir, dist };
	hknpAllHitsCollector collector;
	castRay_Internal(query,	targetShape, targetShapeTransform, &collector);
	_MESSAGE("NumHits %d", collector.hits._size);
	return true;
}

hkVector4f CastRayInternal(hkVector4f from, hkVector4f to, float& hitFractionOut, hkVector4f& hitNormalOut) {
	rayInterface->world = *ptr_bhkWorldM;
	_MESSAGE("from %f %f %f %f to %f %f %f %f", from.x, from.y, from.z, from.w, to.x, to.y, to.z, to.w);
	unkParam arg = unkParam();
	hkVector4f ret = rayInterface->castRay(arg, from, to, hitFractionOut, hitNormalOut);
	//_MESSAGE("hitFraction %f hitNormal %f %f %f %f", hitFractionOut, hitNormalOut.x, hitNormalOut.y, hitNormalOut.z, hitNormalOut.w);
	_MESSAGE("ret %llx", &ret);
	Dump(&ret, 0x20);
	//delete(rayInterface);
	return from;
}

hkVector4f CastRay(hkVector4f from, hkVector4f to, float& hitFractionOut, hkVector4f& hitNormalOut) {
	return CastRayInternal(from, to, hitFractionOut, hitNormalOut);
}

hkVector4f CastRay(hkVector4f from, hkVector4f dir, float dist, float& hitFractionOut, hkVector4f& hitNormalOut) {
	return CastRayInternal(from, from + dir * dist, hitFractionOut, hitNormalOut);
}*/

#pragma endregion

#pragma region Events
class SwayHandler : public PlayerInputHandler {
public:
	explicit constexpr SwayHandler(PlayerControlsData& a_data) noexcept :
		PlayerInputHandler(a_data) {
	};

	void CalculateXY(float x, float y, float divX, float divY) {
		if (UI::GetSingleton()->menuMode || UI::GetSingleton()->GetMenuOpen("CursorMenu"))
			return;
		rotX = max(min(rotX + x / divX, rotLimitX), -rotLimitX);
		rotY = max(min(rotY + y / divY, rotLimitY), -rotLimitY);
	}

	virtual void OnMouseMoveEvent(const MouseMoveEvent* evn) {
		CalculateXY((float)evn->mouseInputX, (float)evn->mouseInputY, rotDivX, rotDivY);
	}

	virtual void OnThumbstickEvent(const ThumbstickEvent* evn) {
		if (evn->idCode == ThumbstickEvent::kRight) {
			CalculateXY(evn->xValue, -evn->yValue, rotDivXPad, rotDivYPad);
		}
	}
};

class LeanLookHandler {
public:
	typedef void (LeanLookHandler::* FnOnMouseMoveEvent)(MouseMoveEvent* evn);
	typedef void (LeanLookHandler::* FnOnThumbstickEvent)(ThumbstickEvent* evn);

	std::vector<float> RotateXY(float x, float y, float rad) {
		std::vector<float> ret{ x, y };
		float _x = x;
		float _y = y;
		ret[0] = cos(rad) * _x - sin(rad) * _y;
		ret[1] = sin(rad) * _x + cos(rad) * _y;
		return ret;
	}

	void HookedMouseMoveEvent(MouseMoveEvent* evn) {
		if (pcam->currentState == pcam->cameraStates[CameraState::kFirstPerson] && !leanR6Style) {
			std::vector<float> rotated = RotateXY((float)evn->mouseInputX, (float)evn->mouseInputY, -rotZ * toRad);
			evn->mouseInputX = (int32_t)rotated[0];
			evn->mouseInputY = (int32_t)rotated[1];
		}
		(this->*mouseEvn)(evn);
	}

	void HookedThumbstickEvent(ThumbstickEvent* evn) {
		if (evn->idCode == ThumbstickEvent::kRight && pcam->currentState == pcam->cameraStates[CameraState::kFirstPerson] && !leanR6Style) {
			std::vector<float> rotated = RotateXY(evn->xValue, -evn->yValue, -rotZ * toRad);
			evn->xValue = rotated[0];
			evn->yValue = -rotated[1];
		}
		(this->*thumbstickEvn)(evn);
	}

	void HookEvents() {
		uintptr_t vtable = *(uintptr_t*)this;
		mouseEvn = SafeWrite64Function(vtable + 0x30, &LeanLookHandler::HookedMouseMoveEvent);
		thumbstickEvn = SafeWrite64Function(vtable + 0x20, &LeanLookHandler::HookedThumbstickEvent);
	}

protected:
	static FnOnMouseMoveEvent mouseEvn;
	static FnOnThumbstickEvent thumbstickEvn;
};
LeanLookHandler::FnOnMouseMoveEvent LeanLookHandler::mouseEvn;
LeanLookHandler::FnOnThumbstickEvent LeanLookHandler::thumbstickEvn;

class CharacterMoveFinishEventWatcher {
public:
	typedef BSEventNotifyControl (CharacterMoveFinishEventWatcher::* FnProcessEvent)(bhkCharacterMoveFinishEvent& evn, BSTEventSource<bhkCharacterMoveFinishEvent>* dispatcher);
	BSEventNotifyControl HookedProcessEvent(bhkCharacterMoveFinishEvent& evn, BSTEventSource<bhkCharacterMoveFinishEvent>* src) {
		Actor* a = (Actor*)((uintptr_t)this - 0x150);
		if (a->Get3D()) {
			NiAVObject* node = a->Get3D();
			NiAVObject* tpNode = a->Get3D(false);
			if (node != lastRoot) {
				bbx = (BSBound*)tpNode->GetExtraData("BBX");
				lastRoot = node->IsNode();
				PreparePlayerSkeleton(lastRoot);
			}
			bool isFP = IsFirstPerson();
			float conditionalMultiplier = 1.0f;
			if (a->GetActorValue(*leftAttackCondition) == 0) {
				conditionalMultiplier -= 0.2f;
			}
			if (a->GetActorValue(*rightAttackCondition) == 0) {
				conditionalMultiplier -= 0.2f;
			}
			if (IsInADS(a)) {
				conditionalMultiplier *= rotADSConditionMult;
				if (rotDisableInADS) {
					rotX = 0;
					rotY = 0;
				}
			}
			float deltaTime = min(*ptr_engineTime - lastRun, 5.0f);
			float timeMult = deltaTime * 60.0f;

			if (leanADSOnly && !IsInADS(a)) {
				leanState = 0;
			}

			if (leanState != 0) {
				UI* ui = UI::GetSingleton();
				if (ui->GetMenuOpen("WorkshopMenu")
					|| ui->GetMenuOpen("DialogueMenu")
					|| a->interactingState != INTERACTING_STATE::kNotInteracting) {
					leanState = 0;
				}
			}

			if (leanState == 1) {
				leanTime = min(leanTime + deltaTime, leanTimeCost);
			}
			else if (leanState == -1){
				leanTime = max(leanTime - deltaTime, -leanTimeCost);
			}
			else {
				if (abs(leanTime) <= deltaTime) {
					leanTime = 0;
				}
				else {
					leanTime = max(min(leanTime - Sign(leanTime) * deltaTime, leanTimeCost), -leanTimeCost);
				}
			}
			float deltaLeanWeight = leanTime / leanTimeCost - leanWeight;
			leanWeight += deltaLeanWeight;

			float pcScale = GetActorScale(p);
			float translateDist = isFP ? 122.0f * sin(leanMax * toRad) * pcScale : 61.0f * sin(leanMax3rd * toRad) * pcScale;
			NiPoint3 pos, dir;
			a->GetEyeVector(pos, dir, true);
			NiPoint3 heading = Normalize(NiPoint3(dir.x, dir.y, 0));

			bhkCharacterController* con = nullptr;
			if (a->currentProcess) {
				con = a->currentProcess->middleHigh->charController.get();
			}
			if (con) {
				rotZ = isFP ? leanMax * leanWeight : leanMax3rd * leanWeight;
				if (deltaLeanWeight != 0) {
					float ratio = (1 - cos(rotZ * toRad)) * 4.0f;
					float signRotZ = Sign(rotZ);
					uintptr_t charProxy = *(uintptr_t*)((uintptr_t)con + 0x470);
					hknpShape* bumper = con->shapes[0]._ptr;
					CompoundShapeData* data = *(CompoundShapeData**)((uintptr_t)bumper + 0x60);
					if (data && data->shape) {
						float bumperWidthHalf = 0.257f * ratio * signRotZ;
						ResizeCollisionShapeX(data, cachedShape0, ratio, bumperWidthHalf);
					}
					bumper = con->shapes[1]._ptr;
					data = *(CompoundShapeData**)((uintptr_t)bumper + 0x60);
					if (data && data->shape) {
						float bumperWidthHalf = 0.307f * ratio * signRotZ;
						ResizeCollisionShapeX(data, cachedShape1, ratio, bumperWidthHalf);
					}
					if (charProxy) {
						NiPoint3 right = CrossProduct(con->forwardVec, con->up);
						hkTransform* charProxyTransform = (hkTransform*)(charProxy + 0x40);
						hkVector4f& charProxyVel = *(hkVector4f*)(charProxy + 0xA0);
						hkVector4f& charProxyLastDisplacement = *(hkVector4f*)(charProxy + 0xB0);
						//_MESSAGE("Length %f", (charProxyLastDisplacement - charProxyVel * con->stepInfo.deltaTime.storage).Length());
						float diff = DotProduct(charProxyVel - con->outVelocity, right * -signRotZ);
						if (collisionDevMode) {
							_MESSAGE("Length %f", diff);
						}
						if (diff > leanCollisionThreshold) {
							if (collisionDevMode) {
								_MESSAGE("Collision");
							}
							hkVector4f displacement = right * translateDist * deltaLeanWeight / HAVOKTOFO4;
							charProxyTransform->m_translation = charProxyTransform->m_translation - displacement;
						}
						else {
							hkVector4f displacement = right * translateDist * deltaLeanWeight / HAVOKTOFO4;
							//float& keepDistance = *(float*)(charProxy + 0xE0);
							//*charProxyVel = *charProxyVel + right * translateDist * deltaLeanWeight / HAVOKTOFO4;
							charProxyTransform->m_translation = charProxyTransform->m_translation + displacement;
						}
					}
				}
				if (dynamicHeight) {
					if (tpNode) {
						NiNode* head = (NiNode*)tpNode->GetObjectByName("HEAD");
						if (!head)
							head = (NiNode*)tpNode->GetObjectByName("Head");
						if (head) {
							float height = head->world.translate.z + heightBuffer - a->data.location.z;
							//_MESSAGE("Current height %f", height);
							if (bbx) {
								float optimalHeight = bbx->extents.z * 2.0f;
								//_MESSAGE("Optimal height %f", optimalHeight);
								if (abs(height - lastHeight) > heightDiffThreshold) {
									float ratio = height / optimalHeight;
									hknpShape* bumper = con->shapes[0]._ptr;
									CompoundShapeData* data = *(CompoundShapeData**)((uintptr_t)bumper + 0x60);
									if (data) {
										data->translate.z = ratio - 1.0f;
										data->scale.z = ratio;
									}
									bumper = con->shapes[1]._ptr;
									data = *(CompoundShapeData**)((uintptr_t)bumper + 0x60);
									if (data) {
										data->translate.z = ratio - 1.0f;
										data->scale.z = ratio;
									}
									//_MESSAGE("Changed height");
									lastHeight = height;
								}
							}
						}
					}
				}
			}

			if (tpNode) {
				rotZ = leanMax3rd * leanWeight;
				transZ = leanMax3rd * leanWeight / 3.0f;
				float rotXRadByThree = rotX * toRad / 3.0f;
				float rotYRadByTwo = rotY * toRad / 2.0f;
				float rotZRadByThree = rotZ * toRad / 3.0f;
				float transZRad = transZ * toRad;
				float transZByTwo = transZ / 2.0f;

				/*hkVector4f rayOrigin = hkVector4f(tpNode->world.translate.x, tpNode->world.translate.y, tpNode->world.translate.z + 60.0f * scale);
				hkVector4f rayDir = right;

				float rayDist = 70.0f * abs(sin(leanMax3rd * toRad));
				float hitFraction;
				hkVector4f hitNormal;
				if (*ptr_bhkWorldM) {
					//hkVector4f rayHit = CastRay(*ptr_bhkWorldM, rayOrigin, rayDir, rayDist, hitFraction, hitNormal);
					hkVector4f rayHit = CastRay(rayOrigin, hkVector4f(0, 0, -1), 200, hitFraction, hitNormal);
				}*/

				NiNode* chestInserted = (NiNode*)tpNode->GetObjectByName("ChestInserted");
				if (chestInserted) {
					NiMatrix3 rot = chestInserted->parent->world.rotate * GetRotationMatrix33(heading, rotZRadByThree) * Inverse(chestInserted->parent->world.rotate);
					chestInserted->local.rotate = rot;
				}

				NiNode* spine2Inserted = (NiNode*)tpNode->GetObjectByName("Spine2Inserted");
				if (spine2Inserted) {
					spine2Inserted->local.rotate = spine2Inserted->parent->world.rotate * GetRotationMatrix33(heading, rotZRadByThree) * Inverse(spine2Inserted->parent->world.rotate);
					//_MESSAGE("spine1Inserted");
				}

				NiNode* spine1Inserted = (NiNode*)tpNode->GetObjectByName("Spine1Inserted");
				if (spine1Inserted) {
					spine1Inserted->local.rotate = spine1Inserted->parent->world.rotate * GetRotationMatrix33(heading, rotZRadByThree) * Inverse(spine1Inserted->parent->world.rotate);
					spine1Inserted->local.translate.z = transZ;
					//_MESSAGE("spine1Inserted");
				}

				NiNode* pelvisInserted = (NiNode*)tpNode->GetObjectByName("PelvisInserted");
				if (pelvisInserted) {
					NiMatrix3 rot = GetRotationMatrix33(rotZRadByThree, 0, 0);
					pelvisInserted->local.rotate = rot;
					pelvisInserted->local.translate.z = transZ;
					//_MESSAGE("pelvisInserted");
				}

				NiNode* llegCalfInserted = (NiNode*)tpNode->GetObjectByName("LLeg_CalfInserted");
				if (llegCalfInserted) {
					NiMatrix3 rot = GetRotationMatrix33(-rotZRadByThree, 0, 0);
					llegCalfInserted->local.rotate = rot;
					llegCalfInserted->local.translate.z = transZByTwo;
					//_MESSAGE("llegFootInserted");
				}

				NiNode* rlegCalfInserted = (NiNode*)tpNode->GetObjectByName("RLeg_CalfInserted");
				if (rlegCalfInserted) {
					NiMatrix3 rot = GetRotationMatrix33(-rotZRadByThree, 0, 0);
					rlegCalfInserted->local.rotate = rot;
					rlegCalfInserted->local.translate.z = transZByTwo;
					//_MESSAGE("rlegFootInserted");
				}

				NiNode* comInserted = (NiNode*)tpNode->GetObjectByName("COMInserted");
				if (comInserted) {
					comInserted->local.translate = NiPoint3(translateDist * leanWeight, 0, 0);
				}

				NiNode* cameraInserted = (NiNode*)tpNode->GetObjectByName("CameraInserted");
				if (cameraInserted) {
					cameraInserted->local.translate = NiPoint3(translateDist * leanWeight, 0, 0);
					//_MESSAGE("cameraInserted");
				}
			}

			if (isFP) {
				//float rayDist = 70.0f * abs(sin(leanMax * toRad));
				//CastRay(a, rayOrigin, rayDir, rayDist);
				rotZ = leanMax * leanWeight;
				transZ = leanMax * leanWeight / 3.0f;
				NiNode* chestInserted1st = (NiNode*)node->GetObjectByName("ChestInserted1st");
				if (chestInserted1st) {
					NiMatrix3 rot = GetRotationMatrix33(0, rotY * toRad, -rotX * toRad);
					chestInserted1st->local.rotate = rot;
					//_MESSAGE("chestInserted1st");
				}

				NiNode* comInserted1st = (NiNode*)node->GetObjectByName("COMInserted1st");
				if (comInserted1st) {
					NiMatrix3 rot = GetRotationMatrix33(rotZ * toRad, 0, 0);
					comInserted1st->local.rotate = rot;
					comInserted1st->local.translate = NiPoint3(translateDist * leanWeight, 0, 0);
					//_MESSAGE("comInserted");
				}

				NiNode* cameraInserted1st = (NiNode*)node->GetObjectByName("CameraInserted1st");
				NiNode* camera = (NiNode*)node->GetObjectByName("Camera");
				if (camera && cameraInserted1st) {
					if (!leanR6Style) {
						NiMatrix3 rot = GetRotationMatrix33(rotZ * toRad, 0, 0);
						cameraInserted1st->local.translate = NiPoint3(translateDist * leanWeight, 0, 0);
						cameraInserted1st->local.rotate = rot;
					}
					else {
						NiMatrix3 rot = GetRotationMatrix33(-rotZ * toRad, 0, 0);
						NiPoint3 zoomData = NiPoint3();
						if (p->currentProcess && p->currentProcess->middleHigh) {
							BSTArray<EquippedItem> equipped = p->currentProcess->middleHigh->equippedItems;
							if (equipped.size() != 0 && equipped[0].item.instanceData &&
								((TESObjectWEAP::InstanceData*)equipped[0].item.instanceData.get())->type == 9) {
								zoomData = ((TESObjectWEAP::InstanceData*)equipped[0].item.instanceData.get())->zoomData->zoomData.cameraOffset;
							}
						}
						NiPoint3 camPos = rot * (camera->local.translate + zoomData);
						cameraInserted1st->local.translate = camPos + Inverse(rot) * (camPos * -1) + NiPoint3(translateDist * leanWeight, 0, 0);
						cameraInserted1st->local.rotate.MakeIdentity();
						//_MESSAGE("cam %f %f %f", cameraInserted->local.translate.x, cameraInserted->local.translate.y, cameraInserted->local.translate.z);
					}
					//_MESSAGE("cameraInserted");
				}
			}

			/*BSBound* bbx = (BSBound*)node->GetExtraData("BBX");

			if (bbx) {
				//From https://github.com/WirelessLan/F4SEPlugins/blob/main/PedoHeightFix/SkeletonManager.cpp
				float height = 64.0f * GetActorScale(a);
				float squeezedHeight = height / 2.0f + height / 2.0f * cos(rotZ * toRad);
				float squeezedWidth = 14.0f * height / squeezedHeight;
				_MESSAGE("New bound width %f height %f", squeezedWidth, squeezedHeight);

				bbx->center.y = 14.0f - squeezedWidth;
				bbx->extents.y = squeezedWidth;
				bbx->center.z = squeezedHeight;
				bbx->extents.z = squeezedHeight;
			}*/

			float step = rotReturnStep * conditionalMultiplier * timeMult;
			float retDiv = max(pow(rotReturnDiv * conditionalMultiplier, timeMult), rotReturnDivMin);
			rotX /= retDiv;
			rotY /= retDiv;
			//_MESSAGE("retDiv %f rotX %f rotY %f", retDiv, rotX, rotY);
			if (abs(rotX) * (abs(rotX) - step) <= 0) {
				rotX = 0;
			}
			else {
				if (rotX > 0) {
					rotX -= step;
				}
				else {
					rotX += step;
				}
			}
			if (abs(rotY) * (abs(rotY) - step) <= 0) {
				rotY = 0;
			}
			else {
				if (rotY > 0) {
					rotY -= step;
				}
				else {
					rotY += step;
				}
			}
		}
		lastRun = *ptr_engineTime;
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

class InputEventReceiverOverride : public BSInputEventReceiver {
public:
	typedef void (InputEventReceiverOverride::* FnPerformInputProcessing)(InputEvent* a_queueHead);

	void ProcessButtonEvent(const ButtonEvent* evn) {
		uint32_t id = evn->idCode;
		if (evn->device == INPUT_DEVICE::kMouse)
			id += 256;

		if (buttonDevMode) {
			_MESSAGE("Button event fired id %d held %f", id, evn->heldDownSecs);
		}

		if (!leanADSOnly || (leanADSOnly && IsInADS(p))) {
			if (toggleLean) {
				if (evn->value && evn->heldDownSecs == 0) {
					if ((leanState == 1 && id == leanLeft)
						|| (leanState == -1 && id == leanRight)) {
						leanState = 0;
					}
					else {
						if (id == leanLeft) {
							leanState = 1;
						}
						else if (id == leanRight) {
							leanState = -1;
						}
					}
				}
			}
			else {
				if (evn->value) {
					if (id == leanLeft) {
						leanState = 1;
					}
					else if (id == leanRight) {
						leanState = -1;
					}
				}
				else {
					if (id == leanLeft || id == leanRight) {
						leanState = 0;
					}
				}
			}
		}
	}

	void HookedPerformInputProcessing(InputEvent* a_queueHead) {
		if (!UI::GetSingleton()->menuMode) {
			InputEvent* evn = a_queueHead;
			while (evn) {
				if (evn->eventType == INPUT_EVENT_TYPE::kButton) {
					ProcessButtonEvent((ButtonEvent*)evn);
				}
				evn = evn->next;
			}
		}
		FnPerformInputProcessing fn = fnHash.at(*(uint64_t*)this);
		if (fn) {
			(this->*fn)(a_queueHead);
		}
	}

	void HookSink() {
		uint64_t vtable = *(uint64_t*)this;
		auto it = fnHash.find(vtable);
		if (it == fnHash.end()) {
			FnPerformInputProcessing fn = SafeWrite64Function(vtable, &InputEventReceiverOverride::HookedPerformInputProcessing);
			fnHash.insert(std::pair<uint64_t, FnPerformInputProcessing>(vtable, fn));
		}
	}

	void UnHookSink() {
		uint64_t vtable = *(uint64_t*)this;
		auto it = fnHash.find(vtable);
		if (it == fnHash.end())
			return;
		SafeWrite64Function(vtable, it->second);
		fnHash.erase(it);
	}

protected:
	static std::unordered_map<uint64_t, FnPerformInputProcessing> fnHash;
};
std::unordered_map<uint64_t, InputEventReceiverOverride::FnPerformInputProcessing> InputEventReceiverOverride::fnHash;

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
		if (!evn.loaded) {
			if (evn.formId == 0x14) {
				_MESSAGE("Player unloaded");
			}
			return BSEventNotifyControl::kContinue;
		}
		if (evn.formId == 0x14) {
			_MESSAGE("Player loaded");
			PreparePlayerSkeleton(p->Get3D(false)->IsNode());
			PreparePlayerSkeleton(p->Get3D(true)->IsNode());
		}
		return BSEventNotifyControl::kContinue;
	}
	F4_HEAP_REDEFINE_NEW(ObjectLoadWatcher);
};

#pragma endregion

#pragma region Initializers

void InitializePlugin() {
	p = PlayerCharacter::GetSingleton();
	pcam = PlayerCamera::GetSingleton();
	((InputEventReceiverOverride*)((uint64_t)pcam + 0x38))->HookSink();
	_MESSAGE("PlayerCharacter %llx PlayerCamera %llx", p, pcam);
	pc = PlayerControls::GetSingleton();
	SwayHandler* sh = new SwayHandler(pc->data);
	pc->RegisterHandler((PlayerInputHandler*)sh);
	((LeanLookHandler*)pc->lookHandler)->HookEvents();
	uint64_t PCVtable = REL::Relocation<uint64_t>{ PlayerCharacter::VTABLE[13] }.address();
	PCWatcher->HookSink(PCVtable);
	leftAttackCondition = (ActorValueInfo*)TESForm::GetFormByID(0x00036E);
	rightAttackCondition = (ActorValueInfo*)TESForm::GetFormByID(0x00036F);
	_MESSAGE("Hooked to event");
	uint8_t bytes[] = { 0xE9, 0x1C, 0x06, 0x00, 0x00, 0x90 };
	REL::safe_write<uint8_t>(ptr_ADS_DistanceCheck.address(), std::span{ bytes });
	for (auto it = GameSettingCollection::GetSingleton()->settings.begin(); it != GameSettingCollection::GetSingleton()->settings.end(); ++it) {
		if (it->first == "fPlayerCoverPeekTime") {
			it->second->SetFloat(0.0f);
			_MESSAGE("%s changed to %f", it->first.c_str(), it->second->_value);
		}
	}
	ObjectLoadWatcher* olw = new ObjectLoadWatcher();
	ObjectLoadedEventSource::GetSingleton()->RegisterSink(olw);
	rayInterface = (CachedRaycastData*)new FakeCachedRaycastData();
}

void LoadConfigs() {
	ini.LoadFile("Data\\F4SE\\Plugins\\UneducatedShooter.ini");
	rotLimitX = std::stof(ini.GetValue("Inertia", "rotLimitX", "12.0"));
	rotLimitY = std::stof(ini.GetValue("Inertia", "rotLimitY", "7.0"));
	rotDivX = std::stof(ini.GetValue("Inertia", "rotDivX", "10.0"));
	rotDivY = std::stof(ini.GetValue("Inertia", "rotDivY", "15.0"));
	rotDivXPad = std::stof(ini.GetValue("Inertia", "rotDivXPad", "4.0"));
	rotDivYPad = std::stof(ini.GetValue("Inertia", "rotDivYPad", "8.0"));
	rotADSConditionMult = std::stof(ini.GetValue("Inertia", "rotADSConditionMult", "5.0"));
	rotReturnDiv = std::stof(ini.GetValue("Inertia", "rotReturnDiv", "1.75"));
	rotReturnDivMin = std::stof(ini.GetValue("Inertia", "rotReturnDivMin", "1.05"));
	rotReturnStep = std::stof(ini.GetValue("Inertia", "rotReturnStep", "0.0"));
	rotDisableInADS = std::stoi(ini.GetValue("Inertia", "rotDisableInADS", "0")) > 0;
	leanTimeCost = std::stof(ini.GetValue("Leaning", "leanTimeCost", "1.0"));
	leanMax = std::stof(ini.GetValue("Leaning", "leanMax", "15.0"));
	leanMax3rd = std::stof(ini.GetValue("Leaning", "leanMax3rd", "30.0"));
	toggleLean = std::stoi(ini.GetValue("Leaning", "ToggleLean", "0")) > 0;
	leanLeft = std::stoi(ini.GetValue("Leaning", "LeanLeft", "0x51"), 0, 16);
	leanRight = std::stoi(ini.GetValue("Leaning", "LeanRight", "0x45"), 0, 16);
	leanADSOnly = std::stoi(ini.GetValue("Leaning", "ADSOnly", "0")) > 0;
	leanR6Style = std::stoi(ini.GetValue("Leaning", "R6Style", "0")) > 0;
	dynamicHeight = std::stoi(ini.GetValue("Height", "dynamicHeight", "1")) > 0;
	heightDiffThreshold = std::stof(ini.GetValue("Height", "heightDiffThreshold", "5.0"));
	heightBuffer = std::stof(ini.GetValue("Height", "heightBuffer", "16.0"));
	leanCollisionThreshold = std::stof(ini.GetValue("Dev", "leanCollisionThreshold", "0.08"));
	collisionDevMode = std::stoi(ini.GetValue("Dev", "collisionDevMode", "0")) > 0;
	buttonDevMode = std::stoi(ini.GetValue("Dev", "buttonDevMode", "0")) > 0;
	ini.Reset();
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
