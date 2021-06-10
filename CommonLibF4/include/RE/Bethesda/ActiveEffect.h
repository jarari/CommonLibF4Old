#pragma once

#include "RE/Bethesda/TESForms.h"

namespace RE {
	class ActiveEffect;
	class MagicItem;
	class TESForm;
	class TESObjectREFR;

	class ActiveEffectReferenceEffectController {
	public:
		virtual ~ActiveEffectReferenceEffectController();

		//	void			** _vtbl;	// 00
		ActiveEffect* effect;  // 08
							   // possibly more
	};

	class __declspec(novtable) ActiveEffect :
		public BSIntrusiveRefCounted {
	public:
		static constexpr auto RTTI{ RTTI::ActiveEffect };
		static constexpr auto VTABLE{ VTABLE::ActiveEffect };
		static constexpr auto FORM_ID{ ENUM_FORM_ID::kActiveEffect };

		enum {
			kFlag_Inactive = 0x8000
		};

		virtual ~ActiveEffect();

		//	void					** _vtbl;		// 00
		ActiveEffectReferenceEffectController controller;
		std::int32_t unk18;
		float unk1C;
		float unk20;
		float unk24;
		std::int32_t unk28;
		std::int8_t unk2C;
		std::int8_t pad2D[3];
		std::int32_t unk30;
		std::int32_t unk34;
		void* niNode;
		MagicItem* item;
		void* effect;					   // 50
		TESObjectREFR* reference;
		TESForm* sourceItem;
		std::int64_t unk60;
		std::int64_t un68;
		float elapsed;
		float duration;
		float magnitude;
		std::int32_t flags;
		std::int32_t unk80;
		std::int32_t effectNum;
		std::int32_t unk88;
		std::int32_t pad8C;
		std::int32_t actorValue;
		std::int32_t unk94;
		std::int64_t unk98;
	};
}
