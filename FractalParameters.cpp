//Visual Studio requires this for no clear reason
#include "stdafx.h"

#include <vector>

#include <Windows.h> //for the RGB macro

//rapidjson
#include "rapidjson/document.h"     // rapidjson's DOM-style API
#include "rapidjson/prettywriter.h" // for stringify JSON

#include "common.cpp"


#ifndef _FractalParameters_
#define _FractalParameters_

using namespace std;

#define get_trick(name) get_ ## name
#define readonly(type, name) \
 private: type name; \
 public: inline type get_trick(name)() {\
        return name;\
 }

class FractalParameters {

	readonly(int, screenWidth)
	readonly(int, screenHeight)
	readonly(int, oversampling)
	inline int get_height() {
		return screenHeight * oversampling;
	}
	inline int get_width() {
		return screenWidth * oversampling;
	}
	readonly(double, rotation)
	readonly(double_c, center)
	readonly(double_c, topleftCorner)
	readonly(double, x_range)
	readonly(double, y_range)
	readonly(double, pixelWidth)
	readonly(double, pixelHeight)
	readonly(int, maxIters)
	readonly(bool, julia)
	readonly(Formula, formula)
	readonly(int, formula_identifier);
		
	readonly(vector<double_c>, inflectionCoords)
	readonly(int, inflectionCount)
	readonly(double, inflectionZoomLevel)

	readonly(double, gradientSpeed)
	readonly(double, gradientOffset)
	readonly(double, gradientSpeedFactor)
	readonly(int, gradientOffsetTerm)

	vector<uint> gradientColors;

public:
	
	double partialInflectionPower;
	double_c partialInflectionCoord;
	int post_transformation_type;
	int pre_transformation_type;
	double_c juliaSeed;

	double transferFunction(double gradientSpeedd) {
		return pow(1.1, gradientSpeedd - 1);
	}

	bool setGradientOffset(double newGradientOffset) {
		bool changed = false;
		if (newGradientOffset >= 0.0 && newGradientOffset <= 1.0) {
			changed = gradientOffset != newGradientOffset;
			gradientOffset = newGradientOffset;
			int interpolatedGradientLength = gradientColors.size() * transferFunction(gradientSpeed);
			gradientOffsetTerm = interpolatedGradientLength * gradientOffset;
		}
		return changed;
	}

	bool setGradientSpeed(double newGradientSpeed) {
		bool changed = false;
		if (newGradientSpeed >= 0.0) {
			changed = newGradientSpeed != gradientSpeed;
			gradientSpeed = newGradientSpeed;
			double computedGradientSpeed = transferFunction(gradientSpeed);
			gradientSpeedFactor = 1.0 / computedGradientSpeed;
			gradientOffsetTerm = gradientColors.size() * computedGradientSpeed * gradientOffset;
		}
		return changed;
	}

	bool setGradientColors(vector<uint>& colors) {
		bool changed = false;
		if (gradientColors.size() != colors.size()) {
			gradientColors.resize(colors.size());
			changed = true;
		}
		for (int i=0; i<colors.size(); i++) {
			uint& old = gradientColors[i];
			uint& new_ = colors[i];
			if (old != new_)
				changed = true;
			old = new_;
		}
		return changed;
	}

	double getZoomLevel() {
		return -log2(x_range) + 2;
	}

	bool setMaxIters(int newMaxIters) {
		if (newMaxIters < 1 || newMaxIters == maxIters)
			return false;
		maxIters = newMaxIters;
		return true;
	}

	bool setCenterAndZoom(double_c newCenter, double zoom) {
		//Set both of these settings together because the zoom level, topleftCorner coordinate and pixel size are related.
		int width = get_width();
		int height = get_height();

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
		//This order is important:
		recalcRequired |= setRenderRange(zoom);
		recalcRequired |= setCoordinates(newCenter);
		recalcRequired |= updatePixelSize();

		return recalcRequired;
	}

private:
	bool updateCenterAndZoom() {
		return setCenterAndZoom(center, getZoomLevel());
	}

public:
	void toggleJulia() {
		julia = !julia;
		if (julia) {
			juliaSeed = center;
			setCenterAndZoom(0, 0);
		}
		else {
			setCenterAndZoom(juliaSeed, 0);
		}
	}
	
	bool changeFormula(int identifier) {
		Formula newFormula = getFormulaObject(identifier);
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

	inline double_c map(int xPos, int yPos) {
		return get_topleftCorner() + xPos * get_pixelWidth() - yPos * get_pixelHeight()*I;
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
		inflectionZoomLevel = getZoomLevel()*power;
	}

	bool addInflection(double_c c) {
		while (inflectionCount >= inflectionCoords.size()) {
			cout << "inflectionCount: " << inflectionCount << "    inflectionCoords.size(): " << inflectionCoords.size() << endl;
			inflectionCoords.resize(inflectionCoords.size() * 2 + 1);
		}
		
		inflectionCoords[inflectionCount] = c;
		inflectionCount++;
		setCenterAndZoom(0, inflectionZoomLevel*(1 / pow(2, inflectionCount)));
		return true;
	}

	bool addInflection(int xPos, int yPos) {
		if (xPos < 0 || xPos > get_width() || yPos < 0 || yPos > get_height()) {
			return false;
		}
		double_c thisInflectionCoord = map(xPos, yPos);
		return addInflection(thisInflectionCoord);
	}

	bool removeInflection(int xPos, int yPos) {
		if (xPos < 0 || xPos > get_width() || yPos < 0 || yPos > get_height()) {
			return false;
		}
		if (inflectionCount > 0) {
			inflectionCount--;
			double_c newCenter = 0;
			if (inflectionCount == 0) {
				newCenter = inflectionCoords[0];
			}
			setCenterAndZoom(newCenter, inflectionZoomLevel*(1 / pow(2, inflectionCount)));
			return true;
		}
		return false;
	}

	void printInflections() {
		cout << "inflectionPower: " << formula.inflectionPower << endl;
		cout << "inflection coords:" << endl;
		for (int i = 0; i < inflectionCount; i++) {
			printf("i=%d", i); printf(": %f ", real(inflectionCoords[i])); printf("+ %f * I\n", imag(inflectionCoords[i]));
		}
	}

private:
	bool initialized = false;

public:
	void reset() {
		setGradientSpeed(38.0);
		setGradientOffset(0);
		setMaxIters(4600);
		while (removeInflection(0, 0));
		setCenterAndZoom(0, 0);
		setInflectionZoomLevel();
	}

	void initialize() {
		//These are default parameters to ensure that the struct is in a consistent state after creation.
		if (initialized) return;
		else initialized = true;

		this->oversampling = 1;
		this->screenWidth = 1200;
		this->screenHeight = 800;
		this->formula = M2;
		this->formula_identifier = M2.identifier;
		this->julia = false;
		this->juliaSeed = -0.75 + 0.1*I;
		this->inflectionCoords.resize(250); //initial capacity
		this->inflectionCount = 0;
		this->inflectionZoomLevel = 0;
		setCenterAndZoom(0, 0);
		this->gradientColors.resize(4);
		gradientColors[0] = RGB(255, 255, 255);
		gradientColors[1] = RGB(167, 140, 52);
		gradientColors[2] = RGB(0, 0, 0);
		gradientColors[3] = RGB(45, 140, 229);
		this->gradientSpeed = 1;
		this->gradientOffset = 0.5;
		setGradientSpeed(38.0);
		setGradientOffset(0);
		this->post_transformation_type = 0;
		this->pre_transformation_type = 0;
		setMaxIters(4600);
		
		this->rotation = 0; //not implemented
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
		document.AddMember("rotation", rotation, a);

		Value centerv(kObjectType);
		centerv.AddMember("Re", real(center), a);
		centerv.AddMember("Im", imag(center), a);
		document.AddMember("center", centerv, a);

		document.AddMember("zoomLevel", getZoomLevel(), a);
		document.AddMember("maxIters", maxIters, a);

		Value juliaSeedv(kObjectType);
		juliaSeedv.AddMember("Re", real(juliaSeed), a);
		juliaSeedv.AddMember("Im", imag(juliaSeed), a);
		document.AddMember("juliaSeed", juliaSeedv, a);

		document.AddMember("julia", julia, a);
		document.AddMember("formula_identifier", formula.identifier, a);
		document.AddMember("post_transformation_type", post_transformation_type, a);	
		document.AddMember("pre_transformation_type", pre_transformation_type, a);
		document.AddMember("inflectionCount", inflectionCount, a);

		Value inflectionCoordsv(kArrayType);
		for (int i = 0; i < inflectionCount; i++) {
			Value thisCoordv(kObjectType);
			thisCoordv.AddMember("Re", real(inflectionCoords[i]), a);
			thisCoordv.AddMember("Im", imag(inflectionCoords[i]), a);
			inflectionCoordsv.PushBack(thisCoordv, a);
		}
		document.AddMember("inflectionCoords", inflectionCoordsv, a);

		Value gradientColorsv(kArrayType);
		for (int i=0; i<gradientColors.size(); i++) {
			Value thisColorv(kObjectType);
			//GetBValue actually gives the r value. It's not a mistake. I want to improve this.
			thisColorv.AddMember("r", GetBValue(gradientColors[i]), a);
			thisColorv.AddMember("g", GetGValue(gradientColors[i]), a);
			thisColorv.AddMember("b", GetRValue(gradientColors[i]), a);
			gradientColorsv.PushBack(thisColorv, a);
		}
		document.AddMember("gradientColors", gradientColorsv, a);

		document.AddMember("gradientSpeed", gradientSpeed, a);
		document.AddMember("gradientOffset", gradientOffset, a);
		document.AddMember("inflectionZoomLevel", inflectionZoomLevel, a);

		//Get string
		StringBuffer sb;
		PrettyWriter<StringBuffer> writer(sb);
		document.Accept(writer);
		cout << sb.GetString() << endl;

		return sb.GetString();
	}

	bool fromJson(string jsonString) {
		using namespace rapidjson;

		Document document;
		if (document.Parse(jsonString.c_str()).HasParseError()) {
			if(debug) cout << "parsing JSON failed" << endl;
			return false;
		}
		else {
			//read from JSON content:
			double programVersion_r = document["programVersion"].GetDouble();

			int oversampling_r = document["oversampling"].GetInt();

			if (programVersion_r >= 6.0) {
				int width_r = document["screenWidth"].GetInt();
				int height_r = document["screenHeight"].GetInt();
				if(debug) cout << "fromJson height width oversampling" << height_r << " " << width_r << " " << oversampling_r << " " << endl;
				resize(oversampling_r, width_r, height_r);
			}
			else {
				int width_r = document["width"].GetInt();
				int height_r = document["height"].GetInt();
				if(debug) cout << "fromJson height width oversampling" << height_r << " " << width_r << " " << oversampling_r << " " << endl;
				resize(oversampling_r, width_r, height_r);
			}

			double_c center_r = document["center"]["Re"].GetDouble() + document["center"]["Im"].GetDouble() * I;
			double zoom_r = document["zoomLevel"].GetDouble();
			setCenterAndZoom(center_r, zoom_r);

			int maxIters_r = document["maxIters"].GetInt();
			setMaxIters(maxIters_r);

			double_c juliaSeed_r = document["juliaSeed"]["Re"].GetDouble() + document["juliaSeed"]["Im"].GetDouble() * I;
			bool julia_r = document["julia"].GetBool();
			juliaSeed = juliaSeed_r;
			julia = julia_r;

			int formula_identifier_r = document["formula_identifier"].GetInt();
			changeFormula(formula_identifier_r);

			if (programVersion_r >= 6.0) {
				int post_transformation_type_r = document["post_transformation_type"].GetInt();
				int pre_transformation_type_r = document["pre_transformation_type"].GetInt();
				post_transformation_type = post_transformation_type_r;
				pre_transformation_type = pre_transformation_type_r;
			}
			else {
				int post_transformation_type_r = document["transformation_type"].GetInt();
				post_transformation_type = post_transformation_type_r;
				pre_transformation_type = 0;
			}
			
			int inflectionCount_r = document["inflectionCount"].GetInt();
			inflectionCount = inflectionCount_r;
			Value& inflectionCoordsv = document["inflectionCoords"];
			for (int i = 0; i < inflectionCount_r; i++) {
				inflectionCoords[i] = inflectionCoordsv[i]["Re"].GetDouble() + inflectionCoordsv[i]["Im"].GetDouble() * I;
			}

			if (programVersion_r >= 6.1) {
				//older versions don't have colors in the parameters
				Value& gradientColorsv = document["gradientColors"];
				int size = gradientColorsv.Size();
				gradientColors.resize(size);
				for (int i=0; i<size; i++) {
					gradientColors[i] = RGB(
						gradientColorsv[i]["b"].GetInt()
						,gradientColorsv[i]["g"].GetInt()
						,gradientColorsv[i]["r"].GetInt()
					);
				}
			}

			double gradientSpeed_r = document["gradientSpeed"].GetDouble();
			double gradientOffset_r = document["gradientOffset"].GetDouble();

			setGradientSpeed(gradientSpeed_r);
			setGradientOffset(gradientOffset_r);

			double inflectionZoomlevel_r = document["inflectionZoomLevel"].GetDouble();
			inflectionZoomLevel = inflectionZoomlevel_r;
		}
		return true;
	}

	bool resize(int newOversampling, int newScreenWidth, int newScreenHeight) {
		assert(newOversampling > 0);
		assert(newScreenWidth >= 0);
		assert(newScreenHeight >= 0);
		if (oversampling != newOversampling || screenWidth != newScreenWidth || screenHeight != newScreenHeight) {
			oversampling = newOversampling;
			screenWidth = newScreenWidth;
			screenHeight = newScreenHeight;
			return updateCenterAndZoom();
		}
		return false;
	}
};

#endif