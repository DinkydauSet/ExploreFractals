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
 public: inline type get_trick(name)() {\
        return name;\
 }

 namespace BI_choices {
	enum {
		DEPARTMENT_SALES
		,DEPARTMENT_IT
		,DEPARTMENT_MT
		,DEPARTMENT_RND
	};
 }

class FractalParameters {

	public: FractalParameters() {
		initialize();
	}

	readonly(uint, screenWidth)
	readonly(uint, screenHeight)
	readonly(uint, oversampling)
	inline uint get_height() {
		return screenHeight * oversampling;
	}
	inline uint get_width() {
		return screenWidth * oversampling;
	}
	readonly(double_c, center) //This is the center in the untransformed complex plane. The coordinate in the center of the viewport can be different because of rotation, inflections etc. Generally this touches on how the program does transformations. The viewport is first identified with a rectangular region in the complex plane, which is then transformed in various ways. This is the center of the starting region. The same goes for the topleftCorner, x_range and y_range.
	readonly(double_c, topleftCorner)
	readonly(double, x_range)
	readonly(double, y_range)
	readonly(double, pixelWidth)
	readonly(double, pixelHeight)
	readonly(uint, maxIters)
	readonly(bool, julia)
	readonly(Formula, formula)
	readonly(int, formula_identifier)
		
	public: vector<double_c> inflectionCoords;//the locations of created Julia morphings
	public: inline vector<double_c> get_inflectionCoords() {return inflectionCoords;} 

	readonly(uint, inflectionCount)
	readonly(double, inflectionZoomLevel) //reset to this zoom level upon creating a new inflection

	readonly(double, gradientSpeed) //a measure how much the gradient is stretched (or shrunk, when it's negative)
	readonly(double, gradientOffset)
	readonly(double, gradientSpeedFactor) //stored for efficiency
	readonly(uint, gradientOffsetTerm) //stored for efficiency

	vector<ARGB> gradientColors;

	readonly(double, rotation_angle) //angle between 0 and 1. 0 means 0 degrees. 0.25 means 90 degrees etc.
	readonly(double_c, center_of_rotation)
	readonly(double_c, rotation_factor) //This is for complex number rotation, which works by multiplying with an element of the unit circle. The number is stored for efficiency because it's used a lot. It can be computed anytime as exp(angle * 2*pi*I).

public:

	double partialInflectionPower;
	double_c partialInflectionCoord;
	int post_transformation_type;
	int pre_transformation_type;
	double_c juliaSeed;
	int BI_choice;

	double transferFunction(double gradientSpeed_) {
		return pow(1.1, gradientSpeed_ - 1);
	}

	bool setGradientOffset(double newGradientOffset) {
		bool changed = false;
		assert(newGradientOffset >= 0.0 && newGradientOffset <= 1.0);
		if (newGradientOffset >= 0.0 && newGradientOffset <= 1.0) {
			changed = gradientOffset != newGradientOffset;
			gradientOffset = newGradientOffset;
			uint interpolatedGradientLength = gradientColors.size() * transferFunction(gradientSpeed);
			gradientOffsetTerm = interpolatedGradientLength * gradientOffset;
		}
		return changed;
	}

	bool setGradientSpeed(double newGradientSpeed) {
		bool changed = newGradientSpeed != gradientSpeed;
		gradientSpeed = newGradientSpeed;
		double computedGradientSpeed = transferFunction(gradientSpeed);
		gradientSpeedFactor = 1.0 / computedGradientSpeed;
		assert(gradientSpeedFactor > 0);
		gradientOffsetTerm = gradientColors.size() * computedGradientSpeed * gradientOffset;
		return changed;
	}

	bool setGradientColors(vector<ARGB>& colors) {
		bool changed = false;
		if (gradientColors.size() != colors.size()) {
			gradientColors.resize(colors.size());
			changed = true;
		}
		for (int i=0; i<colors.size(); i++) {
			ARGB& old = gradientColors[i];
			ARGB& new_ = colors[i];
			if (old != new_)
				changed = true;
			old = new_;
		}
		return changed;
	}

	double get_zoomLevel() {
		return -log2(x_range) + 2;
	}

	bool setMaxIters(uint newMaxIters) {
		assert(newMaxIters > 0);
		if (newMaxIters <= 0 || newMaxIters == maxIters)
			return false;
		maxIters = newMaxIters;
		return true;
	}

private:
	/*
		The private version of this function does the actual work. The public versions call this function and in addition re-center the rotation. The problem with letting setCenterAndZoomPrivate re-center the rotation is that setRotation also calls setCenterAndZoomPrivate. It would cause infinite recursion.
	*/
	bool setCenterAndZoomPrivate(double_c newCenter, double zoom) {
		//Set both of these settings together because the zoom level, topleftCorner coordinate and pixel size are related.
		uint width = get_width();
		uint height = get_height();

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

		auto updatePixelSize = [&]() {
			double newPixelWidth = x_range / width;
			double newPixelHeight = y_range / height;
			if (newPixelHeight != pixelHeight || newPixelWidth != pixelWidth) {
				pixelWidth = newPixelWidth;
				pixelHeight = newPixelHeight;
				return true;
			}
			return false;
		};

		bool recalcRequired = false;

		//setRenderRange should be done first because it changes the x_range and y_range that the following two actions use.
		recalcRequired |= setRenderRange(zoom);
		recalcRequired |= setCoordinates(newCenter);
		recalcRequired |= updatePixelSize();

		assert(x_range > 0);
		assert(y_range > 0);

		return recalcRequired;
	}


public:

	bool setCenterAndZoomAbsolute(double_c newCenter, double zoom) {
		double old_angle = rotation_angle;
		setRotation(0);
		bool res = setCenterAndZoomPrivate(newCenter, zoom);
		setRotation(old_angle);
		return res;
	}

	bool setCenterAndZoomRelative(double_c newCenter, double zoom) {
		bool res = setCenterAndZoomPrivate(newCenter, zoom);
		setRotation(rotation_angle); //change the rotation to use the new center as its center of rotation, but not changing the angle
		return res;
	}

	bool setCenter(double_c newCenter) {
		return setCenterAndZoomAbsolute(newCenter, get_zoomLevel());
	}

	bool setZoomLevel(double zoomLevel) {
		return setCenterAndZoomAbsolute(center, zoomLevel);
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

	void setRotation(double angle) {
		//There may be an existing rotation. This is the center of the viewport under that rotation:
		double_c current_center = rotation(center);
		//move to that location
		setCenterAndZoomPrivate(current_center, get_zoomLevel());
		//Using the center as the center of rotation makes sense for the user. Here also the new angle is set and the rotation_factor recomputed:
		center_of_rotation = center;
		rotation_angle = normalize_angle(angle);
		rotation_factor = exp(rotation_angle * 2*pi*I);
	}

	inline double_c map(uint xPos, uint yPos) {
		assert(xPos >= 0); assert(xPos <= get_width());
		assert(yPos >= 0); assert(yPos <= get_height());
		return get_topleftCorner() + xPos * get_pixelWidth() - yPos * get_pixelHeight()*I;
	}

	inline double_c rotation(double_c c) {
		return (c - center_of_rotation) * get_rotation_factor() + center_of_rotation;
	}

	inline double_c pre_transformation(double_c c) {
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

	double_c inflections(double_c c) {
		for (int i = inflectionCount - 1;  i>=0;  i--) {
			c = pow(c, formula.inflectionPower) + inflectionCoords[i];
		}
		return c;
	}

	inline double_c post_transformation(double_c c) {
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

	double_c map_with_transformations(double_c c) {
		return
			post_transformation(
			inflections(
			pre_transformation(
			rotation(c))));
	}

	double_c map_with_transformations(int x, int y) {
		return map_with_transformations(map(x, y));
	}

private:
	bool updateCenterAndZoom() {
		return setCenterAndZoomRelative(center, get_zoomLevel());
	}

public:
	bool resize(uint newOversampling, uint newScreenWidth, uint newScreenHeight) {
		assert(newOversampling > 0);
		assert(newScreenWidth > 0);
		assert(newScreenHeight > 0);
		if (oversampling != newOversampling || screenWidth != newScreenWidth || screenHeight != newScreenHeight) {
			oversampling = newOversampling;
			screenWidth = newScreenWidth;
			screenHeight = newScreenHeight;
			updateCenterAndZoom(); //this causes recalculation of the pixel width and some other things
			return true;
		}
		return false;
	}

	void toggleJulia() {
		julia = !julia;
		if (julia) {
			juliaSeed = map_with_transformations(center);
			setCenterAndZoomAbsolute(0, 0);
		}
		else {
			setCenterAndZoomAbsolute(juliaSeed, 0);
		}
	}
	
	bool changeFormula(int identifier) {
		Formula newFormula = getFormulaObject(identifier);
		assert(newFormula.identifier != -1);
		if (newFormula.identifier == -1) return false; //The idenfitifer was not found by getFormulaObject
		if (formula.identifier != newFormula.identifier) {
			formula_identifier = newFormula.identifier;
			formula = newFormula;
			return true;
		}
		return false;
	}
	

	void changeTransformation() {
		post_transformation_type = (post_transformation_type + 1) % NUMBER_OF_TRANSFORMATIONS;
	}

	void setInflectionZoomLevel() {
		/*
			Normally when an inflection is set the zoom level gets reset to 0.
			This sets that "reset zoom level" to the current zoom level.

			However, addInflection uses a correction factor of 1/2 for every inflection set, to keep the pattern the same size.
			The reason is: An inflection halves the distance (the power of the magnification factor) to stuff lying deeper below.

			inflectionZoomLevel is used as a base zoom level (no correction factor applied).
			Therefore to derive it from the current zoom level, the correction needs to be undone (by multiplying by power).
		*/
		double power = pow(2, inflectionCount);
		inflectionZoomLevel = get_zoomLevel()*power;
	}

	void addInflection(double_c c) {
		while (inflectionCount >= inflectionCoords.size()) {
			cout << "inflectionCount: " << inflectionCount << "    inflectionCoords.size(): " << inflectionCoords.size() << endl;
			inflectionCoords.resize(inflectionCoords.size() * 2 + 1);
		}
		inflectionCoords[inflectionCount] = c;
		inflectionCount++;
		double oldAngle = rotation_angle;
		setRotation(0);
		setCenterAndZoomAbsolute(0, inflectionZoomLevel*(1 / pow(2, inflectionCount)));
		setRotation(oldAngle);
	}

	void addInflection(uint xPos, uint yPos) {
		assert( ! (xPos < 0 || xPos > get_width() || yPos < 0 || yPos > get_height()) );
		double_c thisInflectionCoord = pre_transformation(rotation(map(xPos, yPos)));
		addInflection(thisInflectionCoord);
	}

	bool removeInflection() {
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
			return true;
		}
		return false;
	}

	void printInflections() {
		cout << "inflectionPower: " << formula.inflectionPower << endl;
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

		oversampling = 1;
		screenWidth = 1200;
		screenHeight = 800;
		rotation_angle = 0;
		rotation_factor = 1;
		formula = M2;
		formula_identifier = M2.identifier;
		julia = false;
		juliaSeed = -0.75 + 0.1*I;
		inflectionCoords.resize(250); //initial capacity
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
		BI_choice = BI_choices::DEPARTMENT_IT;
		partialInflectionCoord = 0;
		partialInflectionPower = 1;
		center = 0;

		setGradientSpeed(38.0);
		setGradientOffset(0);
		setMaxIters(4600);
		setCenterAndZoomPrivate(0, 0);
	}

	string toJson() {
		using namespace rapidjson;

		//Create JSON object:
		Document document;
		document.Parse(" {} ");
		Document::AllocatorType& a = document.GetAllocator();

		//Add data:
		document.AddMember("programVersion", PROGRAM_VERSION, a);
		document.AddMember("oversampling", oversampling, a);
		document.AddMember("screenWidth", screenWidth, a);
		document.AddMember("screenHeight", screenHeight, a);

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
		document.AddMember("formula_identifier", formula.identifier, a);
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

			uint oversampling_r = get_oversampling();
			if (document.HasMember("oversampling"))
				oversampling_r = document["oversampling"].GetInt();

			uint screenWidth_r = get_screenWidth();
			uint screenHeight_r = get_screenHeight();
			if (programVersion_r >= 6.0) {
				if (document.HasMember("screenWidth"))
					screenWidth_r = document["screenWidth"].GetInt();
				if (document.HasMember("screenHeight"))
					screenHeight_r = document["screenHeight"].GetInt();
			}
			else {
				if (document.HasMember("width"))
					screenWidth_r = document["width"].GetInt();
				if (document.HasMember("height"))
					screenHeight_r = document["height"].GetInt();
			}
			if(debug) cout << "fromJson width height oversampling: " << screenWidth_r << " " << screenHeight_r << " " << oversampling_r << " " << endl;
			resize(oversampling_r, screenWidth_r, screenHeight_r);

			double_c center_r = get_center();
			if (document.HasMember("center"))
				center_r = document["center"]["Re"].GetDouble() + document["center"]["Im"].GetDouble() * I;
			double zoom_r = get_zoomLevel();
			if (document.HasMember("zoomLevel"))
				zoom_r = document["zoomLevel"].GetDouble();
			setCenterAndZoomAbsolute(center_r, zoom_r);

			uint maxIters_r = maxIters;
			if (document.HasMember("maxIters")) {
				maxIters_r = document["maxIters"].GetInt();
			}
			setMaxIters(maxIters_r);

			double_c juliaSeed_r = juliaSeed;
			if (document.HasMember("juliaSeed"))
				juliaSeed_r = document["juliaSeed"]["Re"].GetDouble() + document["juliaSeed"]["Im"].GetDouble() * I;
			bool julia_r = get_julia();
			if (document.HasMember("julia"))
				julia_r = document["julia"].GetBool();
			juliaSeed = juliaSeed_r;
			julia = julia_r;

			int formula_identifier_r = formula.identifier;
			if (document.HasMember("formula_identifier"))
				formula_identifier_r = document["formula_identifier"].GetInt();
			changeFormula(formula_identifier_r);

			int post_transformation_type_r = post_transformation_type;
			int pre_transformation_type_r = pre_transformation_type;
			if (programVersion_r >= 6.0) {
				if (document.HasMember("post_transformation_type"))
					post_transformation_type_r = document["post_transformation_type"].GetInt();
				if (document.HasMember("pre_transformation_type"))
					pre_transformation_type_r = document["pre_transformation_type"].GetInt();
				post_transformation_type = post_transformation_type_r;
				pre_transformation_type = pre_transformation_type_r;
			}
			else {
				if (document.HasMember("transformation_type"))
					post_transformation_type_r = document["transformation_type"].GetInt();
				post_transformation_type = post_transformation_type_r;
				pre_transformation_type = 0;
			}

			if (document.HasMember("partialInflectionCoord") && document.HasMember("partialInflectionPower")) {
				double_c partialInflectionCoord_r = document["partialInflectionCoord"]["Re"].GetDouble() + document["partialInflectionCoord"]["Im"].GetDouble() * I;
				double partialInflectionPower_r = document["partialInflectionPower"].GetDouble();
				
				partialInflectionCoord = partialInflectionCoord_r;
				partialInflectionPower = partialInflectionPower_r;
			}
			
			if (document.HasMember("inflectionCoords")) {
				Value& inflectionCoordsv = document["inflectionCoords"];

				uint inflectionCount_r = inflectionCoordsv.Size();
				inflectionCount = inflectionCount_r;

				if (inflectionCoords.size() < inflectionCount) {
					inflectionCoords.resize(inflectionCount);
				}
				for (uint i = 0; i < inflectionCount_r; i++) {
					inflectionCoords[i] = inflectionCoordsv[i]["Re"].GetDouble() + inflectionCoordsv[i]["Im"].GetDouble() * I;
				}
			}

			if (document.HasMember("gradientColors")) {
				Value& gradientColorsValue = document["gradientColors"];
				uint size = gradientColorsValue.Size();
				gradientColors.resize(size);
				for (uint i=0; i<size; i++) {
					gradientColors[i] = rgb(
						gradientColorsValue[i]["r"].GetInt()
						,gradientColorsValue[i]["g"].GetInt()
						,gradientColorsValue[i]["b"].GetInt()
					);
				}
			}
			else if (programVersion_r < 6.1) {
				//set the gradient that was hardcoded in the previous versions
				gradientColors.resize(4);
				gradientColors[0] = rgb(255, 255, 255);
				gradientColors[1] = rgb(52, 140, 167);
				gradientColors[2] = rgb(0, 0, 0);
				gradientColors[3] = rgb(229, 140, 45);
			}

			double gradientSpeed_r = gradientSpeed;
			if (document.HasMember("gradientSpeed"))
				gradientSpeed_r = document["gradientSpeed"].GetDouble();
			double gradientOffset_r = gradientOffset;
			if (document.HasMember("gradientSpeed"))
				gradientOffset_r = document["gradientOffset"].GetDouble();

			setGradientSpeed(gradientSpeed_r);
			setGradientOffset(gradientOffset_r);

			double inflectionZoomlevel_r = inflectionZoomLevel;
			if (document.HasMember("inflectionZoomLevel"))
				inflectionZoomlevel_r = document["inflectionZoomLevel"].GetDouble();
			inflectionZoomLevel = inflectionZoomlevel_r;

			double rotation_angle_r = rotation_angle;
			if (document.HasMember("rotation_angle")) {
				rotation_angle_r = document["rotation_angle"].GetDouble();
			}
			else if (programVersion_r < 7) {
				//Older versions always have angle 0 because there was no rotation yet.
				rotation_angle_r = 0;
			}
			center_of_rotation = center;
			setRotation(rotation_angle_r);
		}
		cout << "finished setting parameters from json string" << endl;
		if(debug) {
			cout << "result:" << endl;
			cout << toJson() << endl;
		}
		
		return true;
	}
};

#endif