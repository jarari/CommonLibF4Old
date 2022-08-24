#pragma once

#include "RE/NetImmerse/NiFlags.h"
#include "RE/NetImmerse/NiProperty.h"

namespace RE
{
	class __declspec(novtable) NiAlphaProperty :
		public NiProperty  // 00
	{
	public:
		static constexpr auto RTTI{ RTTI::NiAlphaProperty };
		static constexpr auto VTABLE{ VTABLE::NiAlphaProperty };
		static constexpr auto Ni_RTTI{ Ni_RTTI::NiAlphaProperty };

		NiAlphaProperty() {
			stl::emplace_vtable(this);
			NiTFlags<std::uint16_t, NiProperty> f;
			f.flags = 0xEC;
			flags = f;
			alphaTestRef = 0;
		}
		virtual ~NiAlphaProperty();

		virtual std::int32_t Type() {
			return 0;
		}

		enum class AlphaFunction
		{
			kOne,
			kZero,
			kSrcColor,
			kInvSrcColor,
			kDestColor,
			kInvDestColor,
			kSrcAlpha,
			kInvSrcAlpha,
			kDestAlpha,
			kInvDestAlpha,
			kSrcAlphaTest
		};

		enum class TestFunction
		{
			kAlways,
			kLess,
			kEqual,
			kLessEqual,
			kGreater,
			kNotEqual,
			kGreaterEqual,
			kNever
		};

		void SetDestBlendMode(AlphaFunction f) {
			using func_t = decltype(&NiAlphaProperty::SetDestBlendMode);
			REL::Relocation<func_t> func{ REL::ID(725249) };
			return func(this, f);
		}

		void SetSrcBlendMode(AlphaFunction f) {
			using func_t = decltype(&NiAlphaProperty::SetSrcBlendMode);
			REL::Relocation<func_t> func{ REL::ID(976961) };
			return func(this, f);
		}

		void SetTestMode(TestFunction f) {
			using func_t = decltype(&NiAlphaProperty::SetTestMode);
			REL::Relocation<func_t> func{ REL::ID(976961), 0x20 };
			return func(this, f);
		}

		void SetAlphaBlending(bool b) {
			using func_t = decltype(&NiAlphaProperty::SetAlphaBlending);
			REL::Relocation<func_t> func{ REL::ID(645586) };
			return func(this, b);
		}

		void SetAlphaTesting(bool b) {
			using func_t = decltype(&NiAlphaProperty::SetAlphaTesting);
			REL::Relocation<func_t> func{ REL::ID(645586), 0x20 };
			return func(this, b);
		}

		// members
		NiTFlags<std::uint16_t, NiProperty> flags;  // 28
		std::int8_t alphaTestRef;                   // 2A

		F4_HEAP_REDEFINE_ALIGNED_NEW(NiAlphaProperty);
	};
	static_assert(sizeof(NiAlphaProperty) == 0x30);
}
