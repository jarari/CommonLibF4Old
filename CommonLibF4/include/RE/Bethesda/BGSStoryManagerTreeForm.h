#pragma once

#include "RE/Bethesda/BSFixedString.h"
#include "RE/Bethesda/BSLock.h"
#include "RE/Bethesda/BSPointerHandle.h"
#include "RE/Bethesda/BSStringT.h"
#include "RE/Bethesda/BSTArray.h"
#include "RE/Bethesda/BSTHashMap.h"
#include "RE/Bethesda/BSTSmallIndexScatterTable.h"
#include "RE/Bethesda/FormComponents.h"
#include "RE/Bethesda/TESCondition.h"
#include "RE/Bethesda/TESForms.h"
#include "RE/NetImmerse/NiSmartPointer.h"

namespace RE
{
	class BGSBaseAlias;
	class BGSQuestInstanceText;
	class BGSQuestObjective;
	class BGSStoryEvent;
	class BGSStoryManagerTreeForm;
	class PeriodicUpdateTimer;
	class BGSStoryManagerQuestNode;
	class QueuedPromoteQuestTask;
	class TESFile;
	class TESQuestStage;

	class BGSStoryManagerTreeVisitor
	{
	public:
		static constexpr auto RTTI{ RTTI_BGSStoryManagerTreeVisitor };

		enum class VisitControl;

		virtual ~BGSStoryManagerTreeVisitor();	// 00

		virtual VisitControl VisitBranchNode(BGSStoryManagerBranchNode& a_node) = 0;				 // 01
		virtual VisitControl VisitQuestNode(BGSStoryManagerQuestNode& a_node, bool a_canReset) = 0;	 // 02
		virtual VisitControl VisitQuest(TESQuest& a_quest) = 0;										 // 03
		virtual void Revert() = 0;																	 // 04

		// members
		PeriodicUpdateTimer* timer;							// 08
		std::int32_t currentCursorDepth;					// 10
		BGSStoryManagerQuestNode* lastQuestParent;			// 18
		BSTArray<BGSStoryManagerTreeForm*> cursorAncestry;	// 20
		std::uint32_t queryID;								// 38
	};
	static_assert(sizeof(BGSStoryManagerTreeVisitor) == 0x40);

	class BGSStoryManagerTreeForm :
		public TESForm	// 00
	{
	public:
		static constexpr auto RTTI{ RTTI_BGSStoryManagerTreeForm };
		static constexpr auto FORM_ID{ ENUM_FORM_ID::kNONE };

		// add
		virtual std::uint32_t QChildCount() const { return 0; }														 // 4A
		virtual BGSStoryManagerTreeForm* GetChild([[maybe_unused]] std::uint32_t a_index) const { return nullptr; }	 // 4B
		virtual TESCondition* QConditions() = 0;																	 // 4C
		virtual BGSStoryManagerTreeVisitor::VisitControl AcceptVisitor(BGSStoryManagerTreeVisitor* a_visitor) = 0;	 // 4D

		// members
		std::uint32_t lastVisitorID;  // 20
	};
	static_assert(sizeof(BGSStoryManagerTreeForm) == 0x28);

	struct QUEST_DATA
	{
	public:
		// members
		float questDelayTime;	// 0
		std::uint16_t flags;	// 4
		std::int8_t priority;	// 6
		std::int8_t questType;	// 7
	};
	static_assert(sizeof(QUEST_DATA) == 0x8);

	class TESQuest :
		public BGSStoryManagerTreeForm,	 // 000
		public TESFullName				 // 028
	{
	public:
		static constexpr auto RTTI{ RTTI_TESQuest };
		static constexpr auto FORM_ID{ ENUM_FORM_ID::kQUST };

		struct AliasesAccess;
		struct ListObjectivesAccess;
		struct ListStagesAccess;

		// members
		BSTArray<BGSQuestInstanceText*> instanceData;												   // 038
		std::uint32_t currentInstanceID;															   // 050
		BSTArray<BSTTuple<TESFile*, std::uint32_t>> fileOffsets;									   // 058
		BSTArray<BGSBaseAlias*> aliases;															   // 070
		BSTHashMap<std::uint32_t, BGSLocation*> aliasedLocMap;										   // 088
		BSTArray<BSTSmallArray<ObjectRefHandle>> aliasedHandles;									   // 0B8
		BSReadWriteLock aliasAccessLock;															   // 0D0
		BGSLocation* nonDormantLocation;															   // 0D8
		TESGlobal* questCompleteXPGlobal;															   // 0E0
		BSFixedString swfFile;																		   // 0E8
		QUEST_DATA data;																			   // 0F0
		std::uint32_t eventID;																		   // 0F8
		BSTArray<TESQuestStage*> stages;															   // 100
		BSTArray<BGSQuestObjective*> objectives;													   // 118
		BSTSmallIndexScatterTable<BSTArray<TESQuestStage*>, ListStagesAccess> stageTable;			   // 130
		BSTSmallIndexScatterTable<BSTArray<BGSQuestObjective*>, ListObjectivesAccess> objectiveTable;  // 150
		BSTSmallIndexScatterTable<BSTArray<BGSBaseAlias*>, AliasesAccess> aliasesTable;				   // 170
		TESCondition objConditions;																	   // 190
		TESCondition storyManagerConditions;														   // 198
		BSTHashMap<BGSDialogueBranch*, BSTArray<TESTopic*>*> branchedDialogues[2];					   // 1A0
		BSTArray<TESTopic*> topics[6];																   // 200
		BSTArray<BGSScene*> scenes;																	   // 290
		BSTArray<TESGlobal*>* textGlobal;															   // 2A8
		std::uint32_t totalRefsAliased;																   // 2B0
		std::uint16_t currentStage;																	   // 2B4
		bool alreadyRun;																			   // 2B6
		BSStringT<char> formEditorID;																   // 2B8
		const BGSStoryEvent* startEventData;														   // 2C8
		NiPointer<QueuedPromoteQuestTask> promoteTask;												   // 2D0
		BSTArray<ObjectRefHandle> promotedRefsArray;												   // 2D8
	};
	static_assert(sizeof(TESQuest) == 0x2F0);
}