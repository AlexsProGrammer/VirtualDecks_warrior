
#include "UIConstants.h"
#include "FxParameterModal.h"

namespace
{
	constexpr int kRowHeight     = 26;
	constexpr int kHeaderHeight  = 24;
	constexpr int kFooterHeight  = 28;
	constexpr int kPadding       = 8;
}

FxParameterModal::FxParameterModal(FxProcessor& processor,
                                   juce::Colour theme,
                                   SaveCallback onSave)
	: fx(processor), themeColour(theme), saveCallback(std::move(onSave))
{
	titleLabel.setText(fx.getName(), juce::dontSendNotification);
	titleLabel.setFont(juce::Font(juce::FontOptions(13.0f)).boldened());
	titleLabel.setColour(juce::Label::textColourId, themeColour);
	titleLabel.setJustificationType(juce::Justification::centredLeft);
	addAndMakeVisible(titleLabel);

	bypassToggle.setToggleState(fx.isBypassed(), juce::dontSendNotification);
	bypassToggle.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
	bypassToggle.onClick = [this]
	{
		fx.setBypassed(bypassToggle.getToggleState());
	};
	addAndMakeVisible(bypassToggle);

	resetButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(40, 40, 40, 255));
	resetButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
	resetButton.onClick = [this]
	{
		fx.resetParameters();
		for (size_t i = 0; i < rows.size() && i < fx.getParameters().size(); ++i)
		{
			rows[i].slider->setValue(fx.getParameters()[i].defaultValue,
			                         juce::sendNotificationSync);
		}
	};
	addAndMakeVisible(resetButton);

	saveButton.setColour(juce::TextButton::buttonColourId, themeColour.withAlpha(0.6f));
	saveButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
	saveButton.onClick = [this] { if (saveCallback) saveCallback(); };
	addAndMakeVisible(saveButton);

	buildControls();
	setSize(getPreferredWidth(), getPreferredHeight());
}

void FxParameterModal::buildControls()
{
	auto& params = fx.getParameters();
	rows.reserve(params.size());

	for (size_t i = 0; i < params.size(); ++i)
	{
		auto& p = params[i];
		Row r;

		r.label = std::make_unique<juce::Label>(juce::String(), p.name);
		r.label->setFont(juce::Font(juce::FontOptions(11.0f)));
		r.label->setColour(juce::Label::textColourId, juce::Colours::white);
		addAndMakeVisible(*r.label);

		r.slider = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal,
		                                           juce::Slider::NoTextBox);
		r.slider->setRange(p.minValue, p.maxValue);
		r.slider->setValue(p.get(), juce::dontSendNotification);
		r.slider->setColour(juce::Slider::backgroundColourId, UI::bgCard);
		r.slider->setColour(juce::Slider::trackColourId, themeColour.withAlpha(0.7f));
		r.slider->setColour(juce::Slider::thumbColourId, themeColour);

		r.valueLabel = std::make_unique<juce::Label>(juce::String(),
		                                              juce::String(p.get(), 2) + p.unit);
		r.valueLabel->setFont(juce::Font(juce::FontOptions(10.0f)));
		r.valueLabel->setColour(juce::Label::textColourId, juce::Colours::lightgrey);
		r.valueLabel->setJustificationType(juce::Justification::centredRight);
		addAndMakeVisible(*r.valueLabel);

		auto* lbl = r.valueLabel.get();
		auto* paramPtr = &p;
		r.slider->onValueChange = [paramPtr, lbl, this]
		{
			// rows[i].slider is the source - find it via the lambda capture chain.
			// Easier: use the captured paramPtr directly.
			// Walk our rows to find the matching slider for the value text update.
			for (auto& row : rows)
			{
				if (paramPtr->name == row.label->getText())
				{
					paramPtr->set(row.slider->getValue());
					lbl->setText(juce::String(paramPtr->get(), 2) + paramPtr->unit,
					             juce::dontSendNotification);
					return;
				}
			}
		};

		addAndMakeVisible(*r.slider);
		rows.push_back(std::move(r));
	}
}

int FxParameterModal::getPreferredHeight() const noexcept
{
	return kHeaderHeight + (int) rows.size() * kRowHeight + kFooterHeight + kPadding * 3;
}

void FxParameterModal::paint(juce::Graphics& g)
{
	g.fillAll(UI::bgElevated.withAlpha(0.94f));
	g.setColour(themeColour.withAlpha(0.6f));
	g.drawRect(getLocalBounds(), 1);
}

void FxParameterModal::resized()
{
	auto area = getLocalBounds().reduced(kPadding);

	auto header = area.removeFromTop(kHeaderHeight);
	titleLabel.setBounds(header.removeFromLeft(header.getWidth() / 2));
	bypassToggle.setBounds(header);

	area.removeFromTop(kPadding);

	for (auto& r : rows)
	{
		auto row = area.removeFromTop(kRowHeight);
		r.label->setBounds(row.removeFromLeft(60));
		r.valueLabel->setBounds(row.removeFromRight(50));
		r.slider->setBounds(row);
	}

	area.removeFromTop(kPadding);
	auto footer = area.removeFromBottom(kFooterHeight);
	resetButton.setBounds(footer.removeFromLeft(footer.getWidth() / 2 - 4));
	footer.removeFromLeft(8);
	saveButton.setBounds(footer);
}
