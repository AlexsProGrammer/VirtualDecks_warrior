#include "UIConstants.h"
#include "DeckQueue.h"

//==============================================================================

DeckQueue::DeckQueue(juce::Colour themeColour, TrackCallback onJumpIn)
	: theme(themeColour), onJump(std::move(onJumpIn))
{
	headerLabel.setColour(juce::Label::textColourId, theme);
	headerLabel.setFont(juce::Font(10.0f, juce::Font::bold));
	headerLabel.setJustificationType(juce::Justification::centredLeft);
	addAndMakeVisible(headerLabel);

	list.setModel(this);
	list.setRowHeight(16);
	// Transparent background + no outline: DeckQueue::paint() draws the
	// rounded background, and the ListBox must not overdraw the rounded corners.
	list.setColour(juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
	list.setColour(juce::ListBox::outlineColourId,    juce::Colours::transparentBlack);
	list.setOutlineThickness(0);
	list.setOpaque(false);
	addAndMakeVisible(list);
}

//==============================================================================

void DeckQueue::pushBack(const track& t)
{
	queue.push_back(t);
	list.updateContent();
	list.repaint();
}

track DeckQueue::popFront()
{
	if (queue.empty())
		return {};
	auto t = queue.front();
	queue.pop_front();
	list.updateContent();
	list.repaint();
	return t;
}

//==============================================================================

void DeckQueue::paint(juce::Graphics& g)
{
	const float radius = UI::kCardRadius;
	auto bounds = getLocalBounds().toFloat();
	g.setColour(UI::bgRoot);
	g.fillRoundedRectangle(bounds, radius);
	// Clip child draws to rounded shape
	juce::Path clip;
	clip.addRoundedRectangle(bounds.reduced(1.0f), radius - 1.0f);
	g.reduceClipRegion(clip);
}

void DeckQueue::paintOverChildren(juce::Graphics& g)
{
	// Draw rounded border on top of child components so it's never overdrawn.
	g.setColour(theme.withAlpha(0.4f));
	g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), UI::kCardRadius, 1.0f);
}

void DeckQueue::resized()
{
	auto r = getLocalBounds().reduced(2);
	headerLabel.setBounds(r.removeFromTop(12));
	list.setBounds(r);
}

//==============================================================================

int DeckQueue::getNumRows()
{
	return (int) queue.size();
}

void DeckQueue::paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height,
                                 bool rowIsSelected)
{
	if (rowNumber < 0 || rowNumber >= (int) queue.size())
		return;

	if (rowIsSelected)
		g.fillAll(theme.withAlpha(0.35f));
	else if ((rowNumber & 1) == 0)
		g.fillAll(UI::bgElevated);

	const auto& t = queue[(size_t) rowNumber];
	g.setColour(juce::Colours::white);
	g.setFont(10.5f);
	auto idxStr = juce::String(rowNumber + 1) + ".";
	g.drawText(idxStr, 2, 0, 18, height, juce::Justification::centredLeft);

	auto lenStr = juce::String(track::getLengthString(t.lengthInSeconds, true));
	int  lenW   = 38;
	g.drawText(t.title, 22, 0, juce::jmax(0, width - 22 - lenW - 2), height,
	           juce::Justification::centredLeft, true);
	g.setColour(juce::Colours::lightgrey);
	g.drawText(lenStr, width - lenW - 2, 0, lenW, height, juce::Justification::centredRight);
}

void DeckQueue::listBoxItemClicked(int row, const juce::MouseEvent& e)
{
	if (row < 0 || row >= (int) queue.size())
		return;

	if (e.mods.isPopupMenu())
	{
		juce::PopupMenu menu;
		menu.addItem(1, "Load now");
		menu.addItem(2, "Move up", row > 0);
		menu.addItem(3, "Move down", row < (int) queue.size() - 1);
		menu.addItem(4, "Remove");
		menu.addSeparator();
		menu.addItem(5, "Clear queue");
		menu.showMenuAsync(juce::PopupMenu::Options(), [this, row](int result)
		{
			if (row < 0 || row >= (int) queue.size()) return;
			switch (result)
			{
			case 1: if (onJump) onJump(queue[(size_t) row]); break;
			case 2: std::swap(queue[(size_t) row], queue[(size_t) row - 1]); break;
			case 3: std::swap(queue[(size_t) row], queue[(size_t) row + 1]); break;
			case 4: queue.erase(queue.begin() + row); break;
			case 5: queue.clear(); break;
			default: return;
			}
			list.updateContent();
			list.repaint();
		});
		return;
	}

	if (onJump)
		onJump(queue[(size_t) row]);
}

//==============================================================================

bool DeckQueue::isInterestedInDragSource(const SourceDetails& details)
{
	return details.description.isString();
}

void DeckQueue::itemDropped(const SourceDetails& details)
{
	if (! resolveDragSource)
		return;
	auto t = resolveDragSource(details.description);
	if (t.title.isNotEmpty() || t.url.toString(false).isNotEmpty())
		pushBack(t);
}
