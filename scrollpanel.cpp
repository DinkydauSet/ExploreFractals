/*
    ExploreFractals, a tool for testing the effect of Mandelbrot set Julia morphings
    Copyright (C) 2021  DinkydauSet

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef SCROLLPANEL_H
#define SCROLLPANEL_H

#include <nana/gui.hpp>
#include <nana/gui/place.hpp>
#include <nana/gui/widgets/panel.hpp>
#include <nana/gui/widgets/scroll.hpp>

//this program
#include "common.cpp"

namespace GUI {

using namespace nana;

/*
	panel with a vertical scrollbar

	Usage:
		1. Create a class (for example named MyPanel) for the kind of panel that you want to scroll through.
		2. Instantiate a ScrollPanel<MyPanel> (for example named scrollPanel).
		3. add scrollPanel.visible to the form or panel where you want to see it. Example:

			class MyPanel : public panel<true> {
				...
			};

			class MyForm : public form {
				place pl {*this};
				ScrollPanel<MyPanel> scrollPanel {*this};
				MyForm() {	
					pl.div("<x>");
					pl["x"] << scrollPanel.visible; //add the visible part to the form
				}
			};

	Working:
		The parent of "visible" is the window wd. The parent of "content" is "visible" (see the ScrollPanel initialization list). That means the content is not directly displayed on the window. Only the visible part is directly displayed on the window. The visible part contains the content as a widget. Like any other widget that is too large to fit in a panel, only a part of it can be seen.

		The scrollbar is programmed to move the content around on the VisiblePart panel, which affects the part that can be seen. The most important work is done by the move function.
*/
template <typename ContentPanel>
class ScrollPanel
{
public:
	class VisiblePart : public panel<false>
	{ public:
		place pl{ *this };
		scroll<true> vscrollbar{ *this, rectangle() };
		VisiblePart(window wd) : panel<false>(wd)
		{
			pl.div("<content><scroll weight=15>");
			pl["scroll"] << vscrollbar;
			pl.collocate();
		}
	};
	
	VisiblePart visible;
	ContentPanel content;

	ScrollPanel(window wd)
	: visible(wd)
	, content(visible)
	{
		static_assert(
			is_base_of<panel<true>, ContentPanel>::value
			|| is_base_of<panel<false>, ContentPanel>::value
		, "ContentPanel must be a nana::panel");

		scroll<true>& scroll = visible.vscrollbar;
		scroll.step(40);

		scroll.events().value_changed([&, this](const arg_scroll& arg)
		{
			content.move(0, -1 * scroll.value());
		});

		visible.events().resized([&, this](const arg_resized& arg)
		{
			recalculateScrollbar(arg.height);
		});
	}

	/*
		measures the height of the content by looking at every widget in the panel
	*/
	unsigned contentHeight() {
		unsigned height = 0;
		API::enum_widgets(content, false, [&height](widget& wdg)
		{
			height = max({height, wdg.pos().y + wdg.size().height});
		});
		return height;
	}

	unsigned contentWidth() {
		return visible.size().width - visible.vscrollbar.size().width;
	}

	void recalculateScrollbar(int visibleHeight)
	{
		scroll<true>& scroll = visible.vscrollbar;

		unsigned height = contentHeight();

		//When the window is enlarged, this makes sure the new space is used by the panel by adjusting the displacement. Without this, the panel remains scrolled.
		int panel_displacement = scroll.value();
		int emptyPanelSpaceY = visible.size().height - (height - panel_displacement);
		if (emptyPanelSpaceY > 0) {
			panel_displacement = max({0, panel_displacement - emptyPanelSpaceY});
			content.move(0, -1 * panel_displacement);
		}
		
		scroll.range(visibleHeight);
		scroll.amount(height);

		//Hide the scrollbar when the content fits in the visible part.
		if (visibleHeight >= height) {
			visible.pl.modify("scroll", "weight=0");
			visible.pl.collocate();
			//scroll.hide(); //hiding causes 1 frame where the scrollbar turns black before the collocate takes place. I don't know why, and I don't know if it matters if the scrollbar is not hidden while it's 0 pixels wide.
		}
		else {
			//scroll.show();
			visible.pl.modify("scroll", "weight=15");
			visible.pl.collocate();
		}
		
		unsigned width = contentWidth();
		content.size(nana::size(width, height));
	}
};

} //namespace GUI

#endif