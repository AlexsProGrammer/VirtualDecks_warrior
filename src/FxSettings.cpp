
#include "FxSettings.h"

namespace FxSettings
{
	static const char* kRoot         = "FxSettings";
	static const char* kDeck         = "Deck";
	static const char* kDeckIndex    = "index";
	static const char* kSlot         = "Slot";
	static const char* kSlotCategory = "category";
	static const char* kSelectedId   = "selectedId";
	static const char* kProcessor    = "Processor";
	static const char* kProcessorId  = "id";
	static const char* kParam        = "Param";
	static const char* kParamName    = "name";
	static const char* kParamValue   = "value";

	juce::File getSettingsFile()
	{
		auto dir = juce::File::getSpecialLocation(juce::File::userHomeDirectory)
		               .getChildFile(".otodecks");
		dir.createDirectory();
		return dir.getChildFile("FxSettings.xml");
	}

	static juce::String categoryToString(FxCategory c)
	{
		switch (c) { case FxCategory::Pad: return "Pad";
		             case FxCategory::Beat: return "Beat";
		             case FxCategory::Release: return "Release";
		             default: return "Unknown"; }
	}

	static FxCategory stringToCategory(const juce::String& s)
	{
		if (s == "Pad")     return FxCategory::Pad;
		if (s == "Beat")    return FxCategory::Beat;
		if (s == "Release") return FxCategory::Release;
		return FxCategory::Pad;
	}

	static void writeDeck(juce::XmlElement& deckEl, const FxChain& chain)
	{
		for (int s = 0; s < FxChain::kNumSlots; ++s)
		{
			const auto cat = (FxCategory) s;
			auto* slotEl = deckEl.createNewChildElement(kSlot);
			slotEl->setAttribute(kSlotCategory, categoryToString(cat));

			const auto& list = chain.getProcessors(cat);
			const int activeIdx = chain.getActiveIndex(cat);
			const FxId activeId = (activeIdx >= 0 && activeIdx < (int) list.size())
			                          ? list[(size_t) activeIdx]->getId() : FxId::None;
			slotEl->setAttribute(kSelectedId, (int) activeId);

			for (const auto& proc : list)
			{
				auto* procEl = slotEl->createNewChildElement(kProcessor);
				procEl->setAttribute(kProcessorId, (int) proc->getId());
				for (const auto& p : proc->getParameters())
				{
					auto* paramEl = procEl->createNewChildElement(kParam);
					paramEl->setAttribute(kParamName, p.name);
					paramEl->setAttribute(kParamValue, p.get());
				}
			}
		}
	}

	bool saveAll(const FxChain& deck0, const FxChain& deck1)
	{
		juce::XmlElement root (kRoot);

		auto* d0 = root.createNewChildElement(kDeck); d0->setAttribute(kDeckIndex, 0);
		writeDeck(*d0, deck0);

		auto* d1 = root.createNewChildElement(kDeck); d1->setAttribute(kDeckIndex, 1);
		writeDeck(*d1, deck1);

		const auto file = getSettingsFile();
		return root.writeTo(file, {});
	}

	static void readDeck(const juce::XmlElement& deckEl, FxChain& chain)
	{
		for (auto* slotEl : deckEl.getChildWithTagNameIterator(kSlot))
		{
			const auto cat   = stringToCategory(slotEl->getStringAttribute(kSlotCategory));
			const int  selId = slotEl->getIntAttribute(kSelectedId, (int) FxId::None);

			for (auto* procEl : slotEl->getChildWithTagNameIterator(kProcessor))
			{
				const FxId id = (FxId) procEl->getIntAttribute(kProcessorId, (int) FxId::None);
				const int  procIdx = chain.indexOf(cat, id);
				if (procIdx <= 0) continue;
				auto& proc = *chain.getProcessors(cat)[(size_t) procIdx];

				for (auto* paramEl : procEl->getChildWithTagNameIterator(kParam))
				{
					const auto name = paramEl->getStringAttribute(kParamName);
					const auto val  = paramEl->getDoubleAttribute(kParamValue);
					for (auto& p : proc.getParameters())
					{
						if (p.name == name) { p.set(val); break; }
					}
				}
			}

			const int idx = chain.indexOf(cat, (FxId) selId);
			chain.setActiveIndex(cat, idx);
		}
	}

	bool loadInto(FxChain& deck0, FxChain& deck1)
	{
		const auto file = getSettingsFile();
		if (! file.existsAsFile()) return false;

		auto root = juce::XmlDocument::parse(file);
		if (root == nullptr || ! root->hasTagName(kRoot)) return false;

		for (auto* deckEl : root->getChildWithTagNameIterator(kDeck))
		{
			const int idx = deckEl->getIntAttribute(kDeckIndex, 0);
			if (idx == 0) readDeck(*deckEl, deck0);
			else if (idx == 1) readDeck(*deckEl, deck1);
		}
		return true;
	}
}
