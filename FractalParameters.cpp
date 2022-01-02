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

#ifndef FRACTALPARAMETERS_H
#define FRACTALPARAMETERS_H

//rapidjson
#include "rapidjson/document.h"     // rapidjson's DOM-style API
#include "rapidjson/prettywriter.h" // for stringify JSON

//this program
#include "common.cpp"

#define get_trick(name) get_ ## name
#define readonly(type, name) \
private: type name; \
public: inline const type& get_trick(name)() const {\
	return name;\
}

/*
	contains more than just the parameters. It also contains some values that aren't stricyly necessary to store but that make calculations more efficient. The function toJson creates a JSON representation that contains only the strictly necessary values. fromJson does the same in the other direction.

	All the member functions that change the parameters also update the booleans modifiedSize, modifiedCalculations and modifiedColors, if the function causes a change in size (screenWidth, screenHeight, oversampling), parameters that matter for calculations of iteration counts, and coloring respectively. This can be used to keep track of what actions are needed after making several changes to the parameters.
*/
class FractalParameters {

	public: FractalParameters()
	: procedure(M2) //because Procedure doesn't have a default constructor
	, inflectionCoords(250)
	{
		initialize();
	}

	// These are bools to keep track of changes since the last time clearModified was used.
	// If a change is made and then reverted, the bools remain the same. They don't track net changes.
	bool modifiedMemory;
	bool modifiedSize;
	bool modifiedCalculations;
	bool modifiedColors;
	bool modifiedProcedure;
	bool modifiedInflectionZoom;
	bool modifiedJuliaSeed;

	void clearModified() {
		modifiedMemory = false;
		modifiedSize = false;
		modifiedCalculations = false;
		modifiedColors = false;
		modifiedProcedure = false;
		modifiedInflectionZoom = false;
		modifiedJuliaSeed = false;
	}

	bool modified() const { return (
		modifiedMemory
		|| modifiedSize
		|| modifiedCalculations
		|| modifiedColors
		|| modifiedProcedure
		|| modifiedInflectionZoom
		|| modifiedJuliaSeed
	);}

	//
	// The target is the size as set by the user in the GUI.
	// Oversampling increases the number of samples (or iterated complex numbers) per color on screen. An oversampling of 2 means that all colors will be the average of the colors of a grid of 2×2 points/complex numbers (4 in total). This is the same as creating a large bitmap first and then shrinking it to the target size.
	//
	// Bitmap zoom is the opposite of oversampling. A bitmap zoom of 2 mean that for every 2×2 grid of pixels only 1 point is calculated to determine the color. The effect of this is the same as creating a small bitmap first and then stretching it to the target size.
	//
	// The target size is only a target. When bitmap_zoom is 1, the target is always possible, but when bitmap_zoom is larger, the target size must be divisible by bitmap_zoom to achieve the target size. For example, 101×101 with a bitmap_zoom of 2 means that the actual resolution will be 50×50 (rounded down from 50,5×50,5) which, with the bitmap_zoom applied, will result in 100×100 pixels being shown on screen.
	//
	readonly(uint, target_width)
	readonly(uint, target_height)
	readonly(uint, oversampling)
	readonly(uint, bitmap_zoom)

	//resolution is the number of distinct colors visible to the user
	//I consider the resolution of a zoomed bitmap to be the same as of an unzoomed bitmap. A zoomed bitmap takes up more space on the screen and in memory but it contains no extra details.
	//The resolution can't be 0, even if the user asks for it. It's an assumption throughout the program.
	inline uint width_resolution() const {
		uint res = target_width / bitmap_zoom;
		if (res == 0)	return 1;
		else				return res;
	}
	inline uint height_resolution() const {
		uint res = target_height / bitmap_zoom;
		if (res == 0)	return 1;
		else				return res;
	}

	//The actual size of the bitmap
	inline uint width_bitmap() const {
		return width_resolution() * bitmap_zoom;
	}
	inline uint height_bitmap() const {
		return height_resolution() * bitmap_zoom;
	}
	
	//The number of calculated points
	inline uint width_canvas() const {
		return width_resolution() * oversampling;
	}
	inline uint height_canvas() const {
		return height_resolution() * oversampling;
	}

	readonly(double_c, center) //This is the center in the (untransformed) complex plane. The coordinate in the center of the viewport can be different because of rotation, inflections etc. Generally this touches on how the program does transformations. The viewport is first identified with a rectangular region in the complex plane, which is then transformed in various ways. This is the center of the starting region. The same goes for the topleftCorner, x_range and y_range.
	readonly(double, x_range)
	readonly(double, y_range)
	//The spacing is the distance between points in the canvas.
	//This is stored for efficiency (does this actually help?).
	//Formula: x_spacing = x_range / width_canvas()
	readonly(double, x_spacing)
	readonly(double, y_spacing)

	//Also stored for efficiency
	//Formula: topleftCorner = center - x_range / 2 + (y_range / 2) * I
	readonly(double_c, topleftCorner)

	readonly(uint, maxIters)

	readonly(bool, julia)
	readonly(double_c, juliaSeed);
	
	private: Procedure procedure;
	readonly(int, procedure_identifier)
	public: Procedure get_procedure() const { return getProcedureObject(procedure_identifier); }
	
	readonly(vector<double_c>, inflectionCoords);//the locations of created Julia morphings
	readonly(uint, inflectionCount)
	readonly(double, inflectionZoomLevel) //reset to this zoom level upon creating a new inflection

	readonly(double, gradientSpeed) //a measure how much the gradient is stretched (or shrunk, when it's negative)
	readonly(double, gradientOffset)
	readonly(double, gradientSpeedFactor) //stored for efficiency
	readonly(double, gradientOffsetTerm) //stored for efficiency
	readonly(vector<ARGB>, gradientColors)

	readonly(double, rotation_angle) //angle between 0 and 1. 0 means 0 degrees. 0.25 means 90 degrees etc.
	readonly(double_c, center_of_rotation)
	//This is for complex number rotation, which works by multiplying with an element of the unit circle. The number is stored for efficiency.
	//Formula: exp(rotation_angle * 2*pi*I).
	readonly(double_c, rotation_factor)

	readonly(double, partialInflectionPower)
	readonly(double_c, partialInflectionCoord)
	readonly(int, post_transformation_type)
	readonly(int, pre_transformation_type)
	
public:
	bool setPartialInflectionPower(double power) {
		bool changed = power != partialInflectionPower;
		
		partialInflectionPower = power;

		modifiedCalculations |= changed;
		return changed;
	}

	bool setPartialInflectionCoord(double_c coord) {
		bool changed = coord != partialInflectionCoord;
		
		partialInflectionCoord = coord;

		modifiedCalculations |= changed;
		return changed;
	}

	double transferFunction(double gradientSpeed_) const {
		return pow(1.1, gradientSpeed_ - 1);
	}

	bool setGradientOffset(double newGradientOffset) {
		bool changed = false;
		assert(newGradientOffset >= 0.0 && newGradientOffset <= 1.0);
		if (newGradientOffset >= 0.0 && newGradientOffset <= 1.0) {
			changed = gradientOffset != newGradientOffset;
			gradientOffset = newGradientOffset;
			double interpolatedGradientLength = gradientColors.size() * transferFunction(gradientSpeed);
			gradientOffsetTerm = interpolatedGradientLength * gradientOffset;
		}
		modifiedColors |= changed;
		return changed;
	}

	bool setGradientSpeed(double newGradientSpeed) {
		bool changed = newGradientSpeed != gradientSpeed;

		gradientSpeed = newGradientSpeed;
		double computedGradientSpeed = transferFunction(gradientSpeed);
		gradientSpeedFactor = 1.0 / computedGradientSpeed;

		assert(gradientSpeedFactor > 0);

		gradientOffsetTerm = gradientColors.size() * computedGradientSpeed * gradientOffset;

		modifiedColors |= changed;
		return changed;
	}

	bool setGradientColors(const vector<ARGB>& colors) {
		bool changed = false;
		if (gradientColors.size() != colors.size()) {
			gradientColors.resize(colors.size());
			changed = true;
		}
		for (int i=0; i<colors.size(); i++) {
			ARGB& old = gradientColors[i];
			const ARGB& new_ = colors[i];
			if (old != new_)
				changed = true;
			old = new_;
		}
		modifiedColors |= changed;
		return changed;
	}

	double get_zoomLevel() const {
		return -log2(x_range) + 2;
	}

	bool setMaxIters(uint newMaxIters) {
		assert(newMaxIters > 0);
		if (newMaxIters <= 0 || newMaxIters == maxIters)
			return false;
		maxIters = newMaxIters;
		modifiedCalculations = true;
		return true;
	}

private:
	/*
		The private version of this function does the actual work. The public versions call this function and in addition re-center the rotation. The problem with letting setCenterAndZoomPrivate re-center the rotation is that setRotation also calls setCenterAndZoomPrivate. It would cause infinite recursion.
	*/
	bool setCenterAndZoomPrivate(double_c newCenter, double zoom) {
		//Set both of these settings together because the zoom level, topleftCorner coordinate and pixel size are related.
		uint width = width_canvas();
		uint height = height_canvas();

		auto setRenderRange = [&](double zoom) {
			double x_range_new = 4 / pow(2, zoom);
			double y_range_new = x_range_new * ((double)height / (double)width);
			if (x_range_new != x_range || y_range_new != y_range) {
				x_range = x_range_new;
				y_range = y_range_new;
				return true;
			}
			return false;
		};

		auto setCoordinates = [&](double_c newCenter) {
			bool recalcRequired = (center != newCenter);
			center = newCenter;
			topleftCorner = center - x_range / 2 + (y_range / 2)*I;
			return recalcRequired;
		};

		auto update_spacing = [&]() {
			double new_x_spacing = x_range / width;
			double new_y_spacing = y_range / height;
			if (new_x_spacing != x_spacing || new_y_spacing != y_spacing) {
				x_spacing = new_x_spacing;
				y_spacing = new_y_spacing;
				return true;
			}
			return false;
		};

		bool changed = false;

		//setRenderRange must be done first because it changes the x_range and y_range that the following two actions use.
		changed |= setRenderRange(zoom);
		changed |= setCoordinates(newCenter);
		changed |= update_spacing();

		assert(x_range > 0);
		assert(y_range > 0);

		modifiedCalculations |= changed;
		return changed;
	}


public:

	bool setCenterAndZoomAbsolute(double_c newCenter, double zoom) {
		double old_angle = rotation_angle;
		bool oldModifiedCalculations = modifiedCalculations; //needed because the setRotation calls set modifiedCalculations, even though the rotation is restored.
		setRotation(0);
		bool changed = setCenterAndZoomPrivate(newCenter, zoom);
		setRotation(old_angle);
		modifiedCalculations = oldModifiedCalculations || changed;
		return changed;
	}

	bool setCenterAndZoomRelative(double_c newCenter, double zoom) {
		bool changed = setCenterAndZoomPrivate(newCenter, zoom);
		normalizeRotation(); //This sets rotation to use the new center as its center of rotation, but not changing the angle
		modifiedCalculations |= changed;
		return changed;
	}

	bool setCenter(double_c newCenter) {
		bool changed = setCenterAndZoomAbsolute(newCenter, get_zoomLevel());
		modifiedCalculations |= changed;
		return changed;
	}

	bool setZoomLevel(double zoomLevel) {
		bool changed = setCenterAndZoomAbsolute(center, zoomLevel);
		modifiedCalculations |= changed;
		return changed;
	}

	/*
		normalize an angle to a value between 0 end 1
	*/
	double normalize_angle(double angle) {
		angle -= (int)angle;
		if (angle < 0)
			angle += 1;
		assert(angle >= 0.0); assert(angle <= 1.0);
		return angle;
	}

	double normalize_gradientOffset(double offset) {
		return normalize_angle(offset);
	}

	bool setRotation(double angle) {
		double normalized_angle = normalize_angle(angle);
		bool changed = normalized_angle != rotation_angle;

		if (changed) {
			//There may be an existing rotation. This is the center of the viewport under that rotation:
			double_c current_center = rotation(center);
			//move to that location
			setCenterAndZoomPrivate(current_center, get_zoomLevel());
			//Using the center as the center of rotation makes sense for the user. Here also the new angle is set and the rotation_factor recomputed:
			center_of_rotation = center;
			rotation_angle = normalized_angle;
			rotation_factor = exp(rotation_angle * 2*pi*I);
		}

		modifiedCalculations |= changed;;
		return changed;
	}

	void normalizeRotation() {
		double_c current_center = rotation(center);
		//move to that location
		setCenterAndZoomPrivate(current_center, get_zoomLevel());
		//Using the center as the center of rotation makes sense for the user. Here also the new angle is set and the rotation_factor recomputed:
		center_of_rotation = center;
		rotation_factor = exp(rotation_angle * 2*pi*I);
	}

	inline double_c map(uint xPos, uint yPos) const {
		assert(xPos >= 0); assert(xPos <= width_canvas());
		assert(yPos >= 0); assert(yPos <= height_canvas());
		return topleftCorner + xPos * x_spacing - yPos * y_spacing*I;
	}

	inline double_c rotation(double_c c) const {
		return (c - center_of_rotation) * get_rotation_factor() + center_of_rotation;
	}

	inline double_c pre_transformation(double_c c) const {
		//mostly copied from post_transformation, could be better
		switch(pre_transformation_type) {
			case 0:
				return c;
			case 1: {
				double_c z = 0;
				for (int i = 0; i < 5; i++) {
					z = z * z + c;
				}
				return z;
			}
			case 2:
				return cos(c);
			case 3:
				return c + 2. + 2.*I;
			case 4:
				return sqrt(c);
			case 5:
				return sqrt(sqrt(c));
			case 6:
				return log(c);
			case 7:
				return pow(c - partialInflectionCoord, partialInflectionPower) + partialInflectionCoord;
		}
		assert(false);
		return 0;
	}

	double_c inflections(double_c c) const {
		for (int i = inflectionCount - 1;  i>=0;  i--) {
			c = pow(c, procedure.inflectionPower) + inflectionCoords[i];
		}
		return c;
	}

	inline double_c post_transformation(double_c c) const {
		switch (post_transformation_type) {
			case 0:
				return c;
			case 1: {
				double_c z = 0;
				for (int i = 0; i < 5; i++) {
					z = z * z + c;
				}
				return z;
			}
			case 2:
				return cos(c);
			case 3:
				return c + 2. + 2.*I;
			case 4:
				return sqrt(c);
			case 5:
				return sqrt(sqrt(c));
			case 6:
				return log(c);
			case 7:
				return pow(c, partialInflectionPower) + partialInflectionCoord;
		}
		assert(false);
		return 0;
	}

	double_c map_transformations(double_c c) const {
		return
			post_transformation(
			inflections(
			pre_transformation(
			rotation(c))));
	}

	double_c map_with_transformations(int x, int y) const {
		return map_transformations(map(x, y));
	}

private:
	bool updateCenterAndZoom() {
		return setCenterAndZoomRelative(center, get_zoomLevel());
	}

public:
	bool resize(uint target_width, uint target_height, uint oversampling, uint bitmap_zoom)
	{
		bool changed = false;
		if (
			this->oversampling != oversampling
			|| this->bitmap_zoom != bitmap_zoom
			|| this->target_height != target_height
			|| this->target_width != target_width
		) {
			this->oversampling = oversampling;
			this->bitmap_zoom = bitmap_zoom;
			this->target_height = target_height;
			this->target_width = target_width;

			updateCenterAndZoom(); //this causes recalculation of the pixel width and some other things
			changed = true;
		}
		modifiedMemory |= changed;
		modifiedSize |= changed;
		modifiedCalculations |= changed;
		return changed;
	}

	void setJulia(bool julia)
	{
		if (this->julia != julia)
			modifiedCalculations = true;

		this->julia = julia;
	}

	void setJuliaSeed(double_c seed)
	{
		if (isfinite(seed) == false) { //seed is NaN
			cout << "Attempt to set a NaN julia seed" << endl;
			return; //dontdo: maybe show an error message when this happens
		}

		if (juliaSeed != seed) {
			modifiedJuliaSeed = true;

			//This always works well. If this is skipped because julia is false, and julia is later changed, that also sets modifiedCalculations to true.
			if(julia) modifiedCalculations = true;
		}
		juliaSeed = seed;
	}
	
	bool setProcedure(int id) {
		Procedure newProcedure = getProcedureObject(id);
		assert(newProcedure.id != -1);
		if (newProcedure.id == -1) return false; //The idenfitifer was not found by getProcedureObject, should never happen

		bool changed = false;

		if (procedure.id != newProcedure.id) {
			procedure_identifier = newProcedure.id;
			procedure = newProcedure;
			changed = true;
		}
		modifiedCalculations |= changed;
		modifiedProcedure |= changed;
		return changed;
	}
	
	bool setPostTransformation(int transformation_type) {
		bool changed = transformation_type != post_transformation_type;
		post_transformation_type = transformation_type;
		modifiedCalculations |= changed;
		return changed;
	}

	bool setPreTransformation(int transformation_type) {
		bool changed = transformation_type != pre_transformation_type;
		pre_transformation_type = transformation_type;
		modifiedCalculations |= changed;
		return changed;
	}

	/*
		Normally when an inflection is set the zoom level gets reset to 0.
		These two functions help to set that "reset zoom level".

		However, addInflection uses a correction factor of 1/2 for every inflection set, to keep the pattern the same size.
		The reason is: An inflection halves the distance (the power of the magnification factor) to stuff lying deeper below.

		inflectionZoomLevel is used as a base zoom level (no correction factor applied).
		Therefore to derive it from the current zoom level, the correction needs to be undone (by multiplying by power).
	*/
	void setInflectionZoomLevel(double zoomLevel) {
		bool changed = inflectionZoomLevel != zoomLevel;
		inflectionZoomLevel = zoomLevel;
		modifiedInflectionZoom |= changed;
	}

	void setInflectionZoomLevel() {
		double power = pow(2, inflectionCount);
		setInflectionZoomLevel(get_zoomLevel() * power);
	}

	void addInflection(double_c c) {
		while (inflectionCount >= inflectionCoords.size()) {
			cout << "inflectionCount: " << inflectionCount << "    inflectionCoords.size(): " << inflectionCoords.size() << endl;
			inflectionCoords.resize(inflectionCoords.size() * 2 + 1);
			modifiedMemory = true;
		}
		inflectionCoords[inflectionCount] = c;
		inflectionCount++;
		double oldAngle = rotation_angle;
		setRotation(0);
		setCenterAndZoomAbsolute(0, inflectionZoomLevel*(1 / pow(2, inflectionCount)));
		setRotation(oldAngle);
		modifiedCalculations = true; //setRotation also sets this to true
	}

	void addInflection(uint xPos, uint yPos) {
		assert( ! (xPos < 0 || xPos > width_canvas() || yPos < 0 || yPos > height_canvas()) );
		double_c thisInflectionCoord = pre_transformation(rotation(map(xPos, yPos)));
		addInflection(thisInflectionCoord);
		modifiedCalculations = true;
	}

	bool setInflections(const vector<double_c>& inflections, uint inflectionCount) {
		assert(inflections.size() >= inflectionCount);

		bool changed = false;
		if (this->inflectionCount != inflectionCount) {
			changed = true;
			inflectionCoords = inflections;
			this->inflectionCount = inflectionCount;
		}
		else {
			for (uint i=0; i<inflectionCount; i++) {
				if (inflectionCoords[i] != inflections[i])
					changed = true;

				inflectionCoords[i] = inflections[i];
			}
		}

		modifiedCalculations |= changed;
		return changed;
	}

	bool removeInflection() {
		bool changed = false;

		if (inflectionCount > 0) {
			inflectionCount--;
			double_c newCenter = 0;
			if (inflectionCount == 0) {
				newCenter = inflectionCoords[0];
			}
			double oldAngle = rotation_angle;
			setRotation(0);
			setCenterAndZoomAbsolute(newCenter, inflectionZoomLevel*(1 / pow(2, inflectionCount)));
			setRotation(oldAngle);
			changed = true;
		}

		modifiedCalculations |= changed;
		return changed;
	}

	void printInflections() {
		cout << "inflectionPower: " << procedure.inflectionPower << endl;
		cout << "inflection coords:" << endl;
		for (uint i = 0; i < inflectionCount; i++) {
			printf("i=%d", i); printf(": %f ", real(inflectionCoords[i])); printf("+ %f * I\n", imag(inflectionCoords[i]));
		}
	}

private:
	bool initialized = false;

public:
	void reset(FractalParameters& defaultParameters) {
		setGradientSpeed(defaultParameters.get_gradientSpeed());
		setGradientOffset(defaultParameters.get_gradientOffset());
		setMaxIters(defaultParameters.get_maxIters());
		while (removeInflection());
		setRotation(0);
		setCenterAndZoomAbsolute(0, 0);
		setInflectionZoomLevel();
	}

	void initialize() {
		//These are default parameters to ensure that the struct is in a consistent state after creation.
		if (initialized) return;
		else initialized = true;

		modifiedCalculations = true;
		modifiedMemory = true;
		modifiedSize = true;
		modifiedColors = true;

		target_width = 1200;
		target_height = 800;
		oversampling = 1;
		bitmap_zoom = 1;
		rotation_angle = 0;
		rotation_factor = 1;
		procedure = M2;
		procedure_identifier = M2.id;
		julia = false;
		juliaSeed = -0.75 + 0.1*I;
		//inflectionCoords.resize(250); //already done in the constructor
		inflectionCount = 0;
		inflectionZoomLevel = 0;
		
		post_transformation_type = 0;
		pre_transformation_type = 0;
		gradientColors.resize(4);
		gradientColors[0] = rgb(255, 255, 255);
		gradientColors[1] = rgb(52, 140, 167);
		gradientColors[2] = rgb(0, 0, 0);
		gradientColors[3] = rgb(229, 140, 45);
		gradientSpeed = 1;
		gradientOffset = 0.5;
		partialInflectionCoord = 0;
		partialInflectionPower = 1;
		center = 0;

		setGradientSpeed(38.0);
		setGradientOffset(0);
		setMaxIters(4600);
		setCenterAndZoomPrivate(0, 0);
	}

	void fromParameters(const FractalParameters& P) {
		resize(P.get_target_width(), P.get_target_height(), P.get_oversampling(), P.get_bitmap_zoom());
		setRotation(P.get_rotation_angle());
		setProcedure(P.get_procedure_identifier());
		setJulia(P.get_julia());
		setJuliaSeed(P.get_juliaSeed());
		setInflections(P.get_inflectionCoords(), P.get_inflectionCount());
		setPostTransformation(P.get_post_transformation_type());
		setPreTransformation(P.get_pre_transformation_type());
		setGradientColors(P.get_gradientColors());
		setGradientSpeed(P.get_gradientSpeed());
		setGradientOffset(P.get_gradientOffset());
		setPartialInflectionCoord(P.get_partialInflectionCoord());
		setPartialInflectionPower(P.get_partialInflectionPower());
		setCenterAndZoomAbsolute(P.get_center(), P.get_zoomLevel());
		setMaxIters(P.get_maxIters());
		setInflectionZoomLevel(P.get_inflectionZoomLevel());
	}

	string toJson() const {
		using namespace rapidjson;

		//Create JSON object:
		Document document;
		document.Parse(" {} ");
		Document::AllocatorType& a = document.GetAllocator();

		//Add data:
		document.AddMember("programVersion", PROGRAM_VERSION, a);
		document.AddMember("oversampling", oversampling, a);
		document.AddMember("bitmap_zoom", bitmap_zoom, a);
		document.AddMember("target_width", target_width, a);
		document.AddMember("target_height", target_height, a);

		document.AddMember("rotation_angle", rotation_angle, a);

		{
			Value v(kObjectType);
			v.AddMember("Re", real(center), a);
			v.AddMember("Im", imag(center), a);
			document.AddMember("center", v, a);
		}

		document.AddMember("zoomLevel", get_zoomLevel(), a);
		document.AddMember("maxIters", maxIters, a);

		{
			Value v(kObjectType);
			v.AddMember("Re", real(juliaSeed), a);
			v.AddMember("Im", imag(juliaSeed), a);
			document.AddMember("juliaSeed", v, a);
		}

		document.AddMember("julia", julia, a);
		document.AddMember("procedure_identifier", procedure_identifier, a);
		document.AddMember("post_transformation_type", post_transformation_type, a);	
		document.AddMember("pre_transformation_type", pre_transformation_type, a);
		document.AddMember("inflectionCount", inflectionCount, a);

		document.AddMember("inflectionZoomLevel", inflectionZoomLevel, a);

		Value inflectionCoordsValue(kArrayType);
		for (uint i = 0; i < inflectionCount; i++) {
			Value v(kObjectType);
			v.AddMember("Re", real(inflectionCoords[i]), a);
			v.AddMember("Im", imag(inflectionCoords[i]), a);
			inflectionCoordsValue.PushBack(v, a);
		}
		document.AddMember("inflectionCoords", inflectionCoordsValue, a);

		document.AddMember("gradientSpeed", gradientSpeed, a);
		document.AddMember("gradientOffset", gradientOffset, a);
		Value gradientColorsValue(kArrayType);
		for (int i=0; i<gradientColors.size(); i++) {
			Value v(kObjectType);
			v.AddMember("r", getRValue(gradientColors[i]), a);
			v.AddMember("g", getGValue(gradientColors[i]), a);
			v.AddMember("b", getBValue(gradientColors[i]), a);
			gradientColorsValue.PushBack(v, a);
		}
		document.AddMember("gradientColors", gradientColorsValue, a);

		document.AddMember("partialInflectionPower", partialInflectionPower, a);
		{
			Value v(kObjectType);
			v.AddMember("Re", real(partialInflectionCoord), a);
			v.AddMember("Im", imag(partialInflectionCoord), a);
			document.AddMember("partialInflectionCoord", v, a);
		}

		//Get string
		StringBuffer sb;
		PrettyWriter<StringBuffer> writer(sb);
		document.Accept(writer);
		cout << sb.GetString() << endl;

		return sb.GetString();
	}

	bool fromJson(string& jsonString) {
		using namespace rapidjson;

		Document document;
		if (document.Parse(jsonString.c_str()).HasParseError()) {
			if(debug) cout << "parsing JSON failed" << endl;
			return false;
		}
		else {
			if(debug) cout << "jsonString is valid JSON" << endl;
			//read from JSON content:
			double programVersion_r = PROGRAM_VERSION;
			if (document.HasMember("programVersion"))
				programVersion_r = document["programVersion"].GetDouble();

			uint oversampling_r = oversampling;
			if (document.HasMember("oversampling"))
				oversampling_r = document["oversampling"].GetInt();

			uint bitmap_zoom_r = bitmap_zoom;
			if (document.HasMember("bitmap_zoom"))
				bitmap_zoom_r = document["bitmap_zoom"].GetInt();

			uint target_width_r = target_width;
			uint target_height_r = target_height;
			if (programVersion_r >= 10.0) {
				if (document.HasMember("target_width"))
					target_width_r = document["target_width"].GetInt();
				if (document.HasMember("target_height"))
					target_height_r = document["target_height"].GetInt();
			}
			else if (programVersion_r >= 6.0) {
				if (document.HasMember("screenWidth"))
					target_width_r = document["screenWidth"].GetInt();
				if (document.HasMember("screenHeight"))
					target_height_r = document["screenHeight"].GetInt();
			}
			else {
				if (document.HasMember("width"))
					target_width_r = document["width"].GetInt();
				if (document.HasMember("height"))
					target_height_r = document["height"].GetInt();
			}
			if(debug) cout << "fromJson width height oversampling: " << target_width_r << " " << target_height_r << " " << oversampling_r << " " << endl;
			

			double_c center_r = get_center();
			if (document.HasMember("center"))
				center_r = document["center"]["Re"].GetDouble() + document["center"]["Im"].GetDouble() * I;
			double zoom_r = get_zoomLevel();
			if (document.HasMember("zoomLevel"))
				zoom_r = document["zoomLevel"].GetDouble();

			uint maxIters_r = maxIters;
			if (document.HasMember("maxIters")) {
				maxIters_r = document["maxIters"].GetInt();
			}

			double_c juliaSeed_r = juliaSeed;
			if (document.HasMember("juliaSeed"))
				juliaSeed_r = document["juliaSeed"]["Re"].GetDouble() + document["juliaSeed"]["Im"].GetDouble() * I;
			bool julia_r = get_julia();
			if (document.HasMember("julia"))
				julia_r = document["julia"].GetBool();

			int procedure_identifier_r = procedure.id;
			if (programVersion_r >= 9.0) {
				if (document.HasMember("procedure_identifier"))
					procedure_identifier_r = document["procedure_identifier"].GetInt();
			}
			else {
				if (document.HasMember("formula_identifier"))
					procedure_identifier_r = document["formula_identifier"].GetInt();			
			}

			int post_transformation_type_r = post_transformation_type;
			int pre_transformation_type_r = pre_transformation_type;
			if (programVersion_r >= 6.0) {
				if (document.HasMember("post_transformation_type"))
					post_transformation_type_r = document["post_transformation_type"].GetInt();
				if (document.HasMember("pre_transformation_type"))
					pre_transformation_type_r = document["pre_transformation_type"].GetInt();
			}
			else {
				if (document.HasMember("transformation_type"))
					post_transformation_type_r = document["transformation_type"].GetInt();
				pre_transformation_type_r = 0;
			}

			double_c partialInflectionCoord_r = partialInflectionCoord;
			double partialInflectionPower_r = partialInflectionPower;
			if (document.HasMember("partialInflectionCoord") && document.HasMember("partialInflectionPower")) {
				partialInflectionCoord_r = document["partialInflectionCoord"]["Re"].GetDouble() + document["partialInflectionCoord"]["Im"].GetDouble() * I;
				partialInflectionPower_r = document["partialInflectionPower"].GetDouble();
			}

			double gradientSpeed_r = gradientSpeed;
			if (document.HasMember("gradientSpeed"))
				gradientSpeed_r = document["gradientSpeed"].GetDouble();
			double gradientOffset_r = gradientOffset;
			if (document.HasMember("gradientSpeed"))
				gradientOffset_r = document["gradientOffset"].GetDouble();

			double inflectionZoomlevel_r = inflectionZoomLevel;
			if (document.HasMember("inflectionZoomLevel"))
				inflectionZoomlevel_r = document["inflectionZoomLevel"].GetDouble();

			double rotation_angle_r = rotation_angle;
			if (document.HasMember("rotation_angle")) {
				rotation_angle_r = document["rotation_angle"].GetDouble();
			}
			else if (programVersion_r < 7) {
				//Older versions always have angle 0 because there was no rotation yet.
				rotation_angle_r = 0;
			}
			
			resize(target_width_r, target_height_r, oversampling_r, bitmap_zoom_r);
			setCenterAndZoomAbsolute(center_r, zoom_r);
			setMaxIters(maxIters_r);
			setJuliaSeed(juliaSeed_r);
			setJulia(julia_r);
			setProcedure(procedure_identifier_r);
			setPostTransformation(post_transformation_type_r);
			setPreTransformation(pre_transformation_type_r);
			setPartialInflectionCoord(partialInflectionCoord_r);
			setPartialInflectionPower(partialInflectionPower_r);
			setGradientSpeed(gradientSpeed_r);
			setGradientOffset(gradientOffset_r);
			setInflectionZoomLevel(inflectionZoomlevel_r);
			center_of_rotation = center; //dontdo: assignment instead of using a function, doesn't update modified~ bools
			setRotation(rotation_angle_r);
			
			if (document.HasMember("inflectionCoords")) {
				Value& inflectionCoordsv = document["inflectionCoords"];

				uint inflectionCount_r = inflectionCoordsv.Size();
				vector<double_c> inflections_r(inflectionCount_r);

				for (uint i = 0; i < inflectionCount_r; i++) {
					inflections_r[i] = inflectionCoordsv[i]["Re"].GetDouble() + inflectionCoordsv[i]["Im"].GetDouble() * I;
				}

				setInflections(inflections_r, inflectionCount_r);
			}

			if (document.HasMember("gradientColors")) {
				Value& gradientColorsValue = document["gradientColors"];

				uint size = gradientColorsValue.Size();
				vector<ARGB> colors(size);

				for (uint i=0; i<size; i++) {
					colors[i] = rgb(
						gradientColorsValue[i]["r"].GetInt()
						,gradientColorsValue[i]["g"].GetInt()
						,gradientColorsValue[i]["b"].GetInt()
					);
				}

				setGradientColors(colors);
			}
			else if (programVersion_r < 6.1) {
				//set the gradient that was hardcoded in the previous versions
				vector<ARGB> colors(4);
				colors[0] = rgb(255, 255, 255);
				colors[1] = rgb(52, 140, 167);
				colors[2] = rgb(0, 0, 0);
				colors[3] = rgb(229, 140, 45);

				setGradientColors(colors);
			}
		}

		if(debug) {
			cout << "finished setting parameters from json string" << endl;
			cout << "result:" << endl;
			cout << toJson() << endl;
		}
		
		return true;
	}
};

#endif