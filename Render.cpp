//this program
#include "common.cpp"
#include "FractalCanvas.cpp"



#ifndef _Render_
//#define _Render_

using namespace std;

const bool CALCULATED = false;
const bool GUESSED = true;

struct box {
	double xfrom;
	double xto;
	double yfrom;
	double yto;
};

//constants for Triple Matchmaker:
const double sqrt3 = sqrt(3);
const double a = 2.2;
const double b = 1.4;
const double d = 1.1;


template <int formula_identifier>
double_c escapeTimeFormula(double_c z, double_c c) { return 0; };  //default implementation

template <>
double_c escapeTimeFormula<PROCEDURE_M3>(double_c z, double_c c) {
	return pow(z, 3) + c;
}

template <>
double_c escapeTimeFormula<PROCEDURE_M4>(double_c z, double_c c) {
	return pow(z, 4) + c;
}

template <>
double_c escapeTimeFormula<PROCEDURE_M5>(double_c z, double_c c) {
	return pow(z, 5) + c;
}

template <>
double_c escapeTimeFormula<PROCEDURE_BURNING_SHIP>(double_c z, double_c c) {
	return pow((abs(real(z)) + abs(imag(z))*I), 2) + c;
}

template <>
double_c escapeTimeFormula<PROCEDURE_TRIPLE_MATCHMAKER>(double_c z, double_c c) {
	return (z + a / sqrt3) / (b*(pow(z, 3) - sqrt3 * a*pow(z, 2) + c * z + a * c / sqrt3)) + d;
}

template <>
double_c escapeTimeFormula<PROCEDURE_HIGH_POWER>(double_c z, double_c c) {
	return pow(z, 33554432) + c;
}

/*
This class "Render" is literally a datastructure, but I think of it as a task. It models a task performed on a FractalCanvas.

Render holds a reference to the FractalCanvas that created it to have access to the information stored in the FractalCanvas. Most importantly access to the pointers is required to change the iteration- and pixeldata ...which means care is required so that a Render doesn't use the pointers of the FractalCanvas during a resize etc.

The idea behind the template is that the compiler will optimize everything away that is not used, given specific values for the parameters. For example, in isSameHorizontalLine it says "if (guessing)". If guessing is false, it says "if (false)", so the whole function definition can be recuced to "return false", which will lead to optimizations wherever the function is used as well, because the return value is known at compile time. Also for example in calcPoint there are many ifs 
*/
template <int formula_identifier, bool guessing, bool use_avx, bool julia>
class Render {
public:
	//a copy of some widely used members of canvas.S
	int width;
	int height;
	int screenWidth;
	int screenHeight;
	int oversampling;
	//other 
	FractalCanvas& canvas;
	int renderID;

	int usedThreads; //counts the total number of threads created
	int threadCount;
	Formula formula;
	double_c juliaSeed;
	int maxIters;
	double escapeRadius;
	vector<double_c> inflectionCoords;
	int inflectionCount;
	int inflectionPower;

	chrono::time_point<chrono::steady_clock> startTime;
	chrono::time_point<chrono::steady_clock> endTime;
	bool isFinished;
	int guessedPixelCount;
	int calculatedPixelCount;
	int pixelGroupings;
	long long computedIterations;

	Render(FractalCanvas& canvasContext, int renderID)
	: canvas(canvasContext)
	{
		if(debug) cout << "creating render " << renderID << endl;
		this->renderID = renderID;
		
		threadCount = 0;
		usedThreads = 0;
		pixelGroupings = 0;
		guessedPixelCount = 0;
		calculatedPixelCount = 0;
		computedIterations = 0;
		isFinished = false;

		formula = canvas.S.get_formula();
		juliaSeed = canvas.S.juliaSeed;
		width = canvas.S.get_width();
		height = canvas.S.get_height();
		screenWidth = canvas.S.get_screenWidth();
		screenHeight = canvas.S.get_screenHeight();
		oversampling = canvas.S.get_oversampling();
		maxIters = canvas.S.get_maxIters();
		escapeRadius = canvas.S.get_formula().escapeRadius;
		inflectionCoords = canvas.S.get_inflectionCoords(); //This makes a copy of the whole vector.
		inflectionCount = canvas.S.get_inflectionCount();
		inflectionPower = canvas.S.get_formula().inflectionPower;
	}

	~Render(void) {
		if(debug) cout << "deleting render " << renderID << endl;
	}

	void addToThreadcount(int n) {
		lock_guard<mutex> guard(threadCountChange);
		threadCount += n;
		usedThreads++;
	}

	double_c inflections_m2(double_c c) {
		//More efficient inflection application just for Mandelbrot power 2

		double_c thisInflectionCoord;
		double cr;
		double ci;
		double zr;
		double zi;
		double zrsqr;
		double zisqr;
		zr = real(c);
		zi = imag(c);
		for (int i = inflectionCount - 1; i >= 0; i--) {
			thisInflectionCoord = inflectionCoords[i];
			cr = real(thisInflectionCoord);
			ci = imag(thisInflectionCoord);

			zrsqr = zr * zr;
			zisqr = zi * zi;
			zi = zr * zi;
			zi += zi;
			zi += ci;
			zr = zrsqr - zisqr + cr;
		}
		return zr + zi * I;
	}

	double_c inflections(double_c c) {
		for (int i = inflectionCount - 1; i >= 0; i--) {
			c = pow(c, inflectionPower) + inflectionCoords[i];
		}
		return c;
	}

	/*
		This is a combination of all the regularly used transformations
	*/
	inline double_c map_with_transformations(int x, int y) {
		return
			canvas.S.post_transformation(
			inflections(
			canvas.S.pre_transformation(
			canvas.S.rotation(
			canvas.S.map(x, y)))));
	}

	inline double_c map_with_transformations_m2(int x, int y) {
		return
			canvas.S.post_transformation(
			inflections_m2(
			canvas.S.pre_transformation(
			canvas.S.rotation(
			canvas.S.map(x, y)))));
	}

	int calcPoint(int i, int j) {
		int iterationCount = 0;

		if (formula_identifier == PROCEDURE_M2) {
			double_c c = map_with_transformations_m2(i, j);

			double cr;
			double ci;
			double zr;
			double zi;
			double zrsqr;
			double zisqr;
			if (julia) {
				cr = real(juliaSeed);
				ci = imag(juliaSeed);
				zr = real(c);
				zi = imag(c);
				zrsqr = zr * zr;
				zisqr = zi * zi;
			}
			else {
				double zx;
				double zy;
				zx = real(c);
				zy = imag(c);
				double zx2 = zx;
				double zy2 = zy;

				zx -= 0.25; zy *= zy;
				double q = zx * zx + zy;
				if (4 * q*(q + zx) < zy) {
					canvas.setPixel(i, j, maxIters, CALCULATED);
					return maxIters;
				}
				zx = zx2;
				zy = zy2;
				zx++;
				if (zx * zx + zy * zy < 0.0625) {
					canvas.setPixel(i, j, maxIters, CALCULATED);
					return maxIters;
				}

				cr = zx2;
				ci = zy2;
				zr = 0;
				zi = 0;
				zrsqr = 0;
				zisqr = 0;
			}
			while (zrsqr + zisqr <= 4.0 && iterationCount < maxIters) {
				zi = zr * zi;
				zi += zi;
				zi += ci;
				zr = zrsqr - zisqr + cr;
				zrsqr = zr * zr;
				zisqr = zi * zi;
				iterationCount++;
			}
		}
		if (
			formula_identifier == PROCEDURE_M3
			|| formula_identifier == PROCEDURE_M4
			|| formula_identifier == PROCEDURE_M5
			|| formula_identifier == PROCEDURE_HIGH_POWER
			|| formula_identifier == PROCEDURE_BURNING_SHIP
		) {
			//use a general escape time fractal loop
			double_c c;
			double_c z;
			if (julia) {
				c = juliaSeed;
				z = map_with_transformations(i, j);
			}
			else {
				c = map_with_transformations(i, j);
				z = 0;
			}
			while (real(z)*real(z) + imag(z)*imag(z) < escapeRadius && iterationCount < maxIters) {
				z = escapeTimeFormula<formula_identifier>(z, c);
				iterationCount++;
			}
		}
		if (formula_identifier == PROCEDURE_CHECKERS) {
			double_c c = map_with_transformations_m2(i, j);
			//create checkerboard tiles
			double resolution = pi; //tile size, pi goes well with the natural log transformation
			bool vertical = (int)(imag(c) / resolution) % 2 == 0;
			bool horizontal = (int)(real(c) / resolution) % 2 == 0;
			bool result = horizontal ^ vertical;
			if ((imag(c) < 0) ^ (real(c) < 0)) result = !result;

			//create julia detail simulation
			double transRe = real(c) - (int)(real(c) / resolution) * resolution; //coords translated to the first quadrant's first tile for simplicity
			double transIm = imag(c) - (int)(imag(c) / resolution) * resolution;
			if (transRe < 0) transRe += resolution;
			if (transIm < 0) transIm += resolution;

			bool underInc = transIm < transRe; //coordinate is under the increasing line from (0,0) to (resolution, resolution)
			bool underDec = transIm < resolution - transRe; //under decreasing line from (0, resolution) to (resolution, 0)

			double_c ref;
			if (underInc) {
				if (underDec) ref = 0.5*resolution;
				else ref = resolution + 0.5*resolution*I;
			}
			else {
				if (underDec) ref = 0.5*resolution*I;
				else ref = 0.5*resolution + resolution * I;
			}

			double transRefDist = sqrt(pow(transRe - real(ref), 2) + pow(transIm - imag(ref), 2));
			double distLog = log(transRefDist);
			int resFactors = (int)(distLog / resolution - 0.5);

			if (resFactors % 2 != 0) result = !result;

			if (result) iterationCount = 503; //these choices are arbitrary and just to distinguish tiles. I chose primes because I hope that it makes periodic problems less likely when the number of gradient colors divides one of these values, but I'm not sure if it really helps.
			else iterationCount = 53;

		}
		if (formula_identifier == PROCEDURE_TEST_CONTROL) {
			int thisMaxIters = 100;
			double_c c = map_with_transformations(i, j);
			double cr;
			double ci;
			double zr;
			double zi;
			double zrsqr;
			double zisqr;
			cr = real(c);
			ci = imag(c);
			zr = 0;
			zi = 0;
			zrsqr = 0;
			zisqr = 0;
			while (zrsqr + zisqr <= 4.0 && iterationCount < thisMaxIters) {
				zi = zr * zi;
				zi += zi;
				zi += ci;
				zr = zrsqr - zisqr + cr;
				zrsqr = zr * zr;
				zisqr = zi * zi;
				iterationCount++;
			}
		}
		if (formula_identifier == PROCEDURE_TRIPLE_MATCHMAKER) {
			double_c c;
			double_c z;
			double summ = 0;
			if (julia) {
				c = juliaSeed;
				z = map_with_transformations(i, j);
			}
			else {
				c = map_with_transformations(i, j);
				z = 0;
			}
			for (int k = 2; k < maxIters; k++) {
				z = escapeTimeFormula<PROCEDURE_TRIPLE_MATCHMAKER>(z, c);
				summ += abs(z);
			}
			iterationCount = (int)(summ);
		}
		if (formula_identifier == PROCEDURE_BI) {
			double_c c = map_with_transformations(i, j);
			
			auto insideBox = [](box& box, double_c c) {
				double cr = real(c);
				double ci = imag(c);
				return (
					cr >= box.xfrom
					&& cr <= box.xto
					&& ci >= box.yfrom
					&& ci <= box.yto
				);
			};

			double axisThickness = 0.005;

			box yaxis;
			yaxis.xfrom = 0;
			yaxis.xto = yaxis.xfrom + axisThickness;
			yaxis.yfrom = 0;
			yaxis.yto = 1;

			box xaxis;
			xaxis.xfrom = 0;
			xaxis.xto = 1;
			xaxis.yfrom = 0;
			xaxis.yto = xaxis.yfrom + axisThickness;

			double spacing = 0.02;
			double thickness = 0.2;
			int barcount = 4;
			vector<box> bars(barcount);
			for (int i=0; i<barcount; i++) {
				bars[i].xfrom = yaxis.xto + spacing + (i == 0 ? 0 : bars[i-1].xto);
				bars[i].xto = bars[i].xfrom + thickness;
				bars[i].yfrom = yaxis.yfrom;
			}
			
			switch (canvas.S.BI_choice) {
				case BI_choices::DEPARTMENT_IT: {
					bars[0].yto = 0.6;
					bars[1].yto = 0.7;
					bars[2].yto = 0.2;
					bars[3].yto = 0.9;
					break;
				}
				case BI_choices::DEPARTMENT_MT: {
					bars[0].yto = 0.1;
					bars[1].yto = 0.2;
					bars[2].yto = 0.4;
					bars[3].yto = 0.6;
					break;
				}
				case BI_choices::DEPARTMENT_RND: {
					bars[0].yto = 0.9;
					bars[1].yto = 0.7;
					bars[2].yto = 0.8;
					bars[3].yto = 0.7;
					break;
				}
				case BI_choices::DEPARTMENT_SALES: {
					bars[0].yto = 0.5;
					bars[1].yto = 0.8;
					bars[2].yto = 0.3;
					bars[3].yto = 0.3;
					break;
				}
			}

			iterationCount = 3;
			if (insideBox(yaxis, c) || insideBox(xaxis, c))
				iterationCount = 1;
			else {
				for (int i=0; i<barcount; i++) {
					if (insideBox(bars[i], c))
						iterationCount = 2;
				}
			}
			
		}
		canvas.setPixel(i, j, iterationCount, CALCULATED);

		return iterationCount;
	}

	#define setPixelAndThisIter(i, y, iterationCount, mode) \
	canvas.setPixel(i, y, iterationCount, mode); \
	if (isSame) \
		if (iterationCount != thisIter) { \
			if (thisIter == -1) \
				thisIter = iterationCount; \
			else \
				isSame = false; \
		}

	bool calcPixelVector(vector<int>& pixelVect, int fromPixel, int toPixel) {
		if (renderID != canvas.lastRenderID) {
			if(debug) cout << "Render " << renderID << " cancelled; terminating thread" << endl;
			return true; //Returning true but it doesn't really matter what the result is when the render is cancelled.
		}

		int pixelCount = toPixel - fromPixel;
		bool isSame = true;
		int thisIter = -1;

		if (!use_avx || formula.identifier != PROCEDURE_M2 || pixelCount < 4) {
			int thisI = pixelVect[fromPixel + fromPixel];
			int thisJ = pixelVect[fromPixel + fromPixel + 1];
			int thisIter = calcPoint(thisI, thisJ);

			for (int k = fromPixel + 1; k < toPixel; k++) {
				assert(thisI < width); assert(thisJ < height);

				thisI = pixelVect[k + k];
				thisJ = pixelVect[k + k + 1];
				if (calcPoint(thisI, thisJ) != thisIter)
					isSame = false;
			}
		}
		else { //AVX only for Mandelbrot power 2
			//AVX is used. Length 4 arrays and vectors are constructed to iterate 4 pixels at once. That means 4 i-values, 4 j-values, 4 c-values etc.
			int i[4] = {
				pixelVect[fromPixel + fromPixel]
				,pixelVect[fromPixel + fromPixel + 2]
				,pixelVect[fromPixel + fromPixel + 4]
				,pixelVect[fromPixel + fromPixel + 6]
			};
			int j[4]{
				pixelVect[fromPixel + fromPixel + 1]
				,pixelVect[fromPixel + fromPixel + 3]
				,pixelVect[fromPixel + fromPixel + 5]
				,pixelVect[fromPixel + fromPixel + 7]
			};
			double_c c[4] = {
				map_with_transformations_m2(i[0], j[0])
				,map_with_transformations_m2(i[1], j[1])
				,map_with_transformations_m2(i[2], j[2])
				,map_with_transformations_m2(i[3], j[3])
			};
			int iterationCounts[4] = { 0, 0, 0, 0 };
			int nextPixel = fromPixel + 4;

			__m256d cr, ci, zr, zi, zrsqr, zisqr; //256-bit vectors of 4 doubles each
			//access to doubles individually through array (p for pointer) interpretation
			double* crp = (double*)&cr;
			double* cip = (double*)&ci;
			double* zrp = (double*)&zr;
			double* zip = (double*)&zi;
			double* zrsqrp = (double*)&zrsqr;
			double* zisqrp = (double*)&zisqr;
			//double versions of the vector variables, used to iterate pixels individually:
			double crd, cid, zrd, zid, zrsqrd, zisqrd;

			__m256d bailout_v = _mm256_set1_pd(4.0);
			__m256d zisqr_plus_zrsqr_v = _mm256_setzero_pd();
			__m256d pixel_has_escaped_v;
			bool* pixel_has_escaped_p = (bool*)&pixel_has_escaped_v;
			__m256d all_true = _mm256_cmp_pd(_mm256_setzero_pd(), _mm256_setzero_pd(), _CMP_EQ_OS);

			double julia_r;
			double julia_i;

			//note that the order here is different from the arrays i, j and c. Apparently _mm256_set_pd fills the vector in opposite order?
			if (julia) {
				julia_r = real(juliaSeed);
				julia_i = imag(juliaSeed);

				cr = _mm256_set1_pd(julia_r);
				ci = _mm256_set1_pd(julia_i);
				zr = _mm256_set_pd(real(c[3]), real(c[2]), real(c[1]), real(c[0]));
				zi = _mm256_set_pd(imag(c[3]), imag(c[2]), imag(c[1]), imag(c[0]));
				zrsqr = _mm256_mul_pd(zr, zr);
				zisqr = _mm256_mul_pd(zi, zi);
			}
			else {
				cr = _mm256_set_pd(real(c[3]), real(c[2]), real(c[1]), real(c[0]));
				ci = _mm256_set_pd(imag(c[3]), imag(c[2]), imag(c[1]), imag(c[0]));
				zr = _mm256_setzero_pd();
				zi = _mm256_setzero_pd();
				zrsqr = _mm256_setzero_pd();
				zisqr = _mm256_setzero_pd();
			}

			bool use_avx_iteration = true;

			while (use_avx_iteration) {
				zi = _mm256_mul_pd(zr, zi);
				zi = _mm256_add_pd(zi, zi);
				zi = _mm256_add_pd(zi, ci);
				zr = _mm256_add_pd(_mm256_sub_pd(zrsqr, zisqr), cr);
				zrsqr = _mm256_mul_pd(zr, zr);
				zisqr = _mm256_mul_pd(zi, zi);
				zisqr_plus_zrsqr_v = _mm256_add_pd(zrsqr, zisqr);

				for (int m = 0; m < 4; m++) {
					iterationCounts[m]++;
				}

				pixel_has_escaped_v = _mm256_xor_pd(
					_mm256_cmp_pd(zisqr_plus_zrsqr_v, bailout_v, _CMP_LE_OQ)
					, all_true //xor with this is required to interpret comparison with NaN as true. When there's a NaN I consider that pixel escaped.
				);

				if (
					pixel_has_escaped_p[0]
					|| pixel_has_escaped_p[8] //Indices are multiplied by 8 because the bool values in pixel_has_escaped_p are stored as doubles which are 8 bytes long:
					|| pixel_has_escaped_p[16]
					|| pixel_has_escaped_p[24]
					|| iterationCounts[0] >= maxIters
					|| iterationCounts[1] >= maxIters
					|| iterationCounts[2] >= maxIters
					|| iterationCounts[3] >= maxIters
					) {
					//when here, one of the pixels has escaped or exceeded the max number of iterations

					for (int k = 0; k < 4; k++) { //for each pixel in the AVX vector
						if (pixel_has_escaped_p[8 * k] || iterationCounts[k] >= maxIters) { //this pixel is done
							if (nextPixel < toPixel) {
								//There is work left to do. The finished pixel needs to be replaced by a new one.

								setPixelAndThisIter(i[k], j[k], iterationCounts[k], CALCULATED); //it is done so we set it
								iterationCounts[k] = -1; //to mark pixel k in the vectors as done/invalid

								bool pixelIsValid = false;

								while (!pixelIsValid && nextPixel < toPixel) {
									//take new pixel:
									i[k] = pixelVect[nextPixel + nextPixel];
									j[k] = pixelVect[nextPixel + nextPixel + 1];
									c[k] = map_with_transformations_m2(i[k], j[k]);
									nextPixel++;

									//verify that this pixel is valid:
									if (julia) {
										pixelIsValid = true;

										iterationCounts[k] = 0;
										crp[k] = julia_r;
										cip[k] = julia_i;
										zrp[k] = real(c[k]);
										zip[k] = imag(c[k]);
										zrsqrp[k] = zrp[k] * zrp[k];
										zisqrp[k] = zip[k] * zip[k];
									}
									else {
										double zx = real(c[k]);
										double zy = imag(c[k]);
										double zx_backup = zx;
										double zy_backup = zy;

										zx++;
										if (zx * zx + zy * zy < 0.0625) {
											setPixelAndThisIter(i[k], j[k], maxIters, CALCULATED);
										}
										else {
											zx = zx_backup;
											zy = zy_backup;
											zx -= 0.25;
											zy *= zy;
											double q = zx * zx + zy;
											if (4 * q*(q + zx) < zy) {
												setPixelAndThisIter(i[k], j[k], maxIters, CALCULATED);
											}
											else {
												pixelIsValid = true;

												iterationCounts[k] = 0;
												crp[k] = zx_backup;
												cip[k] = zy_backup;
												zrp[k] = 0;
												zip[k] = 0;
												zrsqrp[k] = 0;
												zisqrp[k] = 0;
											}
										}

									}
								}
								if (!pixelIsValid) {
									//searching for a valid new pixel resulted in reaching the end of the vector. There are not enough pixels to fill the AVX vector.
									use_avx_iteration = false;
								}
							}
							else {
								use_avx_iteration = false;
							}
						}
					}
				}
			}

			//Finish iterating the remaining 3 or fewer pixels without AVX:
			for (int k = 0; k < 4; k++) {
				int iterationCount = iterationCounts[k];
				if (iterationCount != -1) {
					crd = crp[k];
					cid = cip[k];
					zrd = zrp[k];
					zid = zip[k];
					zrsqrd = zrsqrp[k];
					zisqrd = zisqrp[k];
					while (zrsqrd + zisqrd <= 4.0 && iterationCount < maxIters) {
						zid = zrd * zid;
						zid += zid;
						zid += cid;
						zrd = zrsqrd - zisqrd + crd;
						zrsqrd = zrd * zrd;
						zisqrd = zid * zid;
						iterationCount++;
					}
					setPixelAndThisIter(i[k], j[k], iterationCount, CALCULATED);
				}
			}
		}
		calculatedPixelCount += pixelCount;
		return isSame && formula.isGuessable;
	}

	bool isSameHorizontalLine(int iFrom, int iTo, int height) {
		assert(iTo >= iFrom);
		if (guessing) {
			bool same = true;
			int thisIter = canvas.getIterationcount(iFrom, height);
			for (int i = iFrom + 1; i < iTo; i++) {
				if (thisIter != canvas.getIterationcount(i, height)) {
					same = false;
					break;
				}
			}
			return same;
		}
		else {
			return false;
		}
	}

	bool isSameVerticalLine(int jFrom, int jTo, int width) {
		assert(jTo >= jFrom);
		if (guessing) {
			bool same = true;
			int thisIter = canvas.getIterationcount(width, jFrom);
			for (int j = jFrom + 1; j < jTo; j++) {
				if (thisIter != canvas.getIterationcount(width, j)) {
					same = false;
					break;
				}
			}
			return same;
		}
		else {
			return false;
		}
	}

	bool calcHorizontalLine(int iFrom, int iTo, int height) {
		assert(iTo >= iFrom); //faalde
		int size = iTo - iFrom;
		vector<int> hLine(size * 2);
		int thisIndex = 0;
		for (int i = iFrom; i < iTo; i++) {
			hLine[thisIndex++] = i;
			hLine[thisIndex++] = height;
		}
		return calcPixelVector(hLine, 0, thisIndex / 2);
	}

	bool calcVerticalLine(int jFrom, int jTo, int width) {
		assert(jTo >= jFrom);
		int size = jTo - jFrom;
		vector<int> vLine(size * 2);
		int thisIndex = 0;
		for (int j = jFrom; j < jTo; j++) {
			vLine[thisIndex++] = width;
			vLine[thisIndex++] = j;
		}
		return calcPixelVector(vLine, 0, thisIndex / 2);
	}

	/*
		Calculate the tile from imin to imax (x-coordinates) and from jmin to jmax (y-coordinates). These boundary values belong to the tile. The assumption is that the boundary has already been computed. sameBottom means that the bottom row of points in the tile all have the same iterationcount. iterBottom is that iterationcount. The same goes for sameLeft, sameRight etc.

		The bool inNewThread is only to keep track of the current number of threads.

		bitmap_render_responsibility means that this tile should compute and set the colors of its pixels to the bitmap after it is finished. The function can pass the responsiblity on to subtiles through its recursive calls. It's not entirely clear how long the passing should continue for the best performance. When oversampling is used, it's important not to continue for too long because a tile can be smaller than a pixel. (Each pixel is a raster of oversampling×oversampling calculated points.) There would be double work done if every tile that has overlap with some pixel calculates its color. See the explanation of stop_creating_threads.
	*/
	void renderSilverRect(bool inNewThread, bool bitmap_render_responsibility, int imin, int imax, int jmin, int jmax, bool sameTop, int iterTop, bool sameBottom, int iterBottom, bool sameLeft, int iterLeft, bool sameRight, int iterRight)
	{
		if (renderID != canvas.lastRenderID) {
			if(debug) cout << "Render " << renderID << " cancelled; terminating thread" << endl;
			return;
		}

		int size = (imax - imin - 1)*(jmax - jmin - 1); //the size of the part that still has to be calculated

		/*
			This is to prevent creating new threads when the tile becomes so small that it's only a few pixels wide or high. That number pixels is hardcoded as 8 in this part:
			(imax - imin) / oversampling < 8
			
			The problem with threads at that scale has to do with oversampling. Oversampling means there are multiple calculated points per pixel. If two tiles overlap with one pixel, and both tiles are being calculated by different threads, it can happen that a pixel color value is changed by 2 threads at the same time, leading to an incorrect result. By not creating new threads, the subdivision into smaller tiles can continue safely.

			 Increasing NEW_TILE_THREAD_MIN_PIXELS can improve performance by allowing for more optimal tile subdivision, but it also causes color values to be calculated in larger groups together making bitmap updates less frequent, and not allowing new threads too early means less usage of all CPU cores.

			Note also that this is only necessary to show progress in the program. Without visible progress, calculating the color values could be postponed until after all the iteration counts have been calculated, but that's not user-friendly.
		*/
		int stop_creating_threads =
			(imax - imin) / oversampling < NEW_TILE_THREAD_MIN_PIXELS
			|| (jmax - jmin) / oversampling < NEW_TILE_THREAD_MIN_PIXELS;

		bool pass_on_bitmap_render_responsibility = bitmap_render_responsibility && !stop_creating_threads;

		if (guessing) {
			if (sameRight && sameLeft && sameTop && sameBottom && iterRight == iterTop && iterTop == iterLeft && iterLeft == iterBottom && iterRight != 1 && iterRight != 0) {
				//The complete boundary of the tile has the same iterationCount. Fill with that same value:
				for (int i = imin + 1; i < imax; i++) {
					for (int j = jmin + 1; j < jmax; j++) {
						canvas.setPixel(i, j, iterLeft, GUESSED);
					}
				}
				guessedPixelCount += (imax - imin - 1)*(jmax - jmin - 1);
				pass_on_bitmap_render_responsibility = false;
				goto returnLabel;
			}
		}

		if (size < MAXIMUM_TILE_SIZE) {
			//The tile is now very small. Stop the recursion and iterate all pixels.
			vector<int> toIterate(size * 2);
			int thisIndex = 0;
			for (int i = imin + 1; i < imax; i++) {
				for (int j = jmin + 1; j < jmax; j++) {
					toIterate[thisIndex++] = i;
					toIterate[thisIndex++] = j;
				}
			}
			calcPixelVector(toIterate, 0, thisIndex / 2);
			pass_on_bitmap_render_responsibility = false;
			goto returnLabel;
		}

		//The tile gets split up:
		if (imax - imin < jmax - jmin) {
			//The tile is taller than it's wide. Split the tile with a horizontal line. The y-coordinate is:
			int j = jmin + (jmax - jmin) / 2;
			if (!stop_creating_threads) {
				j = j - (j%oversampling); //round to whole pixels
			}

			//compute new line
			bool sameNewLine = calcHorizontalLine(imin + 1, imax, j);
			int iterNewLine = canvas.getIterationcount(imin + 1, j);

			//check right and left for equality
			bool sameRightTop = true;
			bool sameLeftTop = true;
			bool sameLeftBottom = true;
			bool sameRightBottom = true;
			int iterRightTop = canvas.getIterationcount(imax, jmin);
			int iterRightBottom = canvas.getIterationcount(imax, j);
			int iterLeftTop = canvas.getIterationcount(imin, jmin);
			int iterLeftBottom = canvas.getIterationcount(imin, j);

			if (!sameRight) {
				sameRightTop = isSameVerticalLine(jmin, j, imax);
				sameRightBottom = isSameVerticalLine(j, jmax, imax);
			}
			if (!sameLeft) {
				sameLeftTop = isSameVerticalLine(jmin, j, imin);
				sameLeftBottom = isSameVerticalLine(j, jmax, imin);
			}

			if (renderID == canvas.lastRenderID) {
				if (threadCount < canvas.number_of_threads && !stop_creating_threads) {
					addToThreadcount(1);
					thread t(&Render::renderSilverRect, this, true, pass_on_bitmap_render_responsibility, imin, imax, jmin, j, sameTop, iterTop, sameNewLine, iterNewLine, sameLeftTop, iterLeftTop, sameRightTop, iterRightTop);
					renderSilverRect(inNewThread, pass_on_bitmap_render_responsibility, imin, imax, j, jmax, sameNewLine, iterNewLine, sameBottom, iterBottom, sameLeftBottom, iterLeftBottom, sameRightBottom, iterRightBottom);
					t.join();
					inNewThread = false; //t is now the new thread
				}
				else {
					if (height - jmax < 0.5*height) {
						renderSilverRect(false, pass_on_bitmap_render_responsibility, imin, imax, jmin, j, sameTop, iterTop, sameNewLine, iterNewLine, sameLeftTop, iterLeftTop, sameRightTop, iterRightTop);
						renderSilverRect(false, pass_on_bitmap_render_responsibility, imin, imax, j, jmax, sameNewLine, iterNewLine, sameBottom, iterBottom, sameLeftBottom, iterLeftBottom, sameRightBottom, iterRightBottom);
					}
					else {
						//same in different order. The intention is that the center of the screen receives priority.
						renderSilverRect(false, pass_on_bitmap_render_responsibility, imin, imax, j, jmax, sameNewLine, iterNewLine, sameBottom, iterBottom, sameLeftBottom, iterLeftBottom, sameRightBottom, iterRightBottom);
						renderSilverRect(false, pass_on_bitmap_render_responsibility, imin, imax, jmin, j, sameTop, iterTop, sameNewLine, iterNewLine, sameLeftTop, iterLeftTop, sameRightTop, iterRightTop);
					}
				}
			}
		}
		else {
			//The tile is wider than it's tall. Split the tile with a vertical line. The x-coordinate is:
			int i = imin + (imax - imin) / 2;
			if (!stop_creating_threads) {
				i = i - (i%oversampling); //round to whole pixels
			}

			//Compute new line
			bool sameNewLine = calcVerticalLine(jmin + 1, jmax, i);
			int iterNewLine = canvas.getIterationcount(i, jmin + 1);

			//Check Top and Bottom for equality
			bool sameRightTop = true;
			bool sameLeftTop = true;
			bool sameLeftBottom = true;
			bool sameRightBottom = true;
			int iterRightTop = canvas.getIterationcount(i, jmin);
			int iterLeftTop = canvas.getIterationcount(imin, jmin);
			int iterRightBottom = canvas.getIterationcount(i, jmax);
			int iterLeftBottom = canvas.getIterationcount(imin, jmax);

			if (!sameTop) {
				sameLeftTop = isSameHorizontalLine(imin, i, jmin);
				sameRightTop = isSameHorizontalLine(i, imax, jmin);
			}
			if (!sameBottom) {
				sameLeftBottom = isSameHorizontalLine(imin, i, jmax);
				sameRightBottom = isSameHorizontalLine(i, imax, jmax);
			}
			if (renderID == canvas.lastRenderID) {
				if (threadCount < canvas.number_of_threads && !stop_creating_threads) {
					addToThreadcount(1);
					thread t(&Render::renderSilverRect, this, true, pass_on_bitmap_render_responsibility, imin, i, jmin, jmax, sameLeftTop, iterLeftTop, sameLeftBottom, iterLeftBottom, sameLeft, iterLeft, sameNewLine, iterNewLine);
					renderSilverRect(inNewThread, pass_on_bitmap_render_responsibility, i, imax, jmin, jmax, sameRightTop, iterRightTop, sameRightBottom, iterRightBottom, sameNewLine, iterNewLine, sameRight, iterRight);
					t.join();
					inNewThread = false; //t is now the new thread
				}
				else {
					if (width - imax < 0.5*width) {
						renderSilverRect(false, pass_on_bitmap_render_responsibility, imin, i, jmin, jmax, sameLeftTop, iterLeftTop, sameLeftBottom, iterLeftBottom, sameLeft, iterLeft, sameNewLine, iterNewLine);
						renderSilverRect(false, pass_on_bitmap_render_responsibility, i, imax, jmin, jmax, sameRightTop, iterRightTop, sameRightBottom, iterRightBottom, sameNewLine, iterNewLine, sameRight, iterRight);
					}
					else {
						renderSilverRect(false, pass_on_bitmap_render_responsibility, i, imax, jmin, jmax, sameRightTop, iterRightTop, sameRightBottom, iterRightBottom, sameNewLine, iterNewLine, sameRight, iterRight);
						renderSilverRect(false, pass_on_bitmap_render_responsibility, imin, i, jmin, jmax, sameLeftTop, iterLeftTop, sameLeftBottom, iterLeftBottom, sameLeft, iterLeft, sameNewLine, iterNewLine);
					}
				}
			}
		}

	returnLabel:
		if (bitmap_render_responsibility && !pass_on_bitmap_render_responsibility) {
			
			int xborderCorrection = (imax == width - 1 ? 0 : 1);
			int yborderCorrection = (jmax == height - 1 ? 0 : 1);

			int xfrom = imin / oversampling;
			int xto = (imax - xborderCorrection) / oversampling + 1;
			int yfrom = jmin / oversampling;  
			int yto = (jmax - yborderCorrection) / oversampling + 1;
			assert(xfrom <= xto);
			assert(yfrom <= yto);
			
			//if(debug) cout << "taking bitmap render responsibility for (xfrom, xto, yfrom, yto): (" << xfrom << ", " << xto << ", " << yfrom << ", " << yto << endl;
			canvas.renderBitmapRect(false, xfrom, xto, yfrom, yto, canvas.lastBitmapRenderID);
		}
		
		pixelGroupings += 2;
		if (inNewThread) {
			addToThreadcount(-1);
		}
	}

	void renderSilverFull() {
		//This calculates a raster of pixels first and then launches threads for each tile in the raster.
		//"Silver" refers to the Mariani-Silver algorithm.
		//Use the option "View guessed pixels" in the program while in a sparse area to see how it works.
		int imin = 0;
		int imax = width - 1;
		int jmin = 0;
		int jmax = height - 1;

		int tiles = (int)(sqrt(canvas.number_of_threads)); //the number of tiles in both horizontal and vertical direction, so in total there are (tiles * tiles)

		int widthStep = width / tiles;
		int heightStep = height / tiles;

		vector<bool> isSameList(2 * tiles*(tiles + 1));
		vector<int> heights(tiles + 1);
		vector<int> widths(tiles + 1);

		for (int k = 0; k < tiles; k++) {
			heights[k] = k * heightStep;
			widths[k] = k * widthStep;
		}
		heights[tiles] = jmax;
		widths[tiles] = imax;

		//This vector is all pixels in the raster in (x-coord, y-coord) format: x-coords are at odd indices (0, 3, ....), y-coords at even indices
		vector<int> rasterCoordinates((tiles + 1)*(height + width) * 2);
		int rasterCoordinateIndex = 0;

		int thisHeight;
		int thisWidth;
		for (int lineNumH = 0; lineNumH < tiles; lineNumH++) {
			for (int lineNumV = 0; lineNumV < tiles; lineNumV++) {
				thisHeight = heights[lineNumV];
				thisWidth = widths[lineNumH];
				for (int k = widths[lineNumH] + 1; k < widths[lineNumH + 1]; k++) { // a '+ 1' here for initial k, because otherwise the next for-loop would calculate the same pixel again (this only saves calculating typically 4 - 16 pixels per render)
					rasterCoordinates[rasterCoordinateIndex++] = k;
					rasterCoordinates[rasterCoordinateIndex++] = thisHeight;
				}
				for (int k = heights[lineNumV]; k < heights[lineNumV + 1]; k++) {
					rasterCoordinates[rasterCoordinateIndex++] = thisWidth;
					rasterCoordinates[rasterCoordinateIndex++] = k;
				}
			}
		}
		for (int lineNumH = 0; lineNumH < tiles; lineNumH++) {
			for (int k = widths[lineNumH]; k < widths[lineNumH + 1]; k++) {
				rasterCoordinates[rasterCoordinateIndex++] = k;
				rasterCoordinates[rasterCoordinateIndex++] = jmax;
			}
		}
		for (int lineNumV = 0; lineNumV < tiles; lineNumV++) {
			for (int k = heights[lineNumV]; k < heights[lineNumV + 1]; k++) {
				rasterCoordinates[rasterCoordinateIndex++] = imax;
				rasterCoordinates[rasterCoordinateIndex++] = k;
			}
		}
		rasterCoordinates[rasterCoordinateIndex++] = imax;
		rasterCoordinates[rasterCoordinateIndex++] = jmax;


		//Calculate the array of raster pixels multithreaded:
		int usingThreadCount = tiles * tiles;
		int pixelCount = rasterCoordinateIndex / 2;
		int thrVectPieceSize = pixelCount / usingThreadCount;
		int createdRasterThreads = 0;
		vector<thread> threadsRaster(usingThreadCount);

		for (int k = 0; k < usingThreadCount - 1; k++) {
			threadsRaster[createdRasterThreads++] = thread(&Render::calcPixelVector, this, ref(rasterCoordinates), k * thrVectPieceSize, (k + 1)*thrVectPieceSize);
		}
		threadsRaster[createdRasterThreads++] = thread(&Render::calcPixelVector, this, ref(rasterCoordinates), (usingThreadCount - 1) * thrVectPieceSize, pixelCount);

		cout << "Calculating initial raster with " << createdRasterThreads << " threads" << endl;
		for (int k = 0; k < createdRasterThreads; k++)
			threadsRaster[k].join();


		//Check which rectangles in the raster can be guessed:
		for (int lineNumH = 0; lineNumH < tiles; lineNumH++) {
			for (int lineNumV = 0; lineNumV < tiles; lineNumV++) {
				isSameList[(lineNumH * tiles + lineNumV) * 2] = isSameHorizontalLine(widths[lineNumH], widths[lineNumH + 1], heights[lineNumV]);
				isSameList[(lineNumH * tiles + lineNumV) * 2 + 1] = isSameVerticalLine(heights[lineNumV], heights[lineNumV + 1], widths[lineNumH]);
			}
		}
		for (int lineNumH = 0; lineNumH < tiles; lineNumH++) {
			isSameList[(lineNumH * tiles + tiles) * 2] = isSameHorizontalLine(widths[lineNumH], widths[lineNumH + 1], jmax);
		}
		for (int lineNumV = 0; lineNumV < tiles; lineNumV++) {
			isSameList[(tiles * tiles + lineNumV) * 2 + 1] = isSameVerticalLine(heights[lineNumV], heights[lineNumV + 1], imax);
		}

		//Launch threads to fully compute each rectangle
		vector<thread> threadsTiles(usingThreadCount);
		int createdTileThreads = 0;
		threadCount = usingThreadCount;

		for (int lineNumH = 0; lineNumH < tiles; lineNumH++) {
			for (int lineNumV = 0; lineNumV < tiles; lineNumV++) {
				int thisImin = widths[lineNumH];
				int thisImax = widths[lineNumH + 1];
				int thisJmin = heights[lineNumV];
				int thisJmax = heights[lineNumV + 1];

				int iterTop = canvas.getIterationcount(thisImax, thisJmin);
				int iterBottom = canvas.getIterationcount(thisImax, thisJmax);
				int iterLeft = canvas.getIterationcount(thisImin, thisJmax);
				int iterRight = canvas.getIterationcount(thisImax, thisJmax);

				bool sameTop = isSameList[(lineNumH * tiles + lineNumV) * 2];
				bool sameBottom = isSameList[(lineNumH * tiles + (lineNumV + 1)) * 2];
				bool sameLeft = isSameList[(lineNumH * tiles + lineNumV) * 2 + 1];
				bool sameRight = isSameList[((lineNumH + 1) * tiles + lineNumV) * 2 + 1];

				threadsTiles[createdTileThreads++] = thread(&Render::renderSilverRect, this, true, true, thisImin, thisImax, thisJmin, thisJmax, sameTop, iterTop, sameBottom, iterBottom, sameLeft, iterLeft, sameRight, iterRight);
			}
		}

		cout << "Calculating tiles with " << createdTileThreads << " threads" << endl;
		for (int k = 0; k < createdTileThreads; k++) {
			threadsTiles[k].join();
		}
		cout << "Calculating tiles finished." << endl;
		return;
	}

	double getElapsedTime() {
		chrono::duration<double> elapsed;
		if (!isFinished) {
			auto now = chrono::high_resolution_clock::now();
			elapsed = now - startTime;
		}
		else {
			elapsed = endTime - startTime;
		}
		return elapsed.count();
	}

	void execute() {
		assert(width > 0);
		assert(height > 0);
		cout << "in execute " << renderID << endl;

		lock_guard<mutex> guard(renders);

		if (renderID != canvas.lastRenderID)
			return;

		startTime = chrono::high_resolution_clock::now();
		renderSilverFull();
		endTime = chrono::high_resolution_clock::now();
		isFinished = true;

		for (int i = 0; i < width; i++) {
			for (int j = 0; j < height; j++) {
				computedIterations += canvas.iters[i*height + j].iterationCount;
			}
		}
	}
};

#endif