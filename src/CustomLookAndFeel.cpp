
#include "CustomLookAndFeel.h"

//==============================================================================

CustomLookAndFeel::CustomLookAndFeel()
{
	using juce::Slider;
	using juce::TextButton;
	using juce::Label;
	using juce::ComboBox;
	using juce::PopupMenu;
	using juce::ScrollBar;
	using juce::TextEditor;
	using juce::ListBox;
	using juce::TableHeaderComponent;
	using juce::ResizableWindow;

	// Window background.
	setColour(ResizableWindow::backgroundColourId, UI::bgRoot);

	// Sliders — defaults; deck-specific accents override per-component.
	setColour(Slider::backgroundColourId,           UI::bgCard);
	setColour(Slider::trackColourId,                UI::borderStrong);
	setColour(Slider::thumbColourId,                UI::textPrimary);
	setColour(Slider::rotarySliderFillColourId,     UI::deck1Accent);
	setColour(Slider::rotarySliderOutlineColourId,  UI::borderSubtle);
	setColour(Slider::textBoxTextColourId,          UI::textPrimary);
	setColour(Slider::textBoxBackgroundColourId,    UI::bgCard);
	setColour(Slider::textBoxOutlineColourId,       UI::borderSubtle);

	// Buttons.
	setColour(TextButton::buttonColourId,           UI::bgCard);
	setColour(TextButton::buttonOnColourId,         UI::deck1Accent);
	setColour(TextButton::textColourOffId,          UI::textPrimary);
	setColour(TextButton::textColourOnId,           UI::textPrimary);

	// Labels.
	setColour(Label::textColourId,                  UI::textPrimary);

	// ComboBox.
	setColour(ComboBox::backgroundColourId,         UI::bgCard);
	setColour(ComboBox::textColourId,               UI::textPrimary);
	setColour(ComboBox::outlineColourId,            UI::borderSubtle);
	setColour(ComboBox::arrowColourId,              UI::textSecondary);
	setColour(ComboBox::buttonColourId,             UI::bgCard);

	// PopupMenu.
	setColour(PopupMenu::backgroundColourId,        UI::bgElevated);
	setColour(PopupMenu::textColourId,              UI::textPrimary);
	setColour(PopupMenu::headerTextColourId,        UI::textSecondary);
	setColour(PopupMenu::highlightedBackgroundColourId, UI::deck1Accent.withAlpha(0.30f));
	setColour(PopupMenu::highlightedTextColourId,   UI::textPrimary);

	// ScrollBar.
	setColour(ScrollBar::backgroundColourId,        juce::Colours::transparentBlack);
	setColour(ScrollBar::thumbColourId,             UI::borderStrong);
	setColour(ScrollBar::trackColourId,             juce::Colours::transparentBlack);

	// TextEditor.
	setColour(TextEditor::backgroundColourId,       UI::bgCard);
	setColour(TextEditor::textColourId,             UI::textPrimary);
	setColour(TextEditor::highlightColourId,        UI::deck1Accent.withAlpha(0.35f));
	setColour(TextEditor::highlightedTextColourId,  UI::textPrimary);
	setColour(TextEditor::outlineColourId,          UI::borderSubtle);
	setColour(TextEditor::focusedOutlineColourId,   UI::deck1Accent);
	setColour(TextEditor::shadowColourId,           juce::Colours::transparentBlack);

	// ListBox.
	setColour(ListBox::backgroundColourId,          UI::bgCard);
	setColour(ListBox::outlineColourId,             UI::borderSubtle);
	setColour(ListBox::textColourId,                UI::textPrimary);

	// TableHeader.
	setColour(TableHeaderComponent::backgroundColourId, UI::bgElevated);
	setColour(TableHeaderComponent::outlineColourId,    UI::borderSubtle);
	setColour(TableHeaderComponent::textColourId,       UI::textSecondary);
}

//==============================================================================
// Helpers

void CustomLookAndFeel::paintPanelBackground(juce::Graphics& g, juce::Rectangle<float> bounds,
	bool elevated, float radius)
{
	if (elevated)
	{
		// Soft drop shadow rendered as a translucent rounded rect drawn behind the panel.
		juce::DropShadow shadow(juce::Colours::black.withAlpha(0.55f), 18, { 0, 4 });
		juce::Path p; p.addRoundedRectangle(bounds, radius);
		shadow.drawForPath(g, p);
	}

	juce::ColourGradient grad(UI::bgSurface.brighter(0.04f), bounds.getX(), bounds.getY(),
		UI::bgSurface.darker(0.10f), bounds.getX(), bounds.getBottom(), false);
	g.setGradientFill(grad);
	g.fillRoundedRectangle(bounds, radius);

	g.setColour(UI::borderSubtle);
	g.drawRoundedRectangle(bounds.reduced(0.5f), radius, 1.0f);
}

void CustomLookAndFeel::paintCardBackground(juce::Graphics& g, juce::Rectangle<float> bounds, float radius)
{
	g.setColour(UI::bgCard);
	g.fillRoundedRectangle(bounds, radius);
	g.setColour(UI::borderSubtle);
	g.drawRoundedRectangle(bounds.reduced(0.5f), radius, 1.0f);
}

std::unique_ptr<juce::Drawable> CustomLookAndFeel::loadIcon(const char* svgData, juce::Colour tint)
{
	if (svgData == nullptr) return nullptr;
	const std::unique_ptr<juce::XmlElement> svgXml(juce::XmlDocument::parse(svgData));
	if (svgXml == nullptr) return nullptr;

	auto drawable = juce::Drawable::createFromSVG(*svgXml);
	if (drawable != nullptr && ! tint.isTransparent())
		drawable->replaceColour(juce::Colours::white, tint);
	return drawable;
}

//==============================================================================
// Buttons

juce::Font CustomLookAndFeel::getTextButtonFont(juce::TextButton& button, int buttonHeight)
{
	const float size = juce::jlimit(10.0f, 14.0f, buttonHeight * 0.42f);
	return juce::Font(juce::FontOptions(size).withStyle(button.getToggleState() ? "Bold" : "Plain"));
}

juce::Font CustomLookAndFeel::getLabelFont(juce::Label& label)
{
	auto f = label.getFont();
	if (f.getHeight() < 0.5f)
		return juce::Font(juce::FontOptions(11.0f));
	return f;
}

juce::Font CustomLookAndFeel::getComboBoxFont(juce::ComboBox&)
{
	return juce::Font(juce::FontOptions(12.0f));
}

juce::Font CustomLookAndFeel::getPopupMenuFont()
{
	return juce::Font(juce::FontOptions(12.0f));
}

void CustomLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button, bool, bool)
{
	const bool toggled = button.getToggleState();
	g.setFont(getTextButtonFont(button, button.getHeight()));

	const auto col = button.findColour(toggled ? juce::TextButton::textColourOnId
	                                            : juce::TextButton::textColourOffId);
	g.setColour(col.withMultipliedAlpha(button.isEnabled() ? 1.0f : 0.5f));

	g.drawFittedText(button.getButtonText(),
		4, 0, button.getWidth() - 8, button.getHeight(),
		juce::Justification::centred, 2);
}

void CustomLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
	const juce::Colour& backgroundColour, bool isMouseOverButton, bool isButtonDown)
{
	const float corner = UI::kButtonRadius;
	auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);

	const bool toggled = button.getToggleState();
	juce::Colour fill;

	if (toggled)
	{
		auto onCol = button.findColour(juce::TextButton::buttonOnColourId);
		// Soft glow halo behind a toggled button.
		g.setColour(onCol.withAlpha(0.20f));
		g.fillRoundedRectangle(bounds.expanded(2.5f), corner + 2.5f);
		fill = onCol;
	}
	else
	{
		fill = backgroundColour.isOpaque() ? backgroundColour : UI::bgCard;
	}

	if (isButtonDown)        fill = fill.darker(0.15f);
	else if (isMouseOverButton) fill = fill.brighter(0.10f);

	g.setColour(fill);
	g.fillRoundedRectangle(bounds, corner);

	// Subtle border.
	g.setColour(toggled ? fill.brighter(0.20f).withAlpha(0.6f)
	                    : UI::borderSubtle);
	g.drawRoundedRectangle(bounds, corner, 1.0f);
}

//==============================================================================
// Sliders

int CustomLookAndFeel::getSliderThumbRadius(juce::Slider& slider)
{
	return juce::jlimit(8, 14, juce::jmin(slider.getWidth(), slider.getHeight()) / 4);
}

void CustomLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
	float sliderPos, float minSliderPos, float maxSliderPos,
	const juce::Slider::SliderStyle style, juce::Slider& slider)
{
	if (slider.isBar())
	{
		g.setColour(slider.findColour(juce::Slider::trackColourId));
		g.fillRect(slider.isHorizontal()
			? juce::Rectangle<float>((float)x, (float)y + 0.5f, sliderPos - (float)x, (float)height - 1.0f)
			: juce::Rectangle<float>((float)x + 0.5f, sliderPos, (float)width - 1.0f, (float)y + ((float)height - sliderPos)));
		return;
	}

	const bool horizontal = slider.isHorizontal();

	// Pill-shaped track centered along the slider axis.
	const float trackThickness = 5.0f;
	juce::Rectangle<float> track;
	if (horizontal)
		track = juce::Rectangle<float>((float)x + 6, (float)y + height * 0.5f - trackThickness * 0.5f,
		                               (float)width - 12, trackThickness);
	else
		track = juce::Rectangle<float>((float)x + width * 0.5f - trackThickness * 0.5f, (float)y + 6,
		                               trackThickness, (float)height - 12);

	g.setColour(slider.findColour(juce::Slider::backgroundColourId).withAlpha(0.85f));
	g.fillRoundedRectangle(track, trackThickness * 0.5f);

	// Filled portion → from track start to current value.
	auto fillCol = slider.findColour(juce::Slider::thumbColourId);

	juce::Rectangle<float> fill;
	if (horizontal)
		fill = juce::Rectangle<float>(track.getX(), track.getY(),
		                              juce::jmax(0.0f, sliderPos - track.getX()), track.getHeight());
	else
		fill = juce::Rectangle<float>(track.getX(), sliderPos,
		                              track.getWidth(), juce::jmax(0.0f, track.getBottom() - sliderPos));

	g.setColour(fillCol);
	g.fillRoundedRectangle(fill, trackThickness * 0.5f);

	// Two-/three-value sliders → render simple pointer arrows at min/max.
	const bool isTwoVal   = (style == juce::Slider::SliderStyle::TwoValueVertical
	                       || style == juce::Slider::SliderStyle::TwoValueHorizontal);
	const bool isThreeVal = (style == juce::Slider::SliderStyle::ThreeValueVertical
	                       || style == juce::Slider::SliderStyle::ThreeValueHorizontal);

	if (isTwoVal || isThreeVal)
	{
		const auto pCol = slider.findColour(juce::Slider::thumbColourId);
		const float pw = trackThickness * 2.0f;
		if (horizontal)
		{
			drawPointer(g, minSliderPos - pw * 0.5f, track.getY() - pw * 0.5f, pw, pCol, 2);
			drawPointer(g, maxSliderPos - pw * 0.5f, track.getBottom() - pw * 0.5f, pw, pCol, 4);
		}
		else
		{
			drawPointer(g, track.getX() - pw * 0.5f, minSliderPos - pw * 0.5f, pw, pCol, 1);
			drawPointer(g, track.getRight() - pw * 0.5f, maxSliderPos - pw * 0.5f, pw, pCol, 3);
		}
	}

	if (isTwoVal) return;

	// Circular thumb.
	const float thumbRadius = (float)getSliderThumbRadius(slider);
	juce::Point<float> thumbCenter;
	if (horizontal)
		thumbCenter = { isThreeVal ? sliderPos : sliderPos, track.getCentreY() };
	else
		thumbCenter = { track.getCentreX(), isThreeVal ? sliderPos : sliderPos };

	juce::Rectangle<float> thumb(thumbRadius * 2.0f, thumbRadius * 2.0f);
	thumb = thumb.withCentre(thumbCenter);

	// Drop shadow under thumb.
	juce::DropShadow shadow(juce::Colours::black.withAlpha(0.45f), 6, { 0, 2 });
	juce::Path tp; tp.addEllipse(thumb);
	shadow.drawForPath(g, tp);

	g.setColour(UI::textPrimary);
	g.fillEllipse(thumb);
	g.setColour(UI::borderStrong);
	g.drawEllipse(thumb.reduced(0.5f), 1.0f);

	// Inner accent dot.
	g.setColour(fillCol);
	g.fillEllipse(thumb.reduced(thumbRadius * 0.55f));
}

void CustomLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
	float sliderPos, const float rotaryStartAngle, const float rotaryEndAngle, juce::Slider& slider)
{
	auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(4.0f);
	const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
	const auto centre = bounds.getCentre();
	const float toAngle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
	const float arcStroke = juce::jmax(2.5f, radius * 0.10f);
	const float arcRadius = radius - arcStroke * 0.5f - 2.0f;

	const auto fillCol = slider.findColour(juce::Slider::rotarySliderFillColourId);
	const auto baseCol = UI::bgCard;

	// Drop shadow under knob disc.
	{
		juce::Path p;
		p.addEllipse(juce::Rectangle<float>(radius * 1.55f, radius * 1.55f).withCentre(centre));
		juce::DropShadow shadow(juce::Colours::black.withAlpha(0.55f), 10, { 0, 3 });
		shadow.drawForPath(g, p);
	}

	// Knob disc with subtle radial gradient.
	juce::Rectangle<float> disc(radius * 1.55f, radius * 1.55f);
	disc = disc.withCentre(centre);
	{
		juce::ColourGradient grad(baseCol.brighter(0.06f), centre.x, disc.getY(),
		                          baseCol.darker(0.18f),  centre.x, disc.getBottom(), false);
		g.setGradientFill(grad);
		g.fillEllipse(disc);
	}
	g.setColour(UI::borderStrong);
	g.drawEllipse(disc.reduced(0.5f), 1.0f);

	// Background arc track.
	juce::Path bgArc;
	bgArc.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f,
	                    rotaryStartAngle, rotaryEndAngle, true);
	g.setColour(UI::borderSubtle);
	g.strokePath(bgArc, juce::PathStrokeType(arcStroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

	// Filled arc indicator.
	if (toAngle > rotaryStartAngle)
	{
		juce::Path arc;
		arc.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f,
		                  rotaryStartAngle, toAngle, true);

		// Soft glow.
		g.setColour(fillCol.withAlpha(0.30f));
		g.strokePath(arc, juce::PathStrokeType(arcStroke + 3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

		g.setColour(fillCol);
		g.strokePath(arc, juce::PathStrokeType(arcStroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
	}

	// Indicator line from disc edge inward.
	const float indicatorLen = radius * 0.45f;
	juce::Point<float> outerEnd(centre.x + arcRadius * std::cos(toAngle - juce::MathConstants<float>::halfPi),
	                            centre.y + arcRadius * std::sin(toAngle - juce::MathConstants<float>::halfPi));
	juce::Point<float> innerEnd(centre.x + (arcRadius - indicatorLen) * std::cos(toAngle - juce::MathConstants<float>::halfPi),
	                            centre.y + (arcRadius - indicatorLen) * std::sin(toAngle - juce::MathConstants<float>::halfPi));
	juce::Path indicator; indicator.startNewSubPath(innerEnd); indicator.lineTo(outerEnd);
	g.setColour(UI::textPrimary);
	g.strokePath(indicator, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

	// Center dot.
	g.setColour(fillCol);
	g.fillEllipse(juce::Rectangle<float>(radius * 0.20f, radius * 0.20f).withCentre(centre));
}

//==============================================================================
// Tables

void CustomLookAndFeel::drawTableHeaderBackground(juce::Graphics& g, juce::TableHeaderComponent& header)
{
	auto r = header.getLocalBounds();

	g.setColour(UI::bgElevated);
	g.fillRect(r);

	g.setColour(UI::borderSubtle);
	g.fillRect(r.removeFromBottom(1));

	for (int i = header.getNumColumns(true); --i >= 0;)
	{
		g.setColour(UI::borderSubtle);
		g.fillRect(header.getColumnPosition(i).removeFromRight(1));
	}
}

void CustomLookAndFeel::drawTableHeaderColumn(juce::Graphics& g, juce::TableHeaderComponent& header,
	const juce::String& columnName, int /*columnId*/, int width, int height,
	bool isMouseOver, bool /*isMouseDown*/, int /*columnFlags*/)
{
	juce::Rectangle<int> area(width, height);

	g.setColour(isMouseOver ? UI::textPrimary : UI::textSecondary);
	g.setFont(juce::Font(juce::FontOptions(11.0f).withStyle("Bold")));

	g.drawFittedText(columnName, area.reduced(8, 0),
		juce::Justification::centredLeft, 1, 1.0f);
}

//==============================================================================
// ComboBox

void CustomLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool /*isButtonDown*/,
	int /*buttonX*/, int /*buttonY*/, int /*buttonW*/, int /*buttonH*/, juce::ComboBox& box)
{
	auto bounds = juce::Rectangle<float>(0.0f, 0.0f, (float)width, (float)height).reduced(0.5f);

	g.setColour(box.findColour(juce::ComboBox::backgroundColourId));
	g.fillRoundedRectangle(bounds, UI::kButtonRadius);

	g.setColour(box.findColour(juce::ComboBox::outlineColourId));
	g.drawRoundedRectangle(bounds, UI::kButtonRadius, 1.0f);

	// Chevron arrow.
	juce::Path arrow;
	const float ax = (float)width - 14.0f;
	const float ay = (float)height * 0.5f;
	arrow.startNewSubPath(ax - 4.0f, ay - 2.0f);
	arrow.lineTo(ax,        ay + 3.0f);
	arrow.lineTo(ax + 4.0f, ay - 2.0f);

	g.setColour(box.findColour(juce::ComboBox::arrowColourId));
	g.strokePath(arrow, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void CustomLookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int width, int height)
{
	auto bounds = juce::Rectangle<float>(0.0f, 0.0f, (float)width, (float)height).reduced(0.5f);
	g.setColour(UI::bgElevated);
	g.fillRoundedRectangle(bounds, UI::kCardRadius);
	g.setColour(UI::borderSubtle);
	g.drawRoundedRectangle(bounds, UI::kCardRadius, 1.0f);
}

//==============================================================================
// Scrollbar

void CustomLookAndFeel::drawScrollbar(juce::Graphics& g, juce::ScrollBar& /*bar*/, int x, int y,
	int width, int height, bool isScrollbarVertical, int thumbStartPosition,
	int thumbSize, bool isMouseOver, bool /*isMouseDown*/)
{
	juce::Rectangle<int> thumbBounds = isScrollbarVertical
		? juce::Rectangle<int>(x + width / 4, thumbStartPosition, juce::jmax(4, width / 2), thumbSize)
		: juce::Rectangle<int>(thumbStartPosition, y + height / 4, thumbSize, juce::jmax(4, height / 2));

	g.setColour((isMouseOver ? UI::borderStrong.brighter(0.30f) : UI::borderStrong).withAlpha(0.85f));
	g.fillRoundedRectangle(thumbBounds.toFloat(),
		(float)juce::jmin(thumbBounds.getWidth(), thumbBounds.getHeight()) * 0.5f);
}

//==============================================================================
// TextEditor

void CustomLookAndFeel::fillTextEditorBackground(juce::Graphics& g, int width, int height, juce::TextEditor& te)
{
	g.setColour(te.findColour(juce::TextEditor::backgroundColourId));
	g.fillRoundedRectangle(juce::Rectangle<float>(0.0f, 0.0f, (float)width, (float)height).reduced(0.5f), UI::kButtonRadius);
}

void CustomLookAndFeel::drawTextEditorOutline(juce::Graphics& g, int width, int height, juce::TextEditor& te)
{
	const auto col = te.hasKeyboardFocus(false)
		? te.findColour(juce::TextEditor::focusedOutlineColourId)
		: te.findColour(juce::TextEditor::outlineColourId);
	g.setColour(col);
	g.drawRoundedRectangle(juce::Rectangle<float>(0.0f, 0.0f, (float)width, (float)height).reduced(0.5f), UI::kButtonRadius, 1.0f);
}
