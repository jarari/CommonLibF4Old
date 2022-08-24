#include <Havok.h>
#include <Utilities.h>
#include <MathUtils.h>
#include <half.h>
#define max(a,b)            (((a) > (b)) ? (a) : (b))
#define min(a,b)            (((a) < (b)) ? (a) : (b))
using namespace RE;
using std::unordered_map;

const F4SE::TaskInterface* taskInterface;
PlayerCharacter* p;
PlayerCamera* pcam;
REL::Relocation<uintptr_t> ptr_PCUpdateMainThread{ REL::ID(633524), 0x22D };
uintptr_t PCUpdateMainThreadOrig;

void HookedUpdate() {
	if (pcam->currentState == pcam->cameraStates[CameraState::k3rdPerson]
		|| pcam->currentState == pcam->cameraStates[CameraState::kFirstPerson]) {
		if (p->Get3D() && p->currentProcess && p->currentProcess->middleHigh) {
			BSTArray<EquippedItem> equipped = p->currentProcess->middleHigh->equippedItems;
			if (equipped.size() != 0 && equipped[0].item.instanceData) {
				TESObjectWEAP::InstanceData* instance = (TESObjectWEAP::InstanceData*)equipped[0].item.instanceData.get();
				if (instance->type == 9 && instance->ammo && p->weaponState != WEAPON_STATE::kSheathed) {
					BGSProjectile* projForm = instance->ammo->data.projectile;
					if (instance->rangedData->overrideProjectile) {
						projForm = instance->rangedData->overrideProjectile;
					}
					if (projForm) {
						bool firstPerson = p->Get3D(true) == p->Get3D();
						NiPoint3 fpBase;
						NiPoint3 fpOffset;
						NiNode* projNode = (NiNode*)p->Get3D()->GetObjectByName("ProjectileNode");
						NiNode* weapon = (NiNode*)p->Get3D()->GetObjectByName("Weapon");
						if (!projNode) {
							projNode = weapon;
						}
						NiPoint3 newPos = projNode->world.translate;
						NiPoint3 dir = Normalize(p->bulletAutoAim - newPos);
						NiPoint3 gunDir = Normalize(ToUpVector(projNode->world.rotate));
						NiPoint3 camDir = Normalize(ToUpVector(pcam->cameraRoot->world.rotate));
						if (firstPerson) {
							fpBase = p->data.location;
							NiNode* camera = (NiNode*)p->Get3D()->GetObjectByName("Camera");
							fpOffset = pcam->cameraRoot->world.translate - camera->world.translate;
							dir = camDir;
							newPos = pcam->cameraRoot->world.translate + dir * 25.f;
						}

						float gunAimDiffThreshold = 0.523f;
						float camFovThreshold = 0.85f;
						float gunAimDiff = acos(DotProduct(camDir, gunDir));
						if (p->gunState == 1 || p->gunState == 3 || p->gunState == 4 || gunAimDiff > gunAimDiffThreshold) {
							dir = gunDir;
						}

						F4::bhkPickData pick = F4::bhkPickData();
						pick.SetStartEnd(newPos, newPos + dir * 10000.f);
						hkMemoryRouter* memRouter = hkMemoryRouter::GetInstancePtr();
						int32_t memSize = 0x30;
						int32_t arrSize = 0x60 * 0xA;
						hknpAllHitsCollector* collector = (hknpAllHitsCollector*)memRouter->heap->BufAlloc(memSize);
						hkMemoryAllocator* containerHeapAllocator = (hkMemoryAllocator*)REL::Relocation<hkMemoryAllocator*>{ REL::ID(409811) }.address();
						stl::emplace_vtable<hknpAllHitsCollector>(collector);
						collector->hits._data = (hknpCollisionResult*)containerHeapAllocator->BufAlloc(arrSize);
						collector->hits._capacityAndFlags = 0x8000000A;
						*(uintptr_t*)((uintptr_t)&pick + 0xD0) = (uintptr_t)collector;
						*(uint32_t*)((uintptr_t)&pick + 0xD8) = 0;
						if (projForm->data.collisionLayer) {
							uint32_t index = projForm->data.collisionLayer->collisionIdx;
							uint64_t filter = *(uint64_t*)((*REL::Relocation<uint64_t*>{ REL::ID(469495) }) + 0x1A0 + 0x8 * index) | 0x40000000;
							uint64_t flag = 0x1C15160;
							if (!((BGSProjectileEx*)projForm)->CollidesWithSmallTransparentLayer())
								flag = 0x15C15160;
							*(uint64_t*)((uintptr_t)&pick + 0xC8) = filter & ~flag;
						}
						*(uint32_t*)((uintptr_t)&pick + 0x0C) = ((((ActorEx*)p)->GetCurrentCollisionGroup() << 16) | 0x9);
						NiAVObject* nodeHit = F4::CombatUtilities::CalculateProjectileLOS(p, projForm, pick);
						if (pick.HasHit()) {
							/*for (int i = 0; i < pick.GetAllCollectorRayHitSize(); ++i) {
								hknpCollisionResult res;
								pick.GetAllCollectorRayHitAt(i, res);
								NiPoint3 hitPos = res.position / *ptr_fBS2HkScale;
								NiPoint3 hitNormal = res.normal;
								if (i == 0) {
									laserPos = hitPos + hitNormal * 2.f;
									laserNormal = hitNormal;
								}
								_MESSAGE("[%d] hitPos %f %f %f hitNormal %f %f %f fraction %f", i, hitPos.x, hitPos.y, hitPos.z, hitNormal.x, hitNormal.y, hitNormal.z, res.fraction.storage);
								_MESSAGE("[%d] hitBodyID %04x queryBodyID %04x", i, res.hitBodyInfo.m_bodyId.value, res.queryBodyInfo.m_bodyId);
							}*/
							hknpCollisionResult res;
							pick.GetAllCollectorRayHitAt(0, res);
							NiPoint3 laserNormal = res.normal;
							NiPoint3 laserPos = res.position / *ptr_fBS2HkScale + laserNormal * 2.f;
							if (weapon) {
								Visit(weapon, [&](NiAVObject* obj) {
									if (!obj->IsTriShape())
										return false;
									NiAVObject* laserBeam = nullptr;
									NiAVObject* laserDot = nullptr;
									if (obj->name == "_LaserBeam")
										laserBeam = obj;
									else if (obj->name == "_LaserDot")
										laserDot = obj;

									if (laserBeam) {
										if (laserBeam->parent) {
											NiPoint3 diff = laserPos - (laserBeam->parent->world.translate + fpBase);
											float dist = Length(diff);
											float laserLen = 759.f;
											using namespace F4::BSGraphics;
											TriShape* triShape = *(TriShape**)((uintptr_t)laserBeam + 0x148);
											VertexDesc* vertexDesc = (VertexDesc*)((uintptr_t)laserBeam + 0x150);
											int32_t vertexCount = *(int32_t*)((uintptr_t)laserBeam + 0x164);
											uint32_t vertexSize = vertexDesc->GetSize();
											uint32_t posOffset = vertexDesc->GetAttributeOffset(Vertex::VA_POSITION);
											float ymin = std::numeric_limits<float>::infinity();
											float ymax = -ymin;
											if (triShape && triShape->buffer08) {
												for (int v = 0; v < vertexCount; ++v) {
													uintptr_t posPtr = (uintptr_t)triShape->buffer08->rawVertexData + v * vertexSize + posOffset;
													NiPoint3 pos{ half_float::half_cast<float, std::float_round_style::round_toward_zero>(*(half_float::half*)(posPtr))
														, half_float::half_cast<float, std::float_round_style::round_toward_zero>(*(half_float::half*)(posPtr + 0x2))
														, half_float::half_cast<float, std::float_round_style::round_toward_zero>(*(half_float::half*)(posPtr + 0x4)) };
													ymin = min(ymin, pos.y);
													ymax = max(ymax, pos.y);
												}
											}
											laserLen = ymax - ymin;
											NiMatrix3 scale = GetScaleMatrix(1, dist / laserLen, 1);
											NiPoint3 targetDir = Normalize(laserPos - (laserBeam->parent->world.translate + fpBase));
											NiPoint3 axis = Normalize(laserBeam->parent->world.rotate * CrossProduct(targetDir, gunDir));
											float ang = acos(max(min(DotProduct(targetDir, gunDir), 1.f), -1.f));
											laserBeam->local.rotate = scale * GetRotationMatrix33(axis, ang);
											NiUpdateData ud;
											laserBeam->UpdateTransformAndBounds(ud);
										}
									}
									if (laserDot) {
										if (laserDot->parent) {
											NiPoint3 diff = laserPos - (laserDot->parent->world.translate + fpBase);
											NiPoint3 a = NiPoint3(0, 0, 1);
											NiPoint3 axis = Normalize(CrossProduct(a, laserNormal));
											float ang = acos(max(min(DotProduct(a, laserNormal), 1.f), -1.f));
											laserDot->local.rotate = GetRotationMatrix33(axis, -ang) * Inverse(laserDot->parent->world.rotate);
											laserDot->local.translate = laserDot->parent->world.rotate * diff;
											NiUpdateData ud;
											laserDot->UpdateTransformAndBounds(ud);
										}
									}
									return false;
								});
							}
						}
						arrSize = 0x60 * (collector->hits._capacityAndFlags & 0x3FFFFFFF);
						containerHeapAllocator->BufFree(collector->hits._data, arrSize);
						memRouter->heap->BufFree(collector, memSize);
					}
				}
			}
		}
	}
	typedef void (* FnUpdate)();
	FnUpdate fn = (FnUpdate)PCUpdateMainThreadOrig;
	if (fn)
		(*fn)();
}

void InitializePlugin() {
	p = PlayerCharacter::GetSingleton();
	pcam = PlayerCamera::GetSingleton();
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
	PCUpdateMainThreadOrig = trampoline.write_call<5>(ptr_PCUpdateMainThread.address(), &HookedUpdate);

	taskInterface = F4SE::GetTaskInterface();
	const F4SE::MessagingInterface* message = F4SE::GetMessagingInterface();
	message->RegisterListener([](F4SE::MessagingInterface::Message* msg) -> void {
		if (msg->type == F4SE::MessagingInterface::kGameDataReady) {
			InitializePlugin();
		}
	});

	return true;
}
