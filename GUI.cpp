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

#ifndef EXPLORE_FRACTALS_GUI_H
#define EXPLORE_FRACTALS_GUI_H

//standard library
#include <unordered_map>
#include <stdexcept>
#include <algorithm>

//Nana (GUI library)
#include <nana/gui.hpp>
#include <nana/gui/detail/bedrock.hpp>
#include <nana/gui/place.hpp>
#include <nana/gui/widgets/panel.hpp>
#include <nana/gui/widgets/menubar.hpp>
#include <nana/gui/widgets/label.hpp>
#include <nana/gui/widgets/button.hpp>
#include <nana/gui/widgets/checkbox.hpp>
#include <nana/gui/widgets/tabbar.hpp>
#include <nana/gui/widgets/listbox.hpp>
#include <nana/gui/widgets/scroll.hpp>
#include <nana/gui/widgets/slider.hpp>
#include <nana/gui/widgets/textbox.hpp>
#include <nana/gui/widgets/combox.hpp>
#include <nana/paint/image_process_selector.hpp>

//this program
#include "common.cpp"
#include "FractalParameters.cpp"
#include "FractalCanvas.cpp"
#include "Render.cpp"
#include "windows_util.cpp"
#include "utilities.cpp"
#include "scrollpanel.cpp"


static_assert(sizeof(WPARAM) == 8, "In this code WPARAM is assumed to be 8 bytes.");
static_assert(sizeof(LPARAM) == 8, "In this code LPARAM is assumed to be 8 bytes.");


namespace GUI {
	
using namespace nana;

GUIInterface* theOnlyNanaGUI; //This will be a pointer to a main_form. FractalCanvas instances don't need to know that it's a main_form. It could be "any" GUIInterface.
HWND main_form_hwnd;
bool showMorphing = false; //todo: test

namespace EFcolors {
	const color darkblue = color(10, 36, 106);
	const color normal = color(212, 208, 200); //the default boring background color for windows in windows
	const color chromiumblue = color(160, 196, 241);
	const color nearwhite = color(245, 245, 245);
	const color inactivegrey = color(192, 192, 192);
	const color normalgrey = color(128, 128, 128);

	const color activetab = normal;
	const color activetabtext = colors::black;
	const color inactivetab = nearwhite;
	const color inactivetabtext = colors::black;
}

namespace EventSource {
	enum {
		default_ = 0
		,history //this is used to prevent creation of a history item when clicking history items (which "changes" the parameters)
		,noEvents //for when ignoring the event is intended
		,fractalPanel

		//Used in the settings panel:
		,procedure
		,post_transformation
		,size
		,iters
		,inflectionZoom
		,location
		,zoomLevel
		,rotation
		,juliaSeed
		,julia
		,gradientSpeed
		,gradientOffset
		
		//other:
		,gradientSpeedSliding
		,gradientOffsetSliding
		,addInflection
		,removeInflection
		,zoomIn
		,zoomOut
		,reset
		,rotationSliderMouseUp
		,bitmapZoom
		,JSON
	};
}

//
// Custom windows api message codes used to notify the GUI thread, from another thread, that it has to do something
// A range of numbers starting with WM_APP is reserved for custom messages, so they will never conflict with codes used by microsoft.
//
namespace Message {
	//message												wParam meaning			lParam meaning
	
	//Messages related to the GUIInterface functions:
	constexpr uint RENDER_STARTED =				WM_APP + 1;	//RenderInterface*
	constexpr uint RENDER_FINISHED =				WM_APP + 2;	//RenderInterface*
	constexpr uint BITMAP_RENDER_STARTED =		WM_APP + 3;	//FractalCanvas*			uint* (bitmapRenderID)
	constexpr uint BITMAP_RENDER_FINISHED =		WM_APP + 4;	//FractalCanvas*	
	constexpr uint PARAMETERS_CHANGED =			WM_APP + 5;	//FractalCanvas*			int* (event source id)
	constexpr uint CANVAS_SIZE_CHANGED =			WM_APP + 6;	//FractalCanvas*
	constexpr uint CANVAS_RESIZE_FAILED =		WM_APP + 7; //FractalCanvas*

	//other messages
	constexpr uint SHOW_PROGRESS_UNFINISHED =	WM_APP + 8;	//RenderInterface*
	constexpr uint DRAW_BITMAP =					WM_APP + 9;	//FractalCanvas*
	constexpr uint CLEANUP_FRACTAL_TAB =			WM_APP + 10;	//FractalCanvas*
	constexpr uint CLEANUP_CHILD_WINDOW =		WM_APP + 11;//child_window_base*
}

/*
	change default font size, 0 to use the system size. All widgets created after changing the default font will use the new default font. Existing widgets keep their font.
*/
void fontsize(double points = 0) {
	paint::font globalFont{ API::typeface(nullptr).name(), points };
	globalFont.set_default();
}

//
// The refresh functions use SendMessage to notify the GUI thread that is has to do something. This prevents problems due to multiple threads using and changing the GUI's memory at the same time. The idea is that only 1 thread (the GUI thread) is allowed to:
//	* create GUI elements
//	* use GUI elements
//	* destroy GUI elements
//
void refreshDuringRender(shared_ptr<RenderInterface> render, FractalCanvas* canvas, uint renderID)
{
	uint renderSize = render->getWidth() * render->getHeight();
	this_thread::sleep_for(chrono::milliseconds(70));
	if(debug) cout << "refreshDuringRender " << renderID << " awakened" << endl;

	while (canvas->lastRenderID == renderID && canvas->activeRenders != 0) {
		if(debug) cout << "refreshDuringRender " << renderID << " waiting for lock drawingBitmap" << endl;
		{
			lock_guard<mutex> guard(drawingBitmap);
			if(debug) cout << "refreshDuringRender " << renderID << " has lock drawingBitmap" << endl;

			if (canvas->lastRenderID == renderID) { //to prevent drawing after the render is already finished, because a drawing is already made immediately after the render finishes. This also prevents an otherwise possible crash: After the immediate drawing, the Render object gets destroyed. Referencing it here can then cause a crash.

				//This notifies the GUI thread that it should update the render progress. The WPARAM parameter for the SHOW_PROGRESS_UNFINISHED message is a pointer to the RenderInterface. Because SendMessage blocks the current thread until the message is handled, the pointer can be used safely.
				SendMessage(main_form_hwnd, Message::SHOW_PROGRESS_UNFINISHED, reinterpret_cast<WPARAM>(render.get()), 0);
				SendMessage(main_form_hwnd, Message::DRAW_BITMAP, reinterpret_cast<WPARAM>(canvas), 0);
			}
			else {
				if(debug) cout << "refreshDuringRender " << renderID << " doesn't draw because the render was cancelled" << endl;
			}
		}
		if(debug) cout << "refreshDuringRender " << renderID << " released lock drawingBitmap" << endl;
		this_thread::sleep_for(std::chrono::milliseconds(100));
	}
	if(debug) cout << "refreshDuringRender thread " << renderID << " ended" << endl;
}

void refreshDuringBitmapRender(FractalCanvas* canvas, uint bitmapRenderID)
{
	this_thread::sleep_for(chrono::milliseconds(70));
	if(debug) cout << "refreshDuringBitmapRender " << bitmapRenderID << " awakened" << endl;

	while (canvas->lastBitmapRenderID == bitmapRenderID && canvas->activeBitmapRenders != 0) {
		if(debug) cout << "refreshDuringBitmapRender " << bitmapRenderID << " waiting for lock drawingBitmap" << endl;
		{
			lock_guard<mutex> guard(drawingBitmap);
			if(debug) cout << "refreshDuringBitmapRender " << bitmapRenderID << " has lock drawingBitmap" << endl;

			//The reason for the second condition is: if a render is active (a normal render - not a bitmap render), it also has a refreshthread. This thread will just do nothing in that case.
			if (canvas->lastBitmapRenderID == bitmapRenderID && canvas->activeRenders == 0) {
				SendMessage(main_form_hwnd, Message::DRAW_BITMAP, reinterpret_cast<WPARAM>(canvas), 0);
			}
			else {
				if(debug) if(canvas->activeRenders == 0) cout << "refreshDuringBitmapRender " << bitmapRenderID << " doesn't draw because the render was cancelled" << endl;
			}
		}
		if(debug) cout << "refreshDuringBitmapRender " << bitmapRenderID << " released lock drawingBitmap" << endl;
		this_thread::sleep_for(std::chrono::milliseconds(100));
	}
	if(debug) cout << "refreshDuringBitmapRender thread " << bitmapRenderID << " ended" << endl;
}


ARGB* graphics_pixelarray(const paint::graphics& graph)
{
	HBITMAP hBitmap = reinterpret_cast<HBITMAP>(const_cast<void*>(graph.pixmap()));
	DIBSECTION dib;
	GetObject(hBitmap, sizeof(dib), (LPVOID)&dib);
	BITMAP bitmap = dib.dsBm;
	return (ARGB*)bitmap.bmBits;
}

class NanaBitmapManager : public BitmapManager {
public:
	ARGB* ptPixels;
	paint::graphics graph;
	uint width;
	uint height;

	NanaBitmapManager()
		: graph(nana::size(0,0))
	{}

	~NanaBitmapManager() {
		if(debug) cout << "destroying NanaBitmapManager" << endl;
	}

	ARGB* realloc(uint width, uint height) {
		assert(width > 0); assert(height > 0);

		nana::size size(width, height);
		graph = paint::graphics(size);
		ptPixels	 = graphics_pixelarray(graph);
		this->width = width;
		this->height = height;

		if(debug) cout << "reallocated NanaBitmapManager to " << width << " " << height << endl;
		return ptPixels;
	}
};


//These functions are used to limit input to text fields in the settings panel.
bool acceptReal(wchar_t key) {
	//For some reason ctrl+c works automatically, but ctrl+v requires allowing the paste key code (which is neither ctrl nor v but something else entirely, as if ctrl+v is a key in itself?).
	switch(key) {
		case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9': case '0':
		case '.': case '-':
		case nana::keyboard::backspace:
		case nana::keyboard::del:
		case nana::keyboard::paste: //ctrl+v
			return true;
		default:
			return false;
	}
}

bool acceptNatural(wchar_t key) {
	switch(key) {
		case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9': case '0':
		case nana::keyboard::backspace:
		case nana::keyboard::del:
		case nana::keyboard::paste:
			return true;
		default:
			return false;
	}
}

bool acceptInteger(wchar_t key) {
	switch(key) {
		case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9': case '0':
		case '-':
		case nana::keyboard::backspace:
		case nana::keyboard::del:
		case nana::keyboard::paste:
			return true;
		default:
			return false;
	}
}

/*
	Text fields that accept only certain characters
*/
namespace NumberInput
{
	class real : public textbox {
	public:
		real(nana::window wd, string tipstring = "") : textbox(wd) {
			multi_lines(false);
			set_accept(acceptReal);
			if(tipstring != "") tip_string(tipstring);
		}
	};
	class natural : public textbox {
	public:
		natural(nana::window wd, string tipstring = "") : textbox(wd) {
			multi_lines(false);
			set_accept(acceptNatural);
			if(tipstring != "") tip_string(tipstring);
		}
	};
	class integer : public textbox {
	public:
		integer(nana::window wd, string tipstring = "") : textbox(wd) {
			multi_lines(false);
			set_accept(acceptInteger);
			if(tipstring != "") tip_string(tipstring);
		}
	};
}


/*
	a label that draws a line before and after centered text, like this:
	------text-----
*/
class linelabel : public label {
public:
	drawing dw{ *this };
	linelabel(window wd, string_view text, color line_color = EFcolors::darkblue) : label(wd, text)
	{
		text_align(align::center, align_v::center);
		dw.draw([this, line_color](paint::graphics& graph)
		{
			nana::size text_size = graph.text_extent_size( caption() );
			int lineHeight = size().height / 2;
			int width = size().width;
			int horizontalSpace = (size().width - text_size.width) / 2; //space before and after the text

			graph.line({0, lineHeight}, {horizontalSpace - 4, lineHeight}, line_color);
			graph.line({width - horizontalSpace + 4, lineHeight}, {width, lineHeight}, line_color);
		});
	}
};



//IDs of procedures that can be selected in the settings panel (could be constexpr with c++20)
const vector<int> procedureIDs {
	M2.id, M3.id, M4.id, M5.id, BURNING_SHIP.id, CHECKERS.id, TRIPLE_MATCHMAKER.id,
	HIGH_POWER.id, RECURSIVE_FRACTAL.id, PURE_MORPHINGS.id, M512.id
#ifndef NDEBUG
	,DEBUG_TEST.id
#endif
};

//Post transformations that can be selected in the settings panel
const vector<int> post_transformation_ids {
	0, 1, 2, 4, 5, 6 //the user can't choose 3 and 7, they're not useful and only confusing
};

//Oversampling options that can be selected in the settings panel
const vector<uint> oversamplingOptions {1, 2, 3, 4, 6, 8, 12};


class SettingsPanel : public panel<true> {
public:
	place pl{ *this };

	linelabel sizeLabel{ *this, "Size" };
	NumberInput::natural width{ *this, "width" }; label widthLabel{ *this, "width" };
	NumberInput::natural height{ *this, "height" }; label heightLabel{ *this, "height" };
	label oversamplingLabel{ *this, "Oversampling" };
	combox oversamplingAmount{ *this }; label amountLabel{ *this, "amount" };
	checkbox undersampling{ *this, "undersampling" };

	linelabel procedureLabel{ *this, "Procedure" };
	menubar procedure{ *this };
	int chosen_procedure;

	linelabel post_transformationLabel{ *this, "Post transformation" };
	menubar post_transformation{ *this };
	int chosen_post_transformation;

	linelabel behaviorLabel{ *this, "Behavior" };
	NumberInput::natural maxIters{ *this, "" }; label maxItersLabel{ *this, "Max iters" };
	NumberInput::real inflectionZoomLevel{ *this, "" }; label inflectionZoomLevelLabel{ *this, "Inflection zoom" };

	linelabel coloringLabel {*this, "Coloring"};
	NumberInput::real gradientSpeed{ *this, "Gradient speed" }; label gradientSpeedLabel{ *this, "Speed" };
	NumberInput::real gradientOffset{ *this, "Gradient offset" }; label gradientOffsetLabel{ *this, "Offset" };

	linelabel parametersLabel{ *this, "Parameters" };
	NumberInput::real zoomLevel{ *this, "Zoom level" }; label zoomLevelLabel{ *this, "Zoom level" };
	NumberInput::real rotation{ *this, "between 0 and 1" }; label rotationLabel{ *this, "Rotation" };
	NumberInput::real centerRe{ *this, "Real coordinate" }; label centerReLabel{ *this, "Re" };
	NumberInput::real centerIm{ *this, "Imaginary coordinate" }; label centerImLabel{ *this, "Im" };
	checkbox julia{ *this, "julia" };
	label juliaSeedLabel{ *this, "Julia seed" };
	NumberInput::real juliaRe{ *this, "Real coordinate" }; label juliaReLabel{ *this, "Re" };
	NumberInput::real juliaIm{ *this, "Imaginary coordinate" }; label juliaImLabel{ *this, "Im" };

	FractalCanvas* canvas{ nullptr };

	void refreshProcedureSelection(int chosen_procedure)
	{
		procedure.clear();
		procedure.push_back( getProcedureObject(chosen_procedure).name() );

		for (int id : procedureIDs) {
			procedure.at(0).append(getProcedureObject(id).name(), [this, id](menu::item_proxy& ip)
			{
				canvas->changeParameters([id](FractalParameters& P)
				{
					P.setProcedure(id);
				}, EventSource::procedure);
				
				refreshProcedureSelection(id);
			});
		}
		this->chosen_procedure = chosen_procedure;
	}

	void refreshPostTransformationSelection(int chosen_post_transformation)
	{
		post_transformation.clear();
		post_transformation.push_back( transformation_name(chosen_post_transformation) );

		for (int id: post_transformation_ids) {
			post_transformation.at(0).append(transformation_name(id), [this, id](menu::item_proxy& ip)
			{
				canvas->changeParameters([id](FractalParameters& P)
				{
					P.setPostTransformation(id);
				}, EventSource::post_transformation);
				
				refreshPostTransformationSelection(id);
			});
		}
		this->chosen_post_transformation = chosen_post_transformation;
	}

	SettingsPanel(window wd) : panel<true>(wd)
	{
		juliaSeedLabel.text_align(align::center, align_v::center);
		oversamplingLabel.text_align(align::center, align_v::center);

		procedure.bgcolor(colors::white);
		post_transformation.bgcolor(colors::white);
		refreshProcedureSelection(M2.id);
		refreshPostTransformationSelection(0);

		for (int option : oversamplingOptions) {
			oversamplingAmount.push_back( to_string(option) );
		}

		oversamplingAmount.editable(true);
		oversamplingAmount.set_accept(acceptInteger);

		// Let the enter key apply changes (needs to be set for each widget separately)
		API::enum_widgets(*this, true, [this](widget& wdg)
		{
			wdg.events().key_press([this](const arg_keyboard& arg)
			{
				if (arg.key == keyboard::enter)
					this->parametersFromFields();
			});
		});

		pl.div(R"(
			vert
			<procedurelabel weight=25 margin=2>
			< weight=25 margin=2 <weight=2><procedure> >

			<post_transformation_label weight=25 margin=2>
			< weight=25 margin=2 <weight=2><post_transformation> >

			<sizelabel weight=25 margin=2>
			<width_ weight=25 margin=2 arrange=[30%, variable]>
			<height_ weight=25 margin=2 arrange=[30%, variable]>
			<oversamplinglabel weight=25 margin=2>
			<oversampling_amount weight=25 margin=2 arrange=[70%, variable]>
			<undersampling weight=25 margin=2>

			<behaviorlabel weight=25 margin=2>
			<maxiters weight=25 margin=2 arrange=[40%, variable]>
			<inflectionzoomlevel weight=25 margin=2 arrange=[60%, variable]>

			<coloringlabel weight=25 margin=2>
			<gradientspeed weight=25 margin=2 arrange=[30%, variable]>
			<gradientoffset weight=25 margin=2 arrange=[30%, variable]>

			<parameterslabel weight=25 margin=2>
			<zoomlevel weight=25 margin=2 arrange=[50%, variable]>
			<rotation weight=25 margin=2 arrange=[50%, variable]>
			<centerre weight=25 margin=2 arrange=[15%, variable]>
			<centerim weight=25 margin=2 arrange=[15%, variable]>
			<julia weight=25 margin=2>
			<juliaseedlabel weight=25 margin=2>
			<juliare weight=25 margin=2 arrange=[15%, variable]>
			<juliaim weight=25 margin=2 arrange=[15%, variable]>
		)");
		pl["procedurelabel"] << procedureLabel;
		pl["procedure"] << procedure;

		pl["post_transformation_label"] << post_transformationLabel;
		pl["post_transformation"] << post_transformation;

		pl["sizelabel"] << sizeLabel;
		pl["width_"] << widthLabel << width;
		pl["height_"] << heightLabel << height;
		pl["oversamplinglabel"] << oversamplingLabel;
		pl["oversampling_amount"] << amountLabel << oversamplingAmount;
		pl["undersampling"] << undersampling;
		
		pl["behaviorlabel"] << behaviorLabel;
		pl["maxiters"] << maxItersLabel << maxIters;
		pl["inflectionzoomlevel"] << inflectionZoomLevelLabel << inflectionZoomLevel;

		pl["coloringlabel"] << coloringLabel;
		pl["gradientspeed"] << gradientSpeedLabel << gradientSpeed;
		pl["gradientoffset"] << gradientOffsetLabel << gradientOffset;

		pl["parameterslabel"] << parametersLabel;
		pl["zoomlevel"] << zoomLevelLabel << zoomLevel;
		pl["rotation"] << rotationLabel << rotation;
		pl["centerre"] << centerReLabel << centerRe;
		pl["centerim"] << centerImLabel << centerIm;
		pl["julia"] << julia;
		pl["juliaseedlabel"] << juliaSeedLabel;
		pl["juliare"] << juliaReLabel << juliaRe;
		pl["juliaim"] << juliaImLabel << juliaIm;

		//pl.collocate();

		size(nana::size(1, 1)); //I have no idea why this is required. Without setting the size at least once, the widgets report their height as 0.
	}

	void setParameters(const FractalParameters& P) {
		width.caption(to_string(P.get_target_width()));
		height.caption(to_string(P.get_target_height()));

		int show_amount = P.get_bitmap_zoom() > 1 ? P.get_bitmap_zoom() : P.get_oversampling();
		if (
			P.get_bitmap_zoom() > 1 && undersampling.checked() == false
			|| (
				P.get_bitmap_zoom() == 1
				&& P.get_oversampling() > 1 && undersampling.checked()
			)
		) {
			show_amount = -show_amount;
		}

		//would be simpler if vector had a indexOf(elt) method
		int oversamplingIndex = ([&]()
		{
			for (int i=0; i<oversamplingOptions.size(); i++) {
				if (show_amount == oversamplingOptions[i]) {
					return i;
				}
			}
			return -1;
		})();

		if (oversamplingIndex != -1)
			oversamplingAmount.option(oversamplingIndex);
		else
			oversamplingAmount.caption(to_string(show_amount));

		//converter that shows many digits. to_string always gives 6 digits
		auto format_double = [](double d, uint precision = 20) -> string
		{
			stringstream ss;
			ss << setprecision(precision) << fixed << d;
			return ss.str();
		};

		refreshProcedureSelection(			P.get_procedure().id								);
		refreshPostTransformationSelection( P.get_post_transformation_type()					);
		maxIters.caption(					to_string(P.get_maxIters())						);
		inflectionZoomLevel.caption(			format_double(P.get_inflectionZoomLevel(), 6)	);
		gradientSpeed.caption(				format_double(P.get_gradientSpeed(), 10)			);
		gradientOffset.caption(				format_double(P.get_gradientOffset(), 10)		);
		zoomLevel.caption(					format_double(P.get_zoomLevel(), 10)				);
		rotation.caption(					format_double(P.get_rotation_angle(), 6)			);
		centerRe.caption(					format_double(real(P.get_center()))				);
		centerIm.caption(					format_double(imag(P.get_center()))				);
		julia.check(							P.get_julia()									);
		juliaRe.caption(						format_double(real(P.get_juliaSeed()))			);
		juliaIm.caption(						format_double(imag(P.get_juliaSeed()))			);
	}

	void parametersFromFields() {
		int i = -1;
		try {
			i=1;  int width_ = width.to_int(); if(width_ <= 0) throw invalid_argument("");
			i=2;  int height_ = height.to_int(); if(height_ <= 0) throw invalid_argument("");
			i=3;  int oversampling_ = stoi(oversamplingAmount.caption()); if(oversampling_ == 0) throw invalid_argument("");
			i=4;  int maxIters_ = maxIters.to_int(); if(maxIters_ <= 0) throw invalid_argument("");
			i=5;  double inflectionZoomLevel_  = inflectionZoomLevel.to_double();
			i=6;  double_c center_ = centerRe.to_double() + centerIm.to_double() * I;
			i=7;  double zoomLevel_ = zoomLevel.to_double();
			//1100100 is arbitrary. I want to allow values outside of the range 0 to 1 but it must not become too crazy, otherwise FractalParameters::normalize_angle fails.
			i=8;  double rotation_ = rotation.to_double(); if(rotation_ > 1100100 || rotation_ < -1100100) throw invalid_argument("");
			i=9;  double_c juliaSeed_ = juliaRe.to_double() + juliaIm.to_double() * I;
			i=10; bool julia_ = julia.checked();
			i=11; double gradientSpeed_ = gradientSpeed.to_double();
			i=12; double gradientOffset_ = gradientOffset.to_double();
			i=13; bool undersampling_ = undersampling.checked();

			// This allows to set undersampling by choosing negative oversampling too.
			if (undersampling_)
				oversampling_ = -oversampling_;

			uint bitmap_zoom_;
			if (oversampling_ > 0) {
				bitmap_zoom_ = 1;
				oversampling_ = oversampling_;
			}
			else {
				bitmap_zoom_ = -oversampling_;
				oversampling_ = 1;
			}
			
			int source_id = ([=]()
			{
				const FractalParameters& P = canvas->P();
				int source_id = EventSource::default_;
				uint changeCount = 0;

				if (P.get_target_width() != width_ || P.get_target_height() != height_ || P.get_oversampling() != oversampling_ || P.get_bitmap_zoom() != bitmap_zoom_) {
					source_id = EventSource::size;
					changeCount++;
				}
				if (P.get_maxIters() != maxIters_) {
					source_id = EventSource::iters;
					changeCount++;
				}
				if (P.get_inflectionZoomLevel() != inflectionZoomLevel_) {
					source_id = EventSource::inflectionZoom;
					changeCount++;
				}
				if (P.get_center() != center_) {
					source_id = EventSource::location;
					changeCount++;
				}
				if (P.get_zoomLevel() != zoomLevel_) {
					source_id = EventSource::zoomLevel;
					changeCount++;
				}
				if (P.get_rotation_angle() != rotation_) {
					source_id = EventSource::rotation;
					changeCount++;
				}
				if (P.get_juliaSeed() != juliaSeed_) {
					source_id = EventSource::juliaSeed;
					changeCount++;
				}
				if (P.get_julia() != julia_) {
					source_id = EventSource::julia;
					changeCount++;
				}
				if (P.get_gradientSpeed() != gradientSpeed_) {
					source_id = EventSource::gradientSpeed;
					changeCount++;
				}
				if (P.get_gradientOffset() != gradientOffset_) {
					source_id = EventSource::gradientOffset;
					changeCount++;
				}

				if (changeCount > 1)
					source_id = EventSource::default_;

				return source_id;
			})();

			canvas->changeParameters([=](FractalParameters& P)
			{
				P.setProcedure(chosen_procedure);
				P.resize(width_, height_, oversampling_, bitmap_zoom_);
				P.setMaxIters(maxIters_);
				P.setInflectionZoomLevel(inflectionZoomLevel_);
				P.setCenterAndZoomRelative(center_, zoomLevel_);
				P.setRotation(rotation_);
				P.setJuliaSeed(juliaSeed_);
				P.setJulia(julia_);
				P.setGradientOffset( P.normalize_gradientOffset(gradientOffset_) );
				P.setGradientSpeed(gradientSpeed_);
			}, source_id);

			
		}
		//catch errors in the fields, such as invalid numbers like 3..141a.
		catch (...) {
			cout << "caught error" << endl;
			string error_message = ([i](){
				switch(i) {
					case 1:  return "invalid width";
					case 2:  return "invalid height";
					case 3:  return "invalid oversampling";
					case 4:  return "invalid maxIters";
					case 5:  return "invalid inflectionZoomLevel";
					case 6:  return "invalid center";
					case 7:  return "invalid zoomLevel";
					case 8:  return "invalid rotation";
					case 9:  return "invalid juliaSeed";
					//case 10
					case 11: return "invalid gradientSpeed";
					case 12: return "invalid gradientOffset";
				}
				assert(false); return "";
			})();

			cout << error_message << endl;

			msgbox mb(*this, "Error", msgbox::ok);
			mb.icon(mb.icon_error);
			mb << error_message;
			mb.show();
		}
	}
};



class EFHistory : public listbox {
public:
	struct HistoryItem {
		FractalParameters* P;
		string name;
		uint index;
		bool is_new;
	};

	unordered_map<uint, FractalParameters> parameters; //maps an index to parameters
	FractalCanvas* canvas;
	uint lastIndex{ 0 };

	EFHistory(window wd, FractalCanvas* canvas_)
	: listbox(wd)
	, canvas(canvas_)
	{
		//the column names
		append_header("n");      
		append_header("action");

		this->sortable(false); //the user can't sort

		this->sort_col(0, true);

		// This appears to leave 55 - 45 = 10 pixels of unused space, but when I try to use those pixels it I get a horizontal scrollbar.
		events().resized([this](const arg_resized& arg) 
		{
			column_at(0).width(45);
			column_at(1).width(arg.width - 55);
		});

		events().selected([this](const arg_listbox& arg)
		{
			if (arg.item.selected()) { //because this event also occurs when items are deselected
				index_pair ip = arg.item.pos();
				if( ! ip.empty() && ! ip.is_category())
				{
					deselectExcept(ip);
					HistoryItem& hitem = arg.item.value<HistoryItem>(); //This only works because all items are associated with a HistoryItem by addItem.

					//A new history item should not have its parameters set when selected.
					if (hitem.is_new) {
						hitem.is_new = false;
					}
					else {
						FractalParameters* historyParameters = hitem.P;
						canvas->changeParameters(*historyParameters, EventSource::history);
					}
				}
			}
		});

		//sort algorithm for column 0
		set_sort_compare(0, [](const std::string&, any* any_l, const std::string&, any* any_r, bool reverse)
		{
			HistoryItem* il = any_cast<HistoryItem>(any_l);
			HistoryItem* ir = any_cast<HistoryItem>(any_r);
			return (reverse ? il->index > ir->index : il->index < ir->index);
		});
	}

	void deselectExcept(index_pair ip) {
		for (index_pair ip_ : selected()) {
			if (ip_ != ip)
				at(0).at(ip_.item).select(false);
		}
	}

	// adds a history item with a higher index than the last one
	void addItem(const FractalParameters& P, string name) {
		lastIndex++;
		parameters[lastIndex] = P;
		
		HistoryItem item;
		item.P = &parameters[lastIndex];
		item.name = name;
		item.index = lastIndex;
		item.is_new = true;

		// I find this notation confusing.
		// This creates a new row with to_string(lastIndex) in the 0th column, the string name in the 1st column, and associates the HistoryItem item with that row.
		at(0)
			.append(to_string(lastIndex))
			.text(1, name)
			.value(item);

		unsort();
		sort_col(0, true);

		//select new item. This automatically deselects all other items through the "selected" event handler
		at(0).back().select(true);
	}
};
	
enum class TabKind {
	Fractal
	,Settings
};

//
// This base class is a preparation for multiple types of panels. I only use FractalPanels at this time.
//
class EFPanelBase : public panel<false> {
public:
	EFPanelBase(window wd) : panel<false>(wd) {}
	virtual TabKind kind() = 0;
	virtual ~EFPanelBase() {}
};

//dontdo: instead of these being constants, use slider.maximum() (or whatever the function is called) every time the value is needed so that at least coarse maximum can be changed by the user (fine and offset are not important to be user definable).
constexpr uint COARSE_MAXIMUM = 150;
constexpr uint FINE_MAXIMUM = 1000;
constexpr uint OFFSET_MAXIMUM = 1000;

class FractalPanel : public EFPanelBase {
public:
	TabKind kind() { return TabKind::Fractal; }

	// FractalPanel
	// contains sidebar
	// contains settings.visible
	// contains content

	//dontdo: c++20 use this when constexpr string is available to get rid of some hardcoded numbers in the nana div text
	//static constexpr uint sidebar_width = 180;
	static constexpr uint scrollbarWeight = 20; //this value is still hardcoded in some other places
	//The reason using these values in div text doesn't work is that place::modify requires a const char*, for example:
	//pl.modify("sidebar", "weight=180");
	//The part "weight=180" needs to be const char*. It's not possible to construct a string there based on a value. I hope constexpr string will solve that problem. I'm not 100% sure.

	class Sidebar : public panel<true> {
	public:
		place pl{ *this };

		button apply{ *this, "Apply" };
		button cancel{ *this, "Cancel" };
		ScrollPanel<SettingsPanel> settings{ *this };

		label historyLabel{ *this, "History" };
		EFHistory history;

		Sidebar(window wd, FractalCanvas* canvas)
		: panel<true>(wd)
		, history(*this, canvas)
		{
			settings.content.canvas = canvas; //This needs to be done here because the settings panel is constructed by the constructor of ScrollPanel<SettingsPanel> which doesn't accept extra parameters.

			// The fixed width/weigth of 180 here prevents a crash when the sidepanel is hidden by using showSidepanel. Without fixed with, upon hiding the sidepanel, the elements are shrunk to a width of 0 pixels which works fine with most widgets but not all, apparently. It may be a bug.
			//
			// The max weight/height of the settings is based on its initial measured height. I assume the height doesn't change. It's always the same list of labels and buttons. The + 10 makes it look a little nicer (adds some vertical padding).
			pl.div(R"(
				<
					weight=180
					<
						vert
						<cancel_apply weight=20>
						<settings weight=70% max=)" + to_string(settings.contentHeight() + 10) + R"(px>
						<historylabel weight=20>
						<history>
					>
				>
			)");
			pl["cancel_apply"] << cancel << apply;
			pl["settings"] << settings.visible;
			pl["historylabel"] << historyLabel;
			pl["history"] << history;
			//pl.collocate(); //doesn't appear to be necessary, why?

			historyLabel.bgcolor(EFcolors::darkblue);
			historyLabel.fgcolor(EFcolors::nearwhite);
			historyLabel.text_align(align::center, align_v::center);
		}
	};

	place pl{ *this };

	Sidebar sidebar;
	bool showing_sidepanel;

	slider coarse{ *this }; label coarselabel{ *this, "coarse" };
	slider fine{ *this }; label finelabel{ *this, "fine" };
	slider offset{ *this }; label offsetlabel{ *this, "offset" };

	scroll<false> hscrollbar{ *this, rectangle() };
	scroll<true> vscrollbar{ *this, rectangle() };
	//offset for drawing, depending on the scrollbar position. The scrollbar change events change these offset values.
	int offsetX = 0;
	int offsetY = 0;

	bool fileAssociation = false;
	bool fileModified = false;
	string filePath{ "" };
	string filename{ "" };

	//This panel has no graphics and contains no widgets so it's not visisble. At the location of this panel, the fractal is drawn by the main_form tabpanel_dw's draw function. The panel is used only for mouse_move events and to calculate the size of the visible part of the fractal.
	panel<false> fractal{ this->handle() };
	shared_ptr<NanaBitmapManager> bitmapManager;
	FractalCanvas canvas; //canvas is intentionally the last class member, so that the destructor of FractalPanel destroys it first. ~FractalCanvas will wait until all threads using the canvas are finished, and those threads may also use other members of this class.

	~FractalPanel() {
		cout << "FractalPanel with canvas " << &canvas << " is being deleted" << endl;
	}

	FractalPanel(window wd, uint number_of_threads, bool show_sidepanel)
		: EFPanelBase(wd)
		, bitmapManager(make_shared<NanaBitmapManager>())
		, canvas(number_of_threads, static_pointer_cast<BitmapManager>(bitmapManager), {theOnlyNanaGUI})
		, sidebar(*this, &canvas)
		, showing_sidepanel(show_sidepanel)
	{
		// Scrollbar weight is in pixels, ignoring DPI, just to make it easier to calculate if they're necessary for the fractal size (which is a bit complicated because the precense of a scrollbar costs space in itself).
		//The part called fractal is just to reserve space. It contains no widgets. It's where the fractal is shown by the main window.
		pl.div(R"(
			<sidebar weight=180>
			<weight=1px>
			<
				vert
				<sliders weight=20 arrange=[50,variable,repeated]>
				< <fractal> <vscrollbar weight=0px> >
				<hscrollbar weight=0px>
			>
		)");

		pl["sidebar"] << sidebar;
		pl["sliders"] << coarselabel << coarse << finelabel << fine << offsetlabel << offset;
		pl["fractal"] << fractal;
		pl["vscrollbar"] << vscrollbar;
		pl["hscrollbar"] << hscrollbar;
		vscrollbar.hide();
		hscrollbar.hide();
		pl.collocate();

		hscrollbar.step(40);
		hscrollbar.events().value_changed([&](const arg_scroll& arg)
		{
			offsetX = hscrollbar.value();
		});
		vscrollbar.step(40);
		vscrollbar.events().value_changed([&](const arg_scroll& arg)
		{
			offsetY = vscrollbar.value();
		});

		//The resizing event takes place before the contents of the window are drawn. The resized event takes place afterwards.
		events().resized([this](const arg_resized& arg)
		{
			recalculateScrollbars();
		});


		//Sidebar events
		sidebar.apply.events().click([this](const arg_click& arg)
		{
			sidebar.settings.content.parametersFromFields();
		});
		sidebar.cancel.events().click([this](const arg_click& arg)
		{
			sidebar.settings.content.setParameters(canvas.P());
		});

		coarselabel.text_align(align::center);
		finelabel.text_align(align::center);
		offsetlabel.text_align(align::center);

		coarse.maximum(COARSE_MAXIMUM);
		fine.maximum(FINE_MAXIMUM);
		offset.maximum(OFFSET_MAXIMUM);

		//Slider events
		//
		// I use this to control when history items are made. I don't want there to be tens of history items per second when the user moves the gradient sliders around. A history item should only be made when the mouse button is released.
		//
		auto onSpeedMouseUp = [this](const arg_mouse& arg)
		{
			canvas.parametersChangedEvent(EventSource::gradientSpeed);
		};
		auto onOffsetMouseUp = [this](const arg_mouse& arg)
		{
			canvas.parametersChangedEvent(EventSource::gradientOffset);
		};


		coarse.events().value_changed([&, this](const arg_slider& arg)
		{
			int posCoarse = arg.widget.value() - 50; //actual range is 0 - COARSE_MAXIMUM, mapped to -50 - (COARSE_MAXIMUM - 50) here

			canvas.changeParameters([=](FractalParameters& P)
			{
				double currentSpeed = P.get_gradientSpeed();
				double newSpeed = posCoarse + (currentSpeed - (int)currentSpeed);
				P.setGradientSpeed(newSpeed);
			}, EventSource::gradientSpeedSliding, false);
		});

		coarse.events().mouse_up(onSpeedMouseUp);


		fine.events().value_changed([&, this](const arg_slider& arg)
		{	
			int posFine = arg.widget.value();

			canvas.changeParameters([=](FractalParameters& P)
			{
				double currentSpeed = P.get_gradientSpeed();
				double newSpeed = ([=]() -> double
				{
					if ((int)(currentSpeed) < 0)
						return (int)(currentSpeed - 1) + (1 - (1.0/(FINE_MAXIMUM + 1))*(double)posFine);
					else
						return (int)(currentSpeed) + (1.0/(FINE_MAXIMUM + 1))*(double)posFine;
				})();

				P.setGradientSpeed(newSpeed);
			}, EventSource::gradientSpeedSliding, false);
		});

		fine.events().mouse_up(onSpeedMouseUp);


		offset.events().value_changed([&, this](const arg_slider& arg)
		{	
			double newGradientOffset = (1.0/OFFSET_MAXIMUM) * arg.widget.value();

			canvas.changeParameters([=](FractalParameters& P)
			{
				P.setGradientOffset(newGradientOffset);
			}, EventSource::gradientOffsetSliding, false);
		});

		offset.events().mouse_up(onOffsetMouseUp);


		//add/remove inflection
		fractal.events().mouse_up([this](const arg_mouse& arg)
		{
			const FractalParameters& P = canvas.P();

			const uint& oversampling = P.get_oversampling();
			const uint& bitmap_zoom = P.get_bitmap_zoom();

			//location of the click in the FractalCanvas
			int xPos = (arg.pos.x + offsetX) * oversampling / bitmap_zoom;
			int yPos = (arg.pos.y + offsetY) * oversampling / bitmap_zoom;

			if ( ! (xPos < 0 || xPos > P.width_canvas() || yPos < 0 || yPos > P.height_canvas())) {
				if (arg.button == mouse::left_button)
				{
					canvas.changeParameters([=](FractalParameters& P)
					{
						P.addInflection(xPos, yPos);
						P.printInflections();
					}, EventSource::addInflection);
				}
				else if (arg.button == mouse::right_button)
				{
					canvas.changeParameters([](FractalParameters& P)
					{
						P.removeInflection();
						P.printInflections();
					}, EventSource::removeInflection);
				}
			}
		});
	}

	void associateFile(string path, string name)
	{
		fileAssociation = true;
		fileModified = false;
		filePath = path;
		filename = name;
	}

	void showSidepanel(bool show) {
		if (show) {
			pl.modify("sidebar", "weight=180");
			showing_sidepanel = true;
		}
		else {
			pl.modify("sidebar", "weight=0");
			showing_sidepanel = false;
		}
		pl.collocate();
	}

	ResizeResult changeParameters(const FractalParameters& parameters, int source_id = EventSource::fractalPanel)
	{
		ResizeResult res = canvas.changeParameters(parameters, source_id);

		if (res.resultType == ResizeResultType::Success)
		{
			updateControls();
		}
		return res;
	}

	/*
		update the fields in the settings panel and the sliders after a change of parameters
	*/
	void updateControls(bool include_color_sliders = true) {
		sidebar.settings.content.setParameters(canvas.P());
		
		if (include_color_sliders)
		{
			double gradientSpeed = canvas.P().get_gradientSpeed();
			coarse.value((int)gradientSpeed + 50); //dontdo: make this 50 a setting?

			if ((int)gradientSpeed >= 0)
				fine.value(FINE_MAXIMUM * (gradientSpeed - (int)gradientSpeed));
			else
				fine.value(-1 * FINE_MAXIMUM * (gradientSpeed - (int)gradientSpeed));

			offset.value( (int)(OFFSET_MAXIMUM * canvas.P().get_gradientOffset()) );
		}
	}

	void recalculateScrollbars()
	{
		const uint bitmapWidth = canvas.P().width_bitmap();
		const uint bitmapHeight = canvas.P().height_bitmap();

		//hscrollbar.visible() is not reliable to check if it's there. For some reason, hidden scrollbars later are reported visible by this function.
		bool has_hscrollbar = hscrollbar.size().height > 0;
		bool has_vscrollbar = vscrollbar.size().width > 0;

		uint availableHeight = fractal.size().height + (has_hscrollbar ? scrollbarWeight : 0);
		uint availableWidth = fractal.size().width + (has_vscrollbar ? scrollbarWeight : 0);

		bool requiredX = availableWidth < bitmapWidth;
		bool requiredY = availableHeight < bitmapHeight;

		//The horizontal scrollbar may be needed anyway just because the vertical scrollbar takes up space. The same goes for the vertical scrollbar.
		requiredX = availableWidth - (requiredY ? scrollbarWeight : 0) < bitmapWidth;
		requiredY = availableHeight - (requiredX ? scrollbarWeight : 0) < bitmapHeight;

		bool changed = false;
		
		if (requiredX && (has_hscrollbar == false)) {
			if(debug) cout << "changed scrollbar 1" << endl;
			pl.modify("hscrollbar", "weight=20px");
			hscrollbar.show();
			changed = true;
		}
		else if ((requiredX == false) && has_hscrollbar) {
			if(debug) cout << "changed scrollbar 2" << endl;
			pl.modify("hscrollbar", "weight=0px");
			hscrollbar.hide();
			changed = true;
		}
		if (requiredY && (has_vscrollbar == false)) {
			if(debug) cout << "changed scrollbar 3" << endl;
			pl.modify("vscrollbar", "weight=20px");
			vscrollbar.show();
			changed = true;
		}
		else if ((requiredY == false) && has_vscrollbar) {
			if(debug) cout << "changed scrollbar 4" << endl;
			pl.modify("vscrollbar", "weight=0px");
			vscrollbar.hide();
			changed = true;
		}

		if (changed)
			pl.collocate();

		//When the window is enlarged, this makes the new space used by the bitmap by adjusting the offset. Without this, the bitmap remains scrolled.
		int emptyPanelSpaceX = fractal.size().width - (bitmapWidth - offsetX);
		int emptyPanelSpaceY = fractal.size().height - (bitmapHeight - offsetY);

		if (emptyPanelSpaceX > 0) {
			offsetX = max({0, offsetX - emptyPanelSpaceX});
		}
		if (emptyPanelSpaceY > 0) {
			offsetY = max({0, offsetY - emptyPanelSpaceY});
		}

		hscrollbar.range(fractal.size().width);
		vscrollbar.range(fractal.size().height);
		hscrollbar.amount(bitmapWidth);
		vscrollbar.amount(bitmapHeight);
	}
};

//dontdo: this is unused. The intention is to make a tabpage for program settings.
class ProgramSettingsPanel : public EFPanelBase {
public:
	TabKind kind() { return TabKind::Settings; }

	place pl{ *this };
	label test{*this, "settings panel"};
	button apply{*this, "Apply"};

	ProgramSettingsPanel(window wd) : EFPanelBase(wd)
	{
		bgcolor(EFcolors::normal);
		pl.div("<x>");
		pl["x"] << test << apply;
		pl.collocate();
	}
};

class TabStatusbarLabels {
public:
	string progress;
	string time;
	string inflections;
	string zoomlevel;
	string formula;
	string resolution;
};

/*
	This is a tabbar that remembers which panels it controls. The nana tabbar has no function to get the panel of a tab.
		
	Panels and tabs shouldn't be added directly through the functions from nana. Instead, addFractalPanel and removePanel can be used. They update the panels vector and the canvas_panel_map.
*/
class EFtabbar {
public:
	uint number_of_threads; //dontdo: I store this value in multiple places. I need it here so I store it here but... not nice

	vector<shared_ptr<EFPanelBase>> panels; //index i in this vector contains the panel of the tab at index i in the tabbar
	unordered_map<FractalCanvas*, shared_ptr<EFPanelBase>> canvas_panel_map; //to answer the question: what is the panel that contains this canvas?
	vector<TabStatusbarLabels> statusbarLabels; //at index i in the vector are the labels for tab i

	tabbar<string> bar;
	int settingsTabIndex = -1; //if there is a settings tab, this is the index, otherwise it's -1.
		
	EFtabbar(window wd, uint number_of_threads)
	: number_of_threads(number_of_threads)
	, bar(wd) 
	{
		bar.toolbox(tabbar<string>::kits::close, true);
		bar.toolbox(tabbar<string>::kits::list, true);
		bar.toolbox(tabbar<string>::kits::scroll, true);
		bar.close_fly(true);

		bar.events().removed([this](const arg_tabbar_removed<string>& arg)
		{
			uint index = arg.item_pos;

			auto panel = panels[index];

			if(panel->kind() == TabKind::Settings)
			{
				settingsTabIndex = -1;
				panels.erase(panels.begin()+index);
				statusbarLabels.erase(statusbarLabels.begin()+index);
			}
			else if(panel->kind() == TabKind::Fractal)
			{
				shared_ptr<FractalPanel> fractalPanel = static_pointer_cast<FractalPanel>(panel);
				FractalCanvas* canvas = &(fractalPanel->canvas);

				panels.erase(panels.begin() + index);
				statusbarLabels.erase(statusbarLabels.begin() + index);

				//There can be active renders that have to end before resources can be cleaned up. By waiting from a different thread, the program doesn't hang while waiting.
				thread([canvas]()
				{
					if(debug) cout << "thread started to clean up resources of the tab of canvas " << canvas << endl;

					//waits for all renders to finish
					//Because only the GUI thread starts renders, after this there will be no renders going on.
					canvas->end_all_usage();

					//instruct the GUI thread to clean up resources for this canvas:
					PostMessage(main_form_hwnd, Message::CLEANUP_FRACTAL_TAB, reinterpret_cast<WPARAM>(canvas), 0);
				}).detach();
			}

			cout << "removed event vanuit EFTabbar " << endl;
		});
	}

	bool hasSettings() {
		return settingsTabIndex >= 0;
	}

	int indexOf(EFPanelBase* panel) {
		for (int i=0; i<panels.size(); i++)
			if (panels[i].get() == panel)
				return i;
		return -1;
	}
	
	// This may return -1 even if this contains the canvas, when a tab was removed but its resources not cleaned up yet
	int indexOf(FractalCanvas* canvas) {
		if (canvas_panel_map.find(canvas) == canvas_panel_map.end())
			return -1;
		else
			return indexOf(canvas_panel_map[canvas].get());
	}

	bool containsCanvas(FractalCanvas* canvas) {
		return
			canvas_panel_map.find(canvas) != canvas_panel_map.end() //c++20 has a contains function for maps
			&& indexOf(canvas) != -1;
	}

	shared_ptr<FractalPanel> addFractalPanel(string title, shared_ptr<FractalPanel> new_panel)
	{
		panels.push_back(new_panel);
		canvas_panel_map[&(new_panel->canvas)] = new_panel;
		statusbarLabels.push_back(TabStatusbarLabels());
		bar.append(title, *new_panel); // This needs to be done last because it causes a activated event on the tabbar, of which the handler expects statusbarLabels etc. to exist.
		return new_panel;
	}

	/*
	shared_ptr<ProgramSettingsPanel> addSettingsPanel(string title) {
		shared_ptr<ProgramSettingsPanel> new_panel = make_shared<ProgramSettingsPanel>(panel_container);
		bar.append(title, *new_panel);
		panels.push_back(new_panel);
		TabStatusbarLabels labels;
		labels.formula = title;
		statusbarLabels.push_back(labels);

		return new_panel;
	}
	*/

	//returns whether an association was made
	bool associateFile(string path, FractalCanvas* canvas)
	{	
		assert(containsCanvas(canvas));

		if (containsCanvas(canvas)) {
			int canvas_index = indexOf(canvas);
			if (canvas_index >= 0)
			{
				shared_ptr<EFPanelBase> panel = canvas_panel_map[canvas];

				assert(panel->kind() == TabKind::Fractal);

				if (panel->kind() == TabKind::Fractal)
				{
					size_t lastSlashIndex = path.find_last_of("/\\");
					string name = path.substr(lastSlashIndex + 1);

					shared_ptr<FractalPanel> fractalPanel = static_pointer_cast<FractalPanel>(panel);
					fractalPanel->associateFile(path, name);
					bar.text(canvas_index, name);
					return true;
				}
			}
		}
		return false;
	}
};


class EFstatusbar : public panel<true> {
public:
	place pl{ *this };

	class statusbarlabel : public label
	{ public:
		statusbarlabel(window wd) : label(wd)
		{
			this->text_align(align::center, align_v::top);
		}
	};
	//contents of the status bar
	statusbarlabel progress{ *this };
	statusbarlabel time{ *this };
	statusbarlabel inflections{ *this };
	statusbarlabel zoomlevel{ *this };
	statusbarlabel coordinates{ *this };
	statusbarlabel iterations{ *this };
	statusbarlabel formula{ *this };
	statusbarlabel resolution{ *this };

	EFstatusbar(window wd) : panel<true>(wd)
	{
		string layout = R"(
			<
					       <progress min=45>
				<weight=1> <time min=60>
				<weight=1> <inflections min=90>
				<weight=1> <zoomlevel min=115>
				<weight=1> <coordinates min=120>
				<weight=1> <iterations min=90>
				<weight=1> <formula>
				<weight=1> <resolution min=100>
			>
		)";
		pl.div(layout);
		iterations.text_align(align::left, align_v::top);
		pl["progress"] << progress;
		pl["time"] << time;
		pl["inflections"] << inflections;
		pl["zoomlevel"] << zoomlevel;
		pl["coordinates"] << coordinates;
		pl["iterations"] << iterations;
		pl["formula"] << formula;
		pl["resolution"] << resolution;
		//pl.collocate();
	}
};


void handleResizeResult(window wd, ResizeResult res)
{
	if (res.resultType == ResizeResultType::OutOfRangeError)
	{
		msgbox mb(wd, "Error", msgbox::ok);
		mb.icon(mb.icon_error);
		mb << "Changing the resolution failed: width and/or height out of range.";
		mb.show();
	}
	else if (res.resultType == ResizeResultType::MemoryError) {
		if (res.changed)
		{
			msgbox mb(wd, "Error", msgbox::ok);
			mb.icon(mb.icon_error);
			mb << "Changing the resolution failed: out of memory.";
			mb.show();
		}
		else
		{
			msgbox mb(wd, "Error", msgbox::ok);
			mb.icon(mb.icon_error);
			mb << "Changing the resolution failed: out of memory. The resolution has not changed.";
			mb.show();
		}
	}
}

void handleReadResult(window wd, ReadResult res) {
	switch (res) {
		case ReadResult::parseError:
		{
			msgbox mb(wd, "Error", msgbox::ok);
			mb.icon(mb.icon_error);
			mb << "The parameter file doesn't have the expected format. If this is a binary file (from program version 4 and earlier), try to open and resave it with ExploreFractals 5.";
			mb.show();
			break;
		}
		case ReadResult::fileError:
		{
			msgbox mb(wd, "Error", msgbox::ok);
			mb.icon(mb.icon_error);
			mb << "The parameter file cannot be openened.";
			mb.show();
			break;
		}
	}
}


//
//	Utilities to manage child windows
//
// This base class makes it possible to store pointers to all child windows in 1 vector, even if the windows are of different types.
class child_window_base {
public:
	virtual ~child_window_base() {}
	virtual void focus() = 0;
};

// The instances of this template are the actual child window classes.
template <typename formtype>
class child_window : public child_window_base {
	static_assert(std::is_base_of<form, formtype>::value, "formtype must be a subclass of form");
public:
	formtype fm;
	void focus() { fm.focus(); }
};


//
// Types used as child windows
//

class help_form : public form {
public:
	place& pl{ get_place() };
	textbox text{ *this, (
R"(Inflections / morphings

This program is made to test the effect of Julia morphings / inflections. A click adds an inflection at the clicked location in the fractal. An inflection is a transformation of the complex plane, which corresponds to how shapes in the Mandelbrot set are related to each other. Deeper shapes are Julia transformed versions of lesser deep shapes at the same zoom path.


Save inflection zoom

Sets the base zoom level to which the program resets after applying an inflection. Without using this setting, the zoomlevel goes back to 0 after creating an inflection. The zoom level is automatically corrected for the number of inflections, because an inflection halves the distance (the exponent in the magnification factor) to deeper shapes.


Coarse and fine

Both control gradient speed. Fine is just finer. The gradient speed controls the number of interpolated colors in the color gradient. Every pixel of the fractal is independently colored based on its iteration count. The color index in the gradient is calculated by (iterationcount of the pixel) mod (number of colors in the gradient).


Procedures

Checkers - A checkerboard pattern to test inflections on a tiling pattern, which can also be found in the Mandelbrot set. The circles are a crude simulation of "details" like in the Mandelbrot set.

Pure Julia morphings - starts as a completely empty plane. Add Julia morphings to transform the plane. The iteration count - which determines the color - in this procedure is the number of Julia morphings until the pixel escapes. The procedure iterates over the list of Julia morphings, instead of iterating the same formula all the time (as with the normal Mandelbrot set).

Triple matchmaker - A formula by Pauldelbrot at fractalforums.org. This fractal doesn't have a notion of escaping. The number of iterations is constant for every pixel. Therefore, changing the max iterations changes the result.


Keyboard shortcuts

CTRL + Z and CTRL + Y - move back and forward in the history
CTRL + T - create a new tab
CTRL + D - duplicate the current tab
CTRL + W - close the current tab
CTRL + TAB and CTRL + SHIFT + TAB - go to the next or previous tab


Using a default parameter file

Use the program to save a parameter file called default.efp to the directory from where you start the program. The program will use the parameters as default for new tabs.


Using commandline parameters

The program also has commandline parameters to render images and Julia morphing animations. Start the program from a commandline like this to get help:
ExploreFractals.exe --help)"
	)};

	help_form() : form( API::make_center(400, 800) )
	{
		caption("Help");

		text.line_wrapped(true);
		text.editable(false);
		text.enable_caret();
		div("x");
		pl["x"] << text;
		pl.collocate();
	}
};


class json_form : public form {
public:
	place& pl{ get_place() };

	textbox text{ *this };
	button capture{ *this, "Capture" };
	button apply{ *this, "Apply" };

	json_form() : form( API::make_center(400, 800)  )
	{
		caption("JSON");

		text.line_wrapped(true);
		text.enable_caret();
		text.set_keywords("special", false, false, { "{", "}", ":", "[", "]" });
		text.set_highlight("special", colors::blue, colors::white);
		text.scheme().mouse_wheel.lines = 8; //dontdo: this doesn't work; may be a bug in nana. Scrolling scrolls only 1 line at a time which is a little annoying.

		pl.div(R"(
			vert
			<text>
			<buttons weight=20>
		)");
		pl["text"] << text;
		pl["buttons"] << capture << apply;
		pl.collocate();
	}
};


class main_form : public form, public GUIInterface
{
public:
	//widgets
	place& pl{ get_place() };

	panel<false> tabpanel{ *this };
	place tabpanel_pl;
	panel<true> viewport{ tabpanel };
	drawing viewport_dw{ viewport };

	menubar menu_{ *this };
	EFtabbar tabs;
	EFstatusbar statusbar{ *this };

	button newTabButton{ *this, "New fractal" };
	button fitToWindowButton{ *this, "Fit to window" };
	button fitToFractalButton{ *this, "Fit to fractal" };
	button resetButton{ *this, "Reset" };
	button toggleJulia{ *this, "Toggle Julia" };
	button leftButton{ *this, "Left"};
	button rightButton{ *this, "Right" };
	button saveInflectionZoomButton { *this, "Save inflection zoom" };

	//dontdo: remove this if unnecessary. Disabling or enabling AVX can be a program setting
	#ifndef NDEBUG
	button avxtoggle { *this, "Toggle AVX" };
	#endif

	label settingsLabel{ *this, "Settings" };
	label settingsTriangle{ *this, "▲" };
	label settingsFiller{ *this, "" }; //label without text, used to center the text "Settings". To the left of the settingsLabel there's the triangle label. By placing another label of the same size to the right the text is centered correctly.

	//other data
	vector<unique_ptr<child_window_base>> childWindows;

	uint number_of_threads;
	FractalParameters defaultParameters;
	FractalCanvas* activeCanvas = nullptr;
	FractalPanel* activeFractalPanel = nullptr;

	//a buffer to generate animation frames in
	//todo
	//paint::graphics tabpanel_graph;


	main_form(FractalParameters& defaultParameters_, uint number_of_threads_)
		: form( API::make_center(1200, 800) )
		, defaultParameters(defaultParameters_)
		, tabs(*this, number_of_threads_)
		, number_of_threads(number_of_threads_)
	{
		tabpanel_pl.bind(tabpanel);
		tabpanel_pl.div("<x>");
		bgcolor(colors::black);
		viewport.bgcolor(colors::black);
		statusbar.bgcolor(colors::black); //this effectively colors only the spacings between the labels because the labels on the statusbar have a color

		viewport.size(nana::size(100, 200));

		//menubar
		int i = -1;
		menu_.push_back("&File");
		{
			i++;
			menu_.at(i).append("Load parameters (CTRL + O)", [this](menu::item_proxy& ip)
			{
				loadParameters();
			});
			menu_.at(i).append("Save parameters and image as (CTRL + S)", [this](menu::item_proxy& ip)
			{
				saveBothAs();
			});
			menu_.at(i).append("Save current file (CTRL + SHIFT + S)", [this](menu::item_proxy& ip)
			{
				saveParameters();
			});
			menu_.at(i).append("Save image", [this](menu::item_proxy& ip)
			{
				if (activeCanvas != nullptr) {
					string path = getDate() + " " + activeCanvas->P().get_procedure().name();
					if (BrowseFile(getHwnd(), FALSE, "Save PNG", "Portable Network Graphics (PNG)\0*.png\0\0", path)) {
						saveImage(activeCanvas, path);
					}
				}
			});
			menu_.at(i).append("Save parameters as", [this](menu::item_proxy& ip)
			{
				saveParametersAs();
			});
		}
		menu_.push_back("&View");
		{
			i++;
			menu_.at(i).append("Sidepanel", [&](menu::item_proxy& ip)
			{
				togglePanel();
			});
			menu_.at(i).append("View guessed pixels", [this](menu::item_proxy& ip)
			{
				if (activeCanvas != nullptr) {
					activeCanvas->enqueueBitmapRender(false, true);
				}
			});
			menu_.at(i).append("View regular colors", [this](menu::item_proxy& ip)
			{
				if (activeCanvas != nullptr) {
					activeCanvas->enqueueBitmapRender(false, false);
				}
			});
			menu_.at(i).append("JSON", [this](menu::item_proxy& ip)
			{
				spawn_child<json_form>();
			});
			//Files window
			//Program settings window
		}
		menu_.push_back("Other");
		{
			i++;
			menu_.at(i).append("Cancel render", [this](menu::item_proxy& ip)
			{
				if (activeCanvas != nullptr) {
					activeCanvas->cancelRender();
				}
			});

			menu_.at(i).append("Help", [this](menu::item_proxy& ip)
			{
				spawn_unique_child<help_form>();
			});
			
			menu_.at(i).append("About", [this](menu::item_proxy& ip)
			{
				msgbox mb(*this, "Information", msgbox::ok);
				mb.icon(mb.icon_information);
				mb << (R"(
Program version: )" + to_string(PROGRAM_VERSION) + R"(

Licence: GNU General Public License, version 3, see: https://www.gnu.org/licenses/gpl-3.0.html

Web resources for this program:
Source code and information: https://github.com/DinkydauSet/ExploreFractals
Fractalforums thread: https://fractalforums.org/other/55/explore-fractals-inflection-tool/777
)"				);
				mb.show();
			});
		}
		
		newTabButton.events().click([&, this](const arg_click& arg)
		{
			create_fractal_tab(defaultParameters, defaultParameters.get_procedure().name());
		});
		fitToFractalButton.events().click([this]()
		{
			fit_to_fractal();
		});
		fitToWindowButton.events().click([this]()
		{
			fit_to_window();
		});
		resetButton.events().click([this]()
		{
			if (activeCanvas != nullptr) {
				activeCanvas->changeParameters([this](FractalParameters& P)
				{
					P.setGradientSpeed(defaultParameters.get_gradientSpeed());
					P.setGradientOffset(defaultParameters.get_gradientOffset());
					P.setMaxIters(defaultParameters.get_maxIters());
					while (P.removeInflection());
					P.setRotation(0);
					P.setCenterAndZoomAbsolute(0, 0);
					P.setInflectionZoomLevel();
				}, EventSource::reset);
			}
		});
		toggleJulia.events().click([this]()
		{
			if (activeCanvas != nullptr) {
				activeCanvas->changeParameters([](FractalParameters& P)
				{
					P.setJulia( ! P.get_julia());
					if (P.get_julia()) {
						P.setJuliaSeed( P.map_transformations(P.get_center()) );
						P.setCenterAndZoomAbsolute(0, 0);
					}
					else {
						P.setCenterAndZoomAbsolute(P.get_juliaSeed(), 0);
					}
				}, EventSource::julia);
			}
		});
		leftButton.events().click([this]()
		{
			if (activeCanvas != nullptr) {
				activeCanvas->changeParameters([](FractalParameters& P)
				{
					P.setRotation(P.get_rotation_angle() - 0.05);
				}, EventSource::rotation);
			}
		});
		rightButton.events().click([this]()
		{
			if (activeCanvas != nullptr) {
				activeCanvas->changeParameters([](FractalParameters& P)
				{
					P.setRotation(P.get_rotation_angle() + 0.05);
				}, EventSource::rotation);
			}
		});

		saveInflectionZoomButton.events().click([this]
		{
			if (activeCanvas != nullptr) {
				activeCanvas->changeParameters([](FractalParameters& P)
				{
					P.setInflectionZoomLevel();
				}, EventSource::inflectionZoom);
			}
		});

		#ifndef NDEBUG
		avxtoggle.events().click([this]
		{
			using_avx = ! using_avx;
		});
		#endif

		settingsLabel.bgcolor(EFcolors::darkblue);
		settingsLabel.fgcolor(EFcolors::nearwhite);
		settingsLabel.text_align(align::center, align_v::center);
		settingsTriangle.bgcolor(EFcolors::darkblue);
		settingsTriangle.fgcolor(EFcolors::nearwhite);
		settingsTriangle.text_align(align::center, align_v::center);
		settingsFiller.bgcolor(EFcolors::darkblue);

		//This function draws on the viewport panel directly. FractalPanels on top of it have no widgets at the location of the viewport, so the FractalPanels are transparent there, which makes it look like the viewport belongs to the tab, but there is only one panel (viewport) in use where fractals are drawn. This saves memory compared to having a panel for each tab.
		//Event handlers are used to keep the position of the viewport consistent with the location of the fractal part of the current tab.
		viewport_dw.draw([this](paint::graphics& graph)
		{
			FractalCanvas* canvas = activeCanvas;
			if (canvas != nullptr)
			{
				assert(activeFractalPanel != nullptr);

				uint width = canvas->P().width_bitmap();
				uint height = canvas->P().height_bitmap();

				//where to draw
				//The location to draw is the rectangle between the sidebar, sliders and scrollbars. Note that the viewport can be larger than the fractal bitmap, in which case not the whole viewport is used. This is handled correctly by bitblt.
				nana::size viewport_size = activeFractalPanel->fractal.size();
				rectangle draw_area(nana::point(0,0), viewport_size);

				//offset by scrollbars
				int offsetX = activeFractalPanel->offsetX;
				int offsetY = activeFractalPanel->offsetY;

				//the bitmap containing the fractal's colors
				paint::graphics& g = activeFractalPanel->bitmapManager->graph;

				//copy part (possibly all) of the bitmap to the screen at the right position
				graph.bitblt(draw_area, g, nana::point(offsetX, offsetY));
			}
		});

		//This graphics object is used to measure text width in pixels. It's not a nice solution but it works. What it returns is the size of the text if it were rendered on this graphic, which means that graphic already has to be big enough for the text to fit, otherwise the returned size is too low. I set the width to 1000 pixels because that ought to be enough for every button caption. (I tried a height of 0 but that doesn't work.)
		paint::graphics measure(nana::size(1000,1));
		uint newTabButtonWidth = measure.text_extent_size(newTabButton.caption()).width;
		uint fitToWindowButtonWidth = measure.text_extent_size(fitToWindowButton.caption()).width;
		uint fitToFractalButtonWidth = measure.text_extent_size(fitToFractalButton.caption()).width;
		uint resetButtonWidth = measure.text_extent_size(resetButton.caption()).width;
		uint toggleJuliaWidth = measure.text_extent_size(toggleJulia.caption()).width;
		uint leftButtonWidth = measure.text_extent_size(leftButton.caption()).width;
		uint rightButtonWidth = measure.text_extent_size(rightButton.caption()).width;
		uint saveInflectionZoomButtonWidth = measure.text_extent_size(saveInflectionZoomButton.caption()).width;

		constexpr uint extra = 6;
		string main_form_layout = R"(
			vert
			<
				weight=20
				<menu_ weight=180>
				<
					<newTabButton min=)"+to_string(newTabButtonWidth + extra)+R"(px>
					<fitToWindowButton min=)"+to_string(fitToWindowButtonWidth + extra)+R"(px>
					<fitToFractalButton min=)"+to_string(fitToFractalButtonWidth + extra)+R"(px>
					<resetButton min=)"+to_string(resetButtonWidth + extra)+R"(px>
					<toggleJulia min=)"+to_string(toggleJuliaWidth + extra)+R"(px>
					<leftButton min=)"+to_string(leftButtonWidth + extra)+R"(px>
					<rightButton min=)"+to_string(rightButtonWidth + extra)+R"(px>
					<saveInflectionZoomButton min=)"+to_string(saveInflectionZoomButtonWidth + extra)+R"(px>)" +
					#ifndef NDEBUG
						"<avxtoggle>"
					#else
						""
					#endif
					+ R"(
				>
			>
			<
				weight=20
				<settingslabel weight=180 arrange=[20, 140, 20]>
				<tabs>
			>
			<tabpanel>
			<weight=1px>
			<statusbar weight=17>
			<weight=1px>
		)";
		pl.div(main_form_layout);

		pl["menu_"] << menu_;

		pl["newTabButton"] << newTabButton;
		pl["fitToWindowButton"] << fitToWindowButton;
		pl["fitToFractalButton"] << fitToFractalButton;
		pl["resetButton"] << resetButton;
		pl["toggleJulia"] << toggleJulia;
		pl["leftButton"] << leftButton;
		pl["rightButton"] << rightButton;
		pl["saveInflectionZoomButton"] << saveInflectionZoomButton;
		
		#ifndef NDEBUG
		pl["avxtoggle"] << avxtoggle;
		#endif

		pl["settingslabel"] << settingsTriangle << settingsLabel << settingsFiller;
		pl["tabs"] << tabs.bar;
		pl["statusbar"] << statusbar;
		pl["tabpanel"] << tabpanel;
		pl.collocate();

		settingsTriangle.events().click([this](const arg_click& arg)
		{
			if(arg.mouse_args->button == nana::mouse::left_button) {
				togglePanel();
			}
		});
		settingsLabel.events().click([this](const arg_click& arg)
		{
			if(arg.mouse_args->button == nana::mouse::left_button) {
				togglePanel();
			}
		});

		tabs.bar.events().activated([&, this](const arg_tabbar<string>& arg)
		{	
			for (uint i = 0; i < tabs.bar.length(); i++) {
				if (i != arg.item_pos) {
					tabs.bar.tab_bgcolor(i, EFcolors::inactivetab);
					tabs.bar.tab_fgcolor(i, EFcolors::inactivetabtext);
				}
				else {
					tabs.bar.tab_bgcolor(arg.item_pos, EFcolors::activetab);
					tabs.bar.tab_fgcolor(arg.item_pos, EFcolors::activetabtext);
				}
			}

			TabStatusbarLabels& labels = tabs.statusbarLabels[arg.item_pos];
			setTabStatusbarLabels(labels);
			
			auto pane = tabs.panels[arg.item_pos];

			if (pane->kind() == TabKind::Fractal) {
				auto fractalpanel = static_pointer_cast<FractalPanel>(pane);

				activeFractalPanel = fractalpanel.get();
				activeCanvas = &(fractalpanel->canvas);

				if ( ! fractalpanel->showing_sidepanel)	settingsTriangle.caption("▼");
				else										settingsTriangle.caption("▲");

				caption(windowTitle(fractalpanel.get()));
				position_viewport();
				viewport_dw.update();
			}
			else {
				activeCanvas = nullptr;
				activeFractalPanel = nullptr;
				caption("ExploreFractals");
			}
		});
		
		//The removed event is also set in the constructor of the EFtabbar

		tabs.bar.events().removed([this](const arg_tabbar<string>& arg)
		{
			//This assert fails when there is only one tab and that tab is removed, which indicates that the removed event occurs before the actual removal:
			//assert(bar.length() == 0);
			//That means the last tab is removed when tabs.bar.length() == 1.

			if (tabs.bar.length() == 1)
			{
				activeCanvas = nullptr;
				activeFractalPanel = nullptr;
			}
			viewport_dw.update();
		});
	}

	void position_viewport()
	{
		if (activeFractalPanel != nullptr)
		{
			FractalPanel* fp = activeFractalPanel;
			if (viewport.size() != fp->fractal.size()) {
				viewport.size(fp->fractal.size());
			}
			if (viewport.pos() != fp->fractal.pos()) {
				viewport.move(fp->fractal.pos());
			}
		}
	}

	//
	//	Functions for child windows
	//

	// creates a childwindow of the specified type
	//	The reason for PostMessage is this: I want the window to be removed from the vector of childWindows when it's destroyed. Doing so during the handling of the destroyed event causes a deadlock. I don't know why; nana must use some mutex during the event that is required by one of the destructors. The solution is to postpone cleanup of the resources until after the event has been handled, by sending a new message.
	//
	template <typename formtype>
	void spawn_child()
	{		
		// I use emplace_back instead of push_back because emplace_back returns the inserted element.
		auto& inserted = childWindows.emplace_back( make_unique<child_window<formtype>>() );
		child_window_base* basepointer = inserted.get();
		child_window<formtype>* child = static_cast<child_window<formtype>*>(basepointer);

		child->fm.events().destroy([basepointer]()
		{
			if(debug) cout << "child window is being closed" << endl;

			PostMessage(main_form_hwnd, Message::CLEANUP_CHILD_WINDOW, reinterpret_cast<WPARAM>(basepointer), 0);

			if(debug) cout << "child window closed" << endl;
		});

		//type specific things
		if constexpr( is_same<formtype, json_form>::value )
		{
			json_form& json = child->fm;

			json.capture.events().click([this, &json](const arg_click& arg)
			{
				FractalCanvas* canvas = activeCanvas;
				if (canvas != nullptr) {
					json.text.caption( canvas->P().toJson() );
				}
				else {
					msgbox mb(json, "Error", msgbox::ok);
					mb.icon(mb.icon_error);
					mb << "No fractal tab open";
					mb.show();
				}
			});

			json.apply.events().click([this, &json](const arg_click& arg)
			{
				FractalCanvas* canvas = activeCanvas;
				if (canvas != nullptr) {
					//try to apply the changes to a copy first
					FractalParameters P = canvas->P();
					string textContent = json.text.caption();
					bool success = P.fromJson(textContent);

					if (success) {
						canvas->changeParameters(P, EventSource::JSON);
					}
					else {
						msgbox mb(json, "Error", msgbox::ok);
						mb.icon(mb.icon_error);
						mb << "Invalid JSON";
						mb.show();
					}
				}
				else {
					msgbox mb(json, "Error", msgbox::ok);
					mb.icon(mb.icon_error);
					mb << "No fractal tab open";
					mb.show();
				}
			});
		}

		child->fm.show();
	}

	// Creates a childwindow of the specified type if none exist yet, otherwise focuses the existing one
	template <typename formtype>
	void spawn_unique_child()
	{
		for (int i=0; i<childWindows.size(); i++)
		{
			child_window<formtype>* child = dynamic_cast<child_window<formtype>*>(childWindows[i].get());
			if (child != nullptr)
			{
				// This type of window already exists. Focus it:	
				child->focus();
				return;
			}
		}

		//This type of windows does not exist yet. Create one:
		spawn_child<formtype>();
	}



	void togglePanel(){
		if (activeCanvas != nullptr) {
			assert(tabs.containsCanvas(activeCanvas));
			
			auto active_panel = static_pointer_cast<FractalPanel>(tabs.canvas_panel_map[activeCanvas]);
			bool currently_showing = active_panel->showing_sidepanel;
			active_panel->showSidepanel( ! currently_showing); //toggle

			if (currently_showing)	settingsTriangle.caption("▼");
			else						settingsTriangle.caption("▲");

			active_panel->recalculateScrollbars();
			//position_viewport(); //unnecessary because togglePanel changes the fractal panel size which also causes position_viewport to be called (right?)
			viewport_dw.update();
		}
	};

	string windowTitle(FractalPanel* fractalpanel) {
		if(fractalpanel->fileAssociation)
			return string((fractalpanel->fileModified ? "*" : "")) + fractalpanel->filePath + " - ExploreFractals";
		else
			return "ExploreFractals";
	}

	shared_ptr<FractalPanel> create_fractal_tab(const FractalParameters& parameters, string title, bool showing_sidepanel = true)
	{
		shared_ptr<FractalPanel> fractalpanel = make_shared<FractalPanel>(tabpanel, number_of_threads, true);

		tabpanel_pl["x"].fasten(*fractalpanel);
		tabpanel_pl.collocate();

		//I want to change the parameters here, so that recalculateScrollbars calculates the scrollbars properly, but I don't want the parametersChanged event here yet because the fractalpanel has not yet been added to the tabbar.
		ResizeResult res = fractalpanel->changeParameters(parameters, EventSource::noEvents);
		handleResizeResult(*this, res);

		fractalpanel->showSidepanel(showing_sidepanel);
		fractalpanel->recalculateScrollbars();

		tabs.addFractalPanel(title, fractalpanel); //Doing this step here and not earlier makes sure scrollbars, sliders etc. have been completely rendered to the offscreen buffer before the tab is added, which prevents annoying flickering

		//causes the render to start
		if (fractalpanel->canvas.P().modified())
			parametersChanged(fractalpanel->canvas.voidPtr(), EventSource::fractalPanel);

		fractalpanel->fractal.events().mouse_move([this, fractalpanel = fractalpanel.get()](const arg_mouse& arg)
		{
			const uint oversampling = fractalpanel->canvas.P().get_oversampling();
			const uint width = fractalpanel->canvas.P().width_canvas();
			const uint height = fractalpanel->canvas.P().height_canvas();
			const uint bitmap_zoom = fractalpanel->canvas.P().get_bitmap_zoom();

			//location of the cursor in the FractalCanvas
			int xPos = (arg.pos.x + fractalpanel->offsetX) * oversampling / bitmap_zoom;
			int yPos = (arg.pos.y + fractalpanel->offsetY) * oversampling / bitmap_zoom;

			if (xPos < 0 || xPos >= width || yPos < 0 || yPos >= height) {
				return;
			}

			double_c coordinate = fractalpanel->canvas.P().map_with_transformations(xPos, yPos);
			uint iterationCount = fractalpanel->canvas.getIterData(xPos, yPos).iterationCount;
				
			updateStatusbarCursorinfo(coordinate, iterationCount);
		});

		//The fractalpanel also registers handlers for these events that update offsetX and offsetY. Those handlers are executed first because they are registered first.
		fractalpanel->hscrollbar.events().value_changed([this](const arg_scroll& arg)
		{
			viewport_dw.update();
		});
		fractalpanel->vscrollbar.events().value_changed([this](const arg_scroll& arg)
		{
			viewport_dw.update();
		});

		fractalpanel->fractal.events().resized([this, fp=fractalpanel.get()](const arg_resized& arg)
		{
			position_viewport();
		});

		fractalpanel->fractal.events().mouse_wheel([this, fp=fractalpanel.get()](const arg_wheel& arg)
		{
			FractalCanvas& canvas = fp->canvas;
			const uint oversampling = canvas.P().get_oversampling();
			const uint bitmapWidth = canvas.P().width_bitmap();
			const uint bitmapHeight = canvas.P().height_bitmap();
			const uint bitmap_zoom = canvas.P().get_bitmap_zoom();

			//location of the cursor in the bitmap
			int xPos = arg.pos.x + fp->offsetX;
			int yPos = arg.pos.y + fp->offsetY;

			if (
				xPos < 0		|| xPos > bitmapWidth
				|| yPos < 0 || yPos > bitmapHeight
			) {
				return;
			}

			canvas.cancelRender();

			bool zoomIn = arg.upwards;
			{
			
				//generate preview of the zoomed in fractal
				paint::graphics viewport_copy;
				API::window_graphics(viewport, viewport_copy);
				paint::graphics& bitmap_graphics = fp->bitmapManager->graph;
				
				if (zoomIn) {
					rectangle fromPart(
						nana::point(xPos - xPos / 4 - fp->offsetX, yPos - yPos / 4 - fp->offsetY)
						,nana::size(bitmapWidth / 4,               bitmapHeight / 4)
					);
					rectangle toPart(nana::point(0, 0), nana::size(bitmapWidth, bitmapHeight));
					viewport_copy.stretch(fromPart, bitmap_graphics, toPart);
				}
				else {
					rectangle fromPart(
						nana::point(0 - fp->offsetX, 0 - fp->offsetY)
						,nana::size(bitmapWidth,     bitmapHeight)
					);
					rectangle toPart(
						nana::point(xPos - xPos / 4,    yPos - yPos / 4)
						,nana::size(bitmapWidth / 4,    bitmapHeight / 4)
					);
					viewport_copy.stretch(fromPart, bitmap_graphics, toPart);
				}

				//Confusing thing about nana:
				//With bitblt x.bitblt(..., y... means x is the destination and y is the source
				//With stretch, x.stretch(..., y... means y is the destination and x is the source

				viewport_dw.update();
			}

			canvas.changeParameters([=, &canvas](FractalParameters& P)
			{
				const uint width = P.width_canvas();
				const uint height = P.height_canvas();

				double zooms = zoomIn ? 2 : -2; //these 2 and -2 are the zoom sizes used for zooming in and out. They could be any number so it could be a setting.
				double magnificationFactor = pow(2, -zooms);

				double_c zoomLocation = canvas.map(xPos * oversampling / bitmap_zoom, yPos * oversampling / bitmap_zoom);

				//This is the difference between the location that is being zoomed in on (the location of the mouse cursor) and the right and top borders of the viewport, expressed as a fraction.
				double margin_right = xPos / (double)bitmapWidth;
				double margin_top = yPos / (double)bitmapHeight;
				 
				double margin_right_size = width * margin_right * P.get_x_spacing();
				double margin_top_size = height * margin_top * P.get_y_spacing();

				double margin_right_new_size = margin_right_size * magnificationFactor;
				double margin_top_new_size = margin_top_size * magnificationFactor;

				double_c new_topleftcorner =
					real(zoomLocation) - margin_right_new_size
					+ (imag(zoomLocation) + margin_top_new_size) * I;

				double_c new_center =
					real(new_topleftcorner) + (margin_right_new_size / margin_right) * 0.5
					+ (imag(new_topleftcorner) - (margin_top_new_size / margin_top) * 0.5) * I;

				P.setCenterAndZoomRelative(new_center, P.get_zoomLevel() + zooms); 
			}, zoomIn ? EventSource::zoomIn : EventSource::zoomOut, false);
		});

		return fractalpanel;
	}

	void associateFile(FractalCanvas* canvas, string path)
	{
		bool success = tabs.associateFile(path, canvas);
		if (success)
		{
			int canvas_index = tabs.indexOf(canvas);
			if (tabs.bar.activated() == canvas_index)
			{
				FractalPanel* fractalpanel = static_pointer_cast<FractalPanel>(
					tabs.canvas_panel_map[canvas]
				).get();
				caption(windowTitle(fractalpanel));
			}
		}
	}

	void drawBitmap(FractalCanvas* canvas)
	{
		assert( dynamic_cast<NanaBitmapManager*>(canvas->bitmapManager.get()) != nullptr ); //it is a NanaBitmapManager

		if (canvas == activeCanvas) {
			viewport_dw.update();
		}
	}

	
	void renderProgress(FractalCanvas* canvas, double progressPct, bool complete, double elapsedSeconds)
	{
		int index = tabs.indexOf(canvas);
		TabStatusbarLabels& labels = tabs.statusbarLabels[index];

		stringstream ssElapsed;
		ssElapsed << setprecision(5) << elapsedSeconds << " s";
			
		stringstream ssProgress;
		if (complete)
			ssProgress << "100%";
		else
			ssProgress << setprecision(2) << fixed << progressPct << '%';

		labels.time = ssElapsed.str();
		labels.progress = ssProgress.str();

		if (tabs.bar.activated() == index) { //this is the active tab, so show the changes immediately
			statusbar.time.caption(labels.time);
			statusbar.progress.caption(labels.progress);
		}
	}

	void parametersChangedAction(FractalCanvas* canvas, int source_id)
	{
		const FractalParameters& P = canvas->P();

		uint inflectionCount = P.get_inflectionCount();
		double zoomLevel = P.get_zoomLevel();
		uint width_resolution = P.width_resolution();
		uint height_resolution = P.height_resolution();
		uint oversampling = P.get_oversampling();
			
		int index = tabs.indexOf(canvas);
		TabStatusbarLabels& labels = tabs.statusbarLabels[index];

		//apply changes to the statusbar labels
		stringstream ssInflections;
		ssInflections << inflectionCount << " morphing" << (inflectionCount == 1 ? "" : "s");
			
		stringstream ssZoomLevel;
		ssZoomLevel << "2^" << (int64)zoomLevel << " = " << scientific << setprecision(3) << pow(2, zoomLevel);
			
		stringstream ssResolution;
		ssResolution << width_resolution << 'x' << height_resolution << " x " << oversampling << 'x' << oversampling;

		labels.formula = P.get_procedure().name();
		labels.inflections = ssInflections.str();
		labels.zoomlevel = ssZoomLevel.str();
		labels.resolution = ssResolution.str();

		bool tab_active = tabs.bar.activated() == index;

		if (tab_active) { //this is the active tab, so show the changes to the statusbar immediately
			statusbar.formula.caption(labels.formula);
			statusbar.inflections.caption(labels.inflections);
			statusbar.zoomlevel.caption(labels.zoomlevel);
			statusbar.resolution.caption(labels.resolution);
		}

		
		shared_ptr<EFPanelBase> pane = tabs.canvas_panel_map[canvas];
		auto fractalpanel = static_pointer_cast<FractalPanel>(pane);

		bool modified = P.modified();

		// if the tab is associated with an opened file, mark as modified (shows a * in the main window title bar)
		if(
			fractalpanel->fileAssociation
			&& fractalpanel->fileModified == false
			&& modified
		) {
			fractalpanel->fileModified = true;	
			tabs.bar.text(index, string("*") + fractalpanel->filename);
			
			if (tab_active) {
				caption(windowTitle(fractalpanel.get()));
			}
		}

		if (
			P.modifiedProcedure
			&& fractalpanel->fileAssociation == false
		) {
			tabs.bar.text(index, P.get_procedure().name());
		}


		//actions related to the source_id

		//Changing a slider's value causes a value_changed event which updates the parameters again, which causes this function to be called again... risking an infinite recursion. The "if" here is to prevent that problem.
		if (source_id != EventSource::gradientOffsetSliding && source_id != EventSource::gradientSpeedSliding)
			fractalpanel->updateControls();
		else
			fractalpanel->updateControls(false);

		//A history item is created when the gradient changes and the mouse button is released, but not while the mouse button is held. This prevents generation of many history items per second while using the sliders.
		if (source_id == EventSource::gradientSpeed)
		{
			fractalpanel->sidebar.history.addItem(P, "speed " + to_string(P.get_gradientSpeed()));
		}
		else if (source_id == EventSource::gradientOffset)
		{
			fractalpanel->sidebar.history.addItem(P, "offset " + to_string(P.get_gradientOffset()));
		}
		else if (
			source_id != EventSource::history
			&& source_id != EventSource::gradientSpeedSliding
			&& source_id != EventSource::gradientOffsetSliding
			&& modified
		) {
			string historyText = ([=, &P]() -> string
			{
				stringstream ss;
				switch (source_id) {
					case EventSource::procedure:						ss << P.get_procedure().name();
					break; case EventSource::post_transformation:	ss << "post " << transformation_name(P.get_post_transformation_type());
					break; case EventSource::size: {
						ss << "size " << P.width_resolution() << 'x' << P.height_resolution()
							<< " x " << P.get_oversampling() << 'x' << P.get_oversampling();
					}
					break; case EventSource::iters:					ss << "max " << P.get_maxIters() << " iters";
					break; case EventSource::inflectionZoom:			ss << "inflection zoom " << P.get_inflectionZoomLevel();
					break; case EventSource::location:				ss << "location " << P.get_center();
					break; case EventSource::zoomLevel:				ss << "zoom " << P.get_zoomLevel();
					break; case EventSource::rotation:				ss << "rotation " << P.get_rotation_angle();
					break; case EventSource::juliaSeed:				ss << "julia " << P.get_juliaSeed();
					break; case EventSource::julia:					ss << "julia " << (P.get_julia() ? "on" : "off");
					break; case EventSource::addInflection:			ss << "add inflection " << P.get_inflectionCount();
					break; case EventSource::removeInflection:		ss << "remove inflection " << P.get_inflectionCount() + 1;
					break; case EventSource::zoomIn:					ss << "zoom in " << P.get_zoomLevel();
					break; case EventSource::zoomOut:				ss << "zoom out " << P.get_zoomLevel();
					break; case EventSource::reset:					ss << "reset parameters";
					break; case EventSource::fractalPanel:			ss << "panel parameters set";
					break; case EventSource::JSON:					ss << "JSON applied";
					break; default: ss << "changes";
				}
				return ss.str();
			})();
			fractalpanel->sidebar.history.addItem(P, historyText);
		}


		if (P.modifiedCalculations) {
			canvas->enqueueRender();
		}
		else if (P.modifiedColors && tab_active) {
			canvas->enqueueBitmapRender();
		}

		canvas->Pmutable().clearModified();
	}

	void parametersChanged(void* canvas, int source_id)
	{
		if (source_id != EventSource::noEvents)
			SendMessage(getHwnd(), Message::PARAMETERS_CHANGED, reinterpret_cast<WPARAM>(canvas), (LPARAM)(int64)source_id);
	}

	void canvasSizeChanged(void* canvas) {
		SendMessage(getHwnd(), Message::CANVAS_SIZE_CHANGED, reinterpret_cast<WPARAM>(canvas), 0);
	}

	void canvasResizeFailed(void* canvas, ResizeResult result) {
		handleResizeResult((window)this, result);
	}

	/*
		starts a refresh thread (refreshes the screen during the render)

		createNewRenderTemplated calls renderStartedEvent on the canvas, which then calls the renderStarted implementation on every GUI.
	*/
	void renderStarted(shared_ptr<RenderInterface> render) {
		SendMessage(getHwnd(), Message::RENDER_STARTED, reinterpret_cast<WPARAM>(&render), 0);
	}

	void renderFinished(shared_ptr<RenderInterface> render) {
		SendMessage(getHwnd(), Message::RENDER_FINISHED, reinterpret_cast<WPARAM>(&render), 0);
	}

	void bitmapRenderStarted(void* canvas, uint bitmapRenderID) {
		if(debug) cout << "GUI bitmapRenderStarted" << endl;
		SendMessage(getHwnd(), Message::BITMAP_RENDER_STARTED, reinterpret_cast<WPARAM>(canvas), reinterpret_cast<LPARAM>(&bitmapRenderID));
		if(debug) cout << "GUI bitmapRenderStarted done" << endl;
	}

	void bitmapRenderFinished(void* canvas, uint bitmapRenderID) {
		SendMessage(getHwnd(), Message::BITMAP_RENDER_FINISHED, reinterpret_cast<WPARAM>(canvas), 0);
	}

	void updateStatusbarCursorinfo(double_c coordinate, uint iterationCount) {
		stringstream ssCoordinate;
		ssCoordinate << fixed << setprecision(5) << real(coordinate) << " + " << imag(coordinate) << 'i';

		statusbar.coordinates.caption(ssCoordinate.str());
		statusbar.iterations.caption(" iters: " + to_string(iterationCount));
	}

	void setTabStatusbarLabels(TabStatusbarLabels& labels) {
		statusbar.formula.caption(labels.formula);
		statusbar.inflections.caption(labels.inflections);
		statusbar.zoomlevel.caption(labels.zoomlevel);
		statusbar.resolution.caption(labels.resolution);
		statusbar.time.caption(labels.time);
		statusbar.progress.caption(labels.progress);
	}

	HWND getHwnd() {
		return reinterpret_cast<HWND>(native_handle());
	}

	void fit_to_fractal() {
		FractalCanvas* canvas = activeCanvas;
		if (canvas != nullptr) {
			assert(tabs.containsCanvas(canvas));

			auto active_panel = static_pointer_cast<FractalPanel>(tabs.canvas_panel_map[canvas]);
			nana::size viewportSize = active_panel->fractal.size(); //excluding scrollbars
			int availableX = viewportSize.width + active_panel->vscrollbar.size().width;
			int availableY = viewportSize.height + active_panel->hscrollbar.size().height;
			int differenceX = active_panel->canvas.P().get_target_width() - availableX;
			int differenceY = active_panel->canvas.P().get_target_height() - availableY;

			nana::size windowSize = size();
			size(nana::size(
				windowSize.width + differenceX
				,windowSize.height + differenceY
			));

			active_panel->recalculateScrollbars();
		}
	}

	void fit_to_window() {
		FractalCanvas* canvas = activeCanvas;
		if (canvas != nullptr) {
			assert(tabs.containsCanvas(canvas));

			auto active_panel = static_pointer_cast<FractalPanel>(tabs.canvas_panel_map[canvas]);
			nana::size viewportSize = active_panel->fractal.size(); //excluding scrollbars
			int availableX = viewportSize.width + active_panel->vscrollbar.size().width;
			int availableY = viewportSize.height + active_panel->hscrollbar.size().height;

			canvas->changeParameters([=](FractalParameters& P)
			{
				P.resize(availableX, availableY, P.get_oversampling(), P.get_bitmap_zoom());
			}, EventSource::size);
		}
	}

	void saveParametersAs() {
		FractalCanvas* canvas = activeCanvas;
		if (canvas != nullptr) {
			string path = getDate() + " " + canvas->P().get_procedure().name();

			if (BrowseFile(getHwnd(), FALSE, "Save parameters", "Parameters\0*.efp\0\0", path)) {
				bool success = writeParameters(canvas->P(), path);

				if (success) {
					associateFile(canvas, path);
				}
			}
		}
	}

	void saveParameters(FractalCanvas* canvas, string path) {
		if (tabs.containsCanvas(canvas))
		{
			bool success = writeParameters(canvas->P(), path);

			if (success) {
				auto fractalpanel = static_pointer_cast<FractalPanel>(tabs.canvas_panel_map[canvas]);

				if (fractalpanel->fileAssociation) {
					fractalpanel->fileAssociation = false; //a path is given, so remove an existing association
					fractalpanel->fileModified = false;
				}
				associateFile(canvas, path);
			}
		}
	}

	void saveParameters(FractalCanvas* canvas) {
		if (tabs.containsCanvas(canvas))
		{
			auto fractalpanel = static_pointer_cast<FractalPanel>(tabs.canvas_panel_map[canvas]);

			if ( ! fractalpanel->fileAssociation)
			{
				string path = getDate() + " " + canvas->P().get_procedure().name();

				if (BrowseFile(getHwnd(), FALSE, "Save parameters", "Parameters\0*.efp\0\0", path))
				{
					bool success = writeParameters(canvas->P(), path);

					if (success) {
						associateFile(canvas, path);
					}
				}				
			}
			else if (
				fractalpanel->fileAssociation
				&& fractalpanel->fileModified
			) {
				bool success = writeParameters(canvas->P(), fractalpanel->filePath);
				if (success) {
					fractalpanel->fileModified = false;

					int index = tabs.indexOf(static_pointer_cast<EFPanelBase>(fractalpanel).get());
					tabs.bar.text(index, fractalpanel->filename);
					caption(windowTitle(fractalpanel.get()));
				}
			}
		}
	}

	void saveParameters() {
		if (activeCanvas != nullptr) {
			saveParameters(activeCanvas);
		}
	}

	void saveBothAs() {
		if (activeCanvas != nullptr) {
			string path = getDate() + " " + activeCanvas->P().get_procedure().name();
			if (BrowseFile(getHwnd(), FALSE, "Save parameters and image", "Parameters\0*.efp\0\0", path)) {
				saveParameters(activeCanvas, path);
				saveImage(activeCanvas, path + ".png");
			}
		}
	}

	void loadParameters() {
		string path = "";
		if (BrowseFile(getHwnd(), TRUE, "Load parameters", "Parameters\0*.efp\0\0", path)) {
			FractalParameters newP;

			ReadResult res = readParametersFile(newP, path);
			handleReadResult(*this, res);

			if (res == ReadResult::succes) {
				shared_ptr<FractalPanel> fractalpanel = create_fractal_tab(newP, "");

				tabs.associateFile(path, &fractalpanel->canvas);
				caption(windowTitle(fractalpanel.get()));
			}
		}
	}
};

// Based on code found at: http://nanapro.sourceforge.net/faq.htm
// I made small changes.
// I use this class to add functionality to the message loop of the nana window.
/*
 *	A helper class for subclassing under Windows.
 *	This is a demo for Nana C++ Library.
 *
 *	Nana library does not provide functions to access the Windows Messages. But
 *	the demo is intend to define a helper to access the Window Messages by subclassing
 *	it.
 *
 *	A C++11 compiler is required for the demo.
 */
class subclass
{
	struct msg_pro
	{
		std::function<bool(UINT, WPARAM, LPARAM, LRESULT*)> before;
		std::function<bool(UINT, WPARAM, LPARAM, LRESULT*)> after;
	};

	typedef std::lock_guard<std::recursive_mutex> lock_guard;
public:
	subclass(nana::window wd)
		:	native_(reinterpret_cast<HWND>(nana::API::root(wd))),
			old_proc_(nullptr)
	{
	}

	~subclass()
	{
		clear();
	}

	void make_before(UINT msg, std::function<bool(UINT, WPARAM, LPARAM, LRESULT*)> fn)
	{
		lock_guard lock(mutex_);
		msg_table_[msg].before = std::move(fn);
		_m_subclass(true);
	}

	void make_after(UINT msg, std::function<bool(UINT, WPARAM, LPARAM, LRESULT*)> fn)
	{
		lock_guard lock(mutex_);
		msg_table_[msg].after = std::move(fn);
		_m_subclass(true);
	}

	void umake_before(UINT msg)
	{
		lock_guard lock(mutex_);
		auto i = msg_table_.find(msg);
		if(msg_table_.end() != i)
		{
			i->second.before = nullptr;
			if(nullptr == i->second.after)
			{
				msg_table_.erase(msg);
				if(msg_table_.empty())
					_m_subclass(false);
			}
		}
	}

	void umake_after(UINT msg)
	{
		lock_guard lock(mutex_);
		auto i = msg_table_.find(msg);
		if(msg_table_.end() != i)
		{
			i->second.after = nullptr;
			if(nullptr == i->second.before)
			{
				msg_table_.erase(msg);
				if(msg_table_.empty())
					_m_subclass(false);
			}
		}
	}

	void umake(UINT msg)
	{
		lock_guard lock(mutex_);
		msg_table_.erase(msg);

		if(msg_table_.empty())
			_m_subclass(false);
	}

	void clear()
	{
		lock_guard lock(mutex_);
		msg_table_.clear();
		_m_subclass(false);
	}
private:
	void _m_subclass(bool enable)
	{
		lock_guard lock(mutex_);

		if(enable)
		{
			if(native_ && (nullptr == old_proc_))
			{
				old_proc_ = (WNDPROC)::SetWindowLongPtr(native_, GWLP_WNDPROC, (LONG_PTR)_m_subclass_proc);
				if(old_proc_)
					table_[native_] = this;
			}
		}
		else
		{
			if(old_proc_)
			{
				table_.erase(native_);
				::SetWindowLongPtr(native_, GWLP_WNDPROC, (LONG_PTR)old_proc_);
				old_proc_ = nullptr;

			}
		}
	}

	static bool _m_call_before(msg_pro& pro, UINT msg, WPARAM wp, LPARAM lp, LRESULT* res)
	{
		return (pro.before ? pro.before(msg, wp, lp, res) : true);
	}

	static bool _m_call_after(msg_pro& pro, UINT msg, WPARAM wp, LPARAM lp, LRESULT* res)
	{
		return (pro.after ? pro.after(msg, wp, lp, res) : true);
	}
private:
	static LRESULT CALLBACK _m_subclass_proc(HWND wd, UINT msg, WPARAM wp, LPARAM lp)
	{
		lock_guard lock(mutex_);

		subclass * self = _m_find(wd);
		if(nullptr == self || nullptr == self->old_proc_)
			return 0;

		auto i = self->msg_table_.find(msg);
		if(self->msg_table_.end() == i)
			return ::CallWindowProc(self->old_proc_, wd, msg, wp, lp);

		LRESULT res = 0;
		bool continue_ = self->_m_call_before(i->second, msg, wp, lp, &res);
		if (continue_)
		{
			res = ::CallWindowProc(self->old_proc_, wd, msg, wp, lp);
			self->_m_call_after(i->second, msg, wp, lp, &res);
			return res;
		}

		if(WM_DESTROY == msg)
			self->clear();

		return res;
	}

	static subclass * _m_find(HWND wd)
	{
		lock_guard lock(mutex_);
		std::map<HWND, subclass*>::iterator i = table_.find(wd);
		if(i != table_.end())
			return i->second;

		return 0;
	}
private:
	HWND native_;
	WNDPROC old_proc_;
	std::map<UINT, msg_pro> msg_table_;

	static std::recursive_mutex mutex_;
	static std::map<HWND, subclass*> table_;
};

std::recursive_mutex subclass::mutex_;
std::map<HWND, subclass*> subclass::table_;



int GUI_main(FractalParameters& defaultParameters, uint number_of_threads, FractalParameters& firstTabParameters)
{
	paint::image_process::selector().stretch("proximal interpolation"); //stretch algorithm used while zooming in and out

	fontsize(10); //widgets created from now on will have a font size of 10 points


	main_form fm(defaultParameters, number_of_threads);
	fm.caption("ExploreFractals");
	theOnlyNanaGUI = static_cast<GUIInterface*>(&fm);
	main_form_hwnd = fm.getHwnd();


	subclass sc(fm);

	//Install a handler that will be called before old window proc, and it determines wheter
	//to pass the controll into the old window proc and the AFTER handler.
	
	sc.make_before(WM_KEYDOWN, [&fm](UINT, WPARAM wParam, LPARAM, LRESULT*)
	{
		// This is not a great solution. This only checks if the key is pressed right now. If the program hangs and the user presses ctrl+T, by the time the keypress for T is handled, ctrl is no longer pressed and it will be handled as if the user pressed just T.
		auto isDown = [](WPARAM key)
		{
			SHORT keyState = GetAsyncKeyState(key);
			return ( 1 << 15 ) & keyState;
		};

		bool ctrl = isDown(VK_CONTROL);
		bool shift = isDown(VK_SHIFT);

		if (ctrl) {
			//Override normal behavior when ctrl is pressed.
			//Returning true means control is passed to the old window proc.

			tabbar<string>& tbar = fm.tabs.bar;

			auto& brock = detail::bedrock::instance();

			switch (wParam)
			{
				case 'T' : {
					//create new tab
					{
						//This root_guard disables screen refreshing and re-enables it when it's destroyed.
						detail::bedrock::root_guard rg{brock, fm};
						fm.create_fractal_tab(fm.defaultParameters, fm.defaultParameters.get_procedure().name());
					}
					API::refresh_window(fm);

					return false;
				}
				case 'W' : {
					//close current tab
					if (tbar.length() == 0) {
						return true;
					}
					{
						detail::bedrock::root_guard rg{brock, fm};
						tbar.erase(tbar.activated()); //this also causes the removed event. The event handler deallocates memory.
					}
					API::refresh_window(fm);
					return false;
				}
				case VK_TAB: {
					//move to the next/previous tab
					size_t length = tbar.length();
					if (length == 0) {
						return true;
					}
					else if (shift) {
						int index = (tbar.activated() - 1 + length) % length;
						tbar.activated(index);
					}
					else {
						int index = (tbar.activated() + 1 + length) % length;
						tbar.activated(index);
					}
					return false;
				}
				case 'D': {
					//Duplicate current tab
					if (tbar.length() == 0) {
						return true;
					}
					auto panel = fm.tabs.panels[ tbar.activated() ];
					if (panel->kind() == TabKind::Fractal)
					{
						shared_ptr<FractalPanel> panel_ = static_pointer_cast<FractalPanel>(panel);
						{
							detail::bedrock::root_guard rg{brock, fm};

							fm.create_fractal_tab(panel_->canvas.P(), panel_->canvas.P().get_procedure().name(), panel_->showing_sidepanel);
						}
						API::refresh_window(fm);
					}
					return false;
				}
				case 'O': {
					//open a file
					fm.loadParameters();
					return false;
				}
				case 'S' : {
					//save the parameters in the current tab
					if (tbar.length() == 0) {
						return true;
					}
					if (shift) {
						fm.saveParameters();
					}
					else {
						fm.saveBothAs();
					}
					return false;
				}
				case 'Z' : {
					//Go to the previous history item
					if (tbar.length() == 0) {
						return true;
					}
					auto panel = fm.tabs.panels[ tbar.activated() ];
					if (panel->kind() == TabKind::Fractal) {
						shared_ptr<FractalPanel> panel_ = static_pointer_cast<FractalPanel>(panel);
						panel_->sidebar.history.move_select(false);
					}
					return false;
				}
				case 'Y' : {
					//Go to the next history item
					if (tbar.length() == 0) {
						return true;
					}
					auto panel = fm.tabs.panels[ tbar.activated() ];
					if (panel->kind() == TabKind::Fractal) {
						shared_ptr<FractalPanel> panel_ = static_pointer_cast<FractalPanel>(panel);
						panel_->sidebar.history.move_select(true);
					}
					return false;
				}
				//todo: remove test
				case 'Q' : {
					showMorphing = ! showMorphing;
					return false;
				}
				default : {
					return true;
				}
			}
		}
		return true;
	});

	sc.make_before(Message::RENDER_STARTED, [&fm](UINT, WPARAM wParam, LPARAM, LRESULT*)
	{
		if(debug) cout << "Message RENDER_STARTED" << endl;
		shared_ptr<RenderInterface> render = *reinterpret_cast<shared_ptr<RenderInterface>*>(wParam);
		FractalCanvas* canvas = reinterpret_cast<FractalCanvas*>(render->canvasPtr());
		uint renderID = render->getId();

		if (fm.tabs.containsCanvas(canvas))
		{
			canvas->addToThreadcount(1);
			thread refreshThread([=]()
			{
				refreshDuringRender(std::move(render), canvas, renderID);
				canvas->addToThreadcount(-1);
			});
			refreshThread.detach();
		}
		return false;
	});

	sc.make_before(Message::RENDER_FINISHED, [&fm](UINT, WPARAM wParam, LPARAM, LRESULT*)
	{
		if(debug) cout << "Message RENDER_FINISHED" << endl;
		shared_ptr<RenderInterface> render = *reinterpret_cast<shared_ptr<RenderInterface>*>(wParam);
		FractalCanvas* canvas = reinterpret_cast<FractalCanvas*>(render->canvasPtr());

		if (fm.tabs.containsCanvas(canvas))
		{
			RenderInterface::ProgressInfo progress = render->getProgress();
			assert(progress.ended);
			fm.renderProgress(canvas, 100, true, progress.elapsedTime);
			fm.drawBitmap(canvas);
		}

		return false;
	});

	sc.make_before(Message::BITMAP_RENDER_STARTED, [&fm](UINT, WPARAM wParam, LPARAM lParam, LRESULT*)
	{
		if(debug) cout << "Message BITMAP_RENDER_STARTED" << endl;
		FractalCanvas* canvas = reinterpret_cast<FractalCanvas*>(wParam);
		uint bitmapRenderID = *reinterpret_cast<uint*>(lParam);

		if (fm.tabs.containsCanvas(canvas))
		{
			canvas->addToThreadcount(1);
			thread refreshThread([=, &fm]()
			{
				refreshDuringBitmapRender(canvas, bitmapRenderID);
				canvas->addToThreadcount(-1);
			});
			refreshThread.detach();
		}

		return false;
	});

	sc.make_before(Message::BITMAP_RENDER_FINISHED, [&fm](UINT, WPARAM wParam, LPARAM, LRESULT*)
	{
		if(debug) cout << "Message BITMAP_RENDER_FINISHED" << endl;
		FractalCanvas* canvas = reinterpret_cast<FractalCanvas*>(wParam);

		if (fm.tabs.containsCanvas(canvas))
		{
			fm.drawBitmap(canvas);
		}

		return false;
	});

	sc.make_before(Message::PARAMETERS_CHANGED, [&fm](UINT, WPARAM wParam, LPARAM lParam, LRESULT*)
	{
		if(debug) cout << "Message PARAMETERS_CHANGED" << endl;
		FractalCanvas* canvas = reinterpret_cast<FractalCanvas*>(wParam);
		int source_id = int(int64(lParam));

		if (fm.tabs.containsCanvas(canvas)) {
			fm.parametersChangedAction(canvas, source_id);
		}

		return false;
	});

	sc.make_before(Message::CANVAS_SIZE_CHANGED, [&fm](UINT, WPARAM wParam, LPARAM, LRESULT*)
	{
		if(debug) cout << "Message CANVAS_SIZE_CHANGED" << endl;
		FractalCanvas* canvas = reinterpret_cast<FractalCanvas*>(wParam);

		if ( fm.tabs.containsCanvas(canvas) )
		{
			static_pointer_cast<FractalPanel>(
				fm.tabs.panels[
					fm.tabs.indexOf( canvas )
				]
			)->recalculateScrollbars();
		}

		return false;
	});

	sc.make_before(Message::SHOW_PROGRESS_UNFINISHED, [&fm](UINT, WPARAM wParam, LPARAM, LRESULT*)
	{
		if(debug) cout << "Message SHOW_PROGRESS_UNFINISHED" << endl;
		RenderInterface* render = reinterpret_cast<RenderInterface*>(wParam);
		FractalCanvas* canvas = reinterpret_cast<FractalCanvas*>(render->canvasPtr());

		if (fm.tabs.containsCanvas(canvas)) {
			RenderInterface::ProgressInfo progress = render->getProgress();
			uint renderSize = render->getWidth() * render->getHeight();
			double progressPct = (double)(progress.guessedPixelCount + progress.calculatedPixelCount) / renderSize * 100;

			fm.renderProgress(canvas, progressPct, progress.ended, progress.elapsedTime);
		}
		return false;
	});

	sc.make_before(Message::DRAW_BITMAP, [&fm](UINT, WPARAM wParam, LPARAM, LRESULT*)
	{
		if(debug) cout << "Message DRAW_BITMAP" << endl;
		FractalCanvas* canvas = reinterpret_cast<FractalCanvas*>(wParam);
		if (fm.tabs.containsCanvas(canvas)) {
			fm.drawBitmap(canvas);
		}
		return false;
	});

	sc.make_before(Message::CLEANUP_FRACTAL_TAB, [&fm](UINT, WPARAM wParam, LPARAM, LRESULT*)
	{
		if(debug) cout << "Message CLEANUP_FRACTAL_TAB" << endl;
		FractalCanvas* canvas = reinterpret_cast<FractalCanvas*>(wParam);

		bool canvas_in_map = fm.tabs.canvas_panel_map.find(canvas) != fm.tabs.canvas_panel_map.end();
		assert(canvas_in_map);

		if (canvas_in_map)
		{
			//just to be sure
			canvas->end_all_usage();

			cout << "before the cleanup" << endl;
			//the cleanup
			fm.tabs.canvas_panel_map.erase(canvas);
			cout << "after the cleanup" << endl;
		}
		return false;
	});

	sc.make_before(Message::CLEANUP_CHILD_WINDOW, [&fm](UINT, WPARAM wParam, LPARAM, LRESULT*)
	{
		if(debug) cout << "Message CLEANUP_CHILD_WINDOW" << endl;
		child_window_base* child = reinterpret_cast<child_window_base*>(wParam);

		int indexof = -1;
		for (int i=0; i<fm.childWindows.size(); i++) {
			if (fm.childWindows[i].get() == child) {
				indexof = i;
				break;
			}
		}
		
		// There is a valid situation in which indexof remains -1. This happens when a window is closed BY removing its resource from childWindows (which happens in the destroy event handler below). That also triggers the cleanup, but there is nothing to clean up.
		if (indexof >= 0)
			fm.childWindows.erase(fm.childWindows.begin() + indexof);

		return false;
	});

	fm.events().destroy([&fm]()
	{
		if(debug) cout << "main form is being destroyed" << endl;
		fm.childWindows.clear(); //this closes all child windows
		if(debug) cout << "all child windows closed" << endl;
	});
	
	//initial tab
	fm.create_fractal_tab(firstTabParameters, firstTabParameters.get_procedure().name());

	//set the window size such that the fractal is visible
	fm.fit_to_fractal();

	fm.show();

	exec();
	if(debug) cout << "Nana instance has ended." << endl;

	return 0;
}

} //namespace GUI

#endif