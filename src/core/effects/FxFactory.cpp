
#include "FxFactory.h"
#include "FxProcessors.h"

namespace
{
	using Owned = FxFactory::Owned;

	// "None" passthrough - selected by default in every slot.
	class NoneFx : public FxProcessor
	{
	public:
		NoneFx() { params.emplace_back("Wet", "", 0.0, 1.0, 0.0); }
		FxId getId() const noexcept override { return FxId::None; }
		juce::String getName() const override { return "None"; }
		void process(juce::AudioBuffer<float>&) override {}
	};
}

std::vector<Owned> FxFactory::buildCategory(FxCategory cat)
{
	std::vector<Owned> out;
	out.reserve(16);
	out.emplace_back(std::make_unique<NoneFx>());

	switch (cat)
	{
		case FxCategory::Pad:
			out.emplace_back(std::make_unique<PadRollFx>());
			out.emplace_back(std::make_unique<StubFx>(FxId::PadSweep,        "Sweep"));
			out.emplace_back(std::make_unique<PadFlangerFx>());
			out.emplace_back(std::make_unique<PadVinylBrakeFx>());
			out.emplace_back(std::make_unique<PadEchoFx>());
			out.emplace_back(std::make_unique<PadReverbFx>());
			out.emplace_back(std::make_unique<PadRecordEchoFx>());
			break;

		case FxCategory::Beat:
			out.emplace_back(std::make_unique<BeatDelayFx>());
			out.emplace_back(std::make_unique<BeatEchoFx>());
			out.emplace_back(std::make_unique<StubFx>(FxId::BeatSpiral,      "Spiral"));
			out.emplace_back(std::make_unique<BeatReverbFx>());
			out.emplace_back(std::make_unique<StubFx>(FxId::BeatTrans,       "Trans"));
			out.emplace_back(std::make_unique<BeatFilterFx>());
			out.emplace_back(std::make_unique<BeatFlangerFx>());
			out.emplace_back(std::make_unique<BeatPhaserFx>());
			out.emplace_back(std::make_unique<StubFx>(FxId::BeatSlipLoop,    "SlipLoop"));
			out.emplace_back(std::make_unique<BeatRollFx>());
			out.emplace_back(std::make_unique<StubFx>(FxId::BeatPitch,       "Pitch"));
			out.emplace_back(std::make_unique<StubFx>(FxId::BeatLowCutEcho,  "L.C.Echo"));
			out.emplace_back(std::make_unique<StubFx>(FxId::BeatHelix,       "Helix"));
			out.emplace_back(std::make_unique<StubFx>(FxId::BeatMobiusSaw,   "MobSaw"));
			out.emplace_back(std::make_unique<StubFx>(FxId::BeatMobiusTri,   "MobTri"));
			break;

		case FxCategory::Release:
			out.emplace_back(std::make_unique<ReleaseVinylBrakeFx>());
			out.emplace_back(std::make_unique<ReleaseRecordEchoFx>());
			out.emplace_back(std::make_unique<ReleaseBackSpinFx>());
			break;

		default: break;
	}

	return out;
}
