#ifndef RENDER_H
#define RENDER_H

//standard library
#include <algorithm>

//Intrinsics, for using avx instructions
#include <intrin.h>
#include <immintrin.h>

//gcem
#include "gcem/gcem.hpp"	 //for constexpr math functions

//this program
#include "common.cpp"
#include "FractalCanvas.cpp"


constexpr bool CALCULATED = false;
constexpr bool GUESSED = true;


/*
	Escape radius for Mandelbrot power n is: pow(2, 1/(n-1))
	Squared that's pow(2, 2/(n-1)). Using the squared esacape radius is more efficient for calculations.

	cardioidRadius is the radius of the largest circle that fits within the cardioid. Checking if a pixel lies within that circle saves having to iterate. This helps especially for unzoomed high power Mandelbrot sets, which are nearly a circle.
*/
template <int power>
struct MandelbrotFormula
{
	static constexpr double escapeRadiusSq = ([]()
	{
		static_assert(power != 1, "Mandelbrot power can't be 1.");
		return gcem::pow(2, 2.0 / (power-1));
	})();

	static constexpr double cardioidRadius =
		gcem::pow(
			1.0/power
			, 1.0/(power-1)
		)
		-  gcem::pow(
			1.0/power
			, power/(power-1.0)
		);

	static bool withinCardioidRadius(double_c c) {
		return abs(c) < cardioidRadius;
	}

	static inline double_c apply(double_c z, double_c c) {
		return pow(z, power) + c;
	}
};

template <int procedure_identifier>
struct EscapeTimeFormula
{
	static constexpr double escapeRadiusSq = ([]()
	{
		constexpr Procedure procedure = getProcedureObject(procedure_identifier);

		if (procedure.kind == ProcedureKind::Mandelbrot)
		{
			return MandelbrotFormula<procedure.inflectionPower>::escapeRadiusSq;
		}
		else if (procedure.id == BURNING_SHIP.id)
		{
			return 4.0;
		}
		else {
			return 4.0; //This value is intended not to be used.
		}
	})();

	static inline double_c apply(double_c z, double_c c)
	{
		constexpr Procedure procedure = getProcedureObject(procedure_identifier);

		if (procedure.kind == ProcedureKind::Mandelbrot)
		{
			return MandelbrotFormula<procedure.inflectionPower>::apply(z,c);
		}
		else if (procedure.id == BURNING_SHIP.id)
		{
			return pow((abs(real(z)) + abs(imag(z))*I), 2) + c;
		}
		else {
			assert(false);
			return 0;
		}
	};
};


/*
This class "Render" is literally a datastructure, but I think of it as a task. It models a task performed on a FractalCanvas.

Render holds a reference to the FractalCanvas that created it to have access to the information stored in the FractalCanvas. Most importantly access to the pointers is required to change the iteration- and pixeldata ...which means care is required so that a Render doesn't use the pointers of the FractalCanvas during a resize etc.

The idea behind the template is that the compiler will optimize everything away that is not used, given specific values for the parameters. For example, in isSameHorizontalLine it says "if (procedure.guessable)". If procedure.guessable is false, it says "if (false)", so the whole function definition can be recuced to "return false", which will lead to optimizations wherever the function is used as well, because the return value is known at compile time. Also for example in calcPoint there are many ifs 
*/
template <int procedure_identifier, bool use_avx, bool julia>
class Render : public RenderInterface {
public:
	FractalCanvas& canvas;
	const uint renderID;
	static constexpr Procedure procedure = getProcedureObject(procedure_identifier); //this value is known at compile time

	//some widely used members of canvas.P
	const vector<double_c>& inflectionCoords;
	const uint width;			uint getWidth() { return width; }
	const uint height;			uint getHeight() { return height; }
	const uint screenWidth;
	const uint screenHeight;
	const uint oversampling;
	const double_c juliaSeed;
	const uint maxIters;
	const uint inflectionCount;
	
	//other
	uint usedThreads{ 0 }; //counts the total number of threads created
	uint threadCount{ 0 };
	chrono::time_point<chrono::high_resolution_clock> startTime;
	chrono::time_point<chrono::high_resolution_clock> endTime;
	bool ended{ false };
	uint64 guessedPixelCount{ 0 };
	uint64 calculatedPixelCount{ 0 };
	uint64 pixelGroupings{ 0 };
	uint64 computedIterations{ 0 };

	Render(FractalCanvas& canvasContext, uint renderID)
	: canvas(canvasContext)
	, renderID(renderID)
	, inflectionCoords(canvas.P().get_inflectionCoords())
	, width(canvas.P().get_width())
	, height(canvas.P().get_height())
	, screenWidth(canvas.P().get_screenWidth())
	, screenHeight(canvas.P().get_screenHeight())
	, oversampling(canvas.P().get_oversampling())
	, juliaSeed(canvas.P().get_juliaSeed())
	, maxIters(canvas.P().get_maxIters())
	, inflectionCount(canvas.P().get_inflectionCount())
	{
		if(debug) cout << "creating render " << renderID << endl;
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
		for (int i = inflectionCount - 1;  i>=0;  i--) {
			c = pow(c, procedure.inflectionPower) + inflectionCoords[i];
		}
		return c;
	}

	/*
		This is a combination of all the regularly used transformations
	*/
	inline double_c map_with_transformations(uint x, uint y) {
		return
			canvas.P().post_transformation(
			inflections(
			canvas.P().pre_transformation(
			canvas.P().rotation(
			canvas.P().map(x, y)))));
	}

	inline double_c map_with_transformations_m2(uint x, uint y) {
		return
			canvas.P().post_transformation(
			inflections_m2(
			canvas.P().pre_transformation(
			canvas.P().rotation(
			canvas.P().map(x, y)))));
	}

	uint calcPoint(uint x, uint y) {
		assert(x < width && y < height);
		assert(x >= 0 && y >= 0);

		uint iterationCount = 0;

		if (procedure_identifier == M2.id) {
			double_c c = map_with_transformations_m2(x, y);

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
				double zx = real(c);
				double zy = imag(c);

				zx -= 0.25; zy *= zy;
				double q = zx * zx + zy;
				if (4 * q*(q + zx) < zy) {
					//point is inside the cardioid
					canvas.setPixel(x, y, maxIters, CALCULATED, true);
					return maxIters;
				}
				zx = real(c);
				zy = imag(c);
				zx++;
				if (zx * zx + zy * zy < 0.0625) {
					//point is inside the circle
					canvas.setPixel(x, y, maxIters, CALCULATED, true);
					return maxIters;
				}

				cr = real(c);
				ci = imag(c);
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
			procedure_identifier == M3.id
			|| procedure_identifier == M4.id
			|| procedure_identifier == M5.id
			|| procedure_identifier == M512.id
			|| procedure_identifier == HIGH_POWER.id
			|| procedure_identifier == BURNING_SHIP.id
		) {
			//use a general escape time fractal loop
			constexpr double escapeRadiusSq = EscapeTimeFormula<procedure_identifier>::escapeRadiusSq;
			double_c c;
			double_c z;
			if (julia) {
				c = juliaSeed;
				z = map_with_transformations(x, y);
			}
			else {
				c = map_with_transformations(x, y);
				z = 0;
				if (	procedure.kind == ProcedureKind::Mandelbrot) {
					//This is a Mandelbrot formula. It can be checked whether c is within the largest circle within the cardioid:
					constexpr int power = procedure.inflectionPower;

					if (MandelbrotFormula<power>::withinCardioidRadius(c))
					{
						canvas.setPixel(x, y, maxIters, CALCULATED, true);
						return maxIters;
					}
				}
			}
			while (real(z)*real(z) + imag(z)*imag(z) < escapeRadiusSq && iterationCount < maxIters) {
				z = EscapeTimeFormula<procedure_identifier>::apply(z, c);
				iterationCount++;
			}
		}
		if (procedure_identifier == CHECKERS.id) {
			double_c c = map_with_transformations_m2(x, y);
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
		if (procedure_identifier == TRIPLE_MATCHMAKER.id) {
			//constants for Triple Matchmaker:
			constexpr double sqrt3 = gcem::sqrt(3);
			constexpr double a = 2.2;
			constexpr double b = 1.4;
			constexpr double d = 1.1;

			double_c c;
			double_c z;
			double summ = 0;
			if (julia) {
				c = juliaSeed;
				z = map_with_transformations(x, y);
			}
			else {
				c = map_with_transformations(x, y);
				z = 0;
			}
			for (uint k = 2; k < maxIters; k++) {
				z = (z + a / sqrt3) / (b*(pow(z, 3) - sqrt3 * a*pow(z, 2) + c * z + a * c / sqrt3)) + d;
				summ += abs(z);
			}
			iterationCount = (int)(summ);
			canvas.setPixel(x, y, iterationCount, CALCULATED, false);
			return iterationCount;
		}
		if (procedure_identifier == RECURSIVE_FRACTAL.id) {
			//normal Mandelbrot power 2 iteration, after which a measure of how hard the pixel escaped and the angle of c are used as coordinate in a julia set. This is a fractal of fractals. The output of one fractal procecure is used as the input for the second.
			double_c c = map_with_transformations_m2(x, y);

			double cr;
			double ci;
			double zr;
			double zi;
			double zrsqr;
			double zisqr;

			{
				double zx;
				double zy;
				zx = real(c);
				zy = imag(c);
				double zx2 = zx;
				double zy2 = zy;

				zx -= 0.25; zy *= zy;
				double q = zx * zx + zy;
				if (4 * q*(q + zx) < zy) {
					canvas.setPixel(x, y, maxIters, CALCULATED, true);
					return maxIters;
				}
				zx = zx2;
				zy = zy2;
				zx++;
				if (zx * zx + zy * zy < 0.0625) {
					canvas.setPixel(x, y, maxIters, CALCULATED, true);
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

			int iterationCountOld = iterationCount;
			if (iterationCount != maxIters) {
				double difference = (zrsqr + zisqr) - 4;
				double ratio = difference / (4 + abs(c)); //I don't know if this is correct.
				double xrange = 3.03143313302079642213;
				double yrange = 1.64202628038626463614;

				double argument = arg(c);
				double arg_normalized = (argument + pi) / (2*pi);
				arg_normalized *= pow(iterationCount, 3);
				arg_normalized = (arg_normalized - (int)arg_normalized);

				double_c topleftcorner = (-1 * xrange / 2) + (yrange / 2) * I;
				double_c mapped = topleftcorner + arg_normalized*xrange - (ratio * yrange) * I;

				
				iterationCount = 0;
				//julia preparation
				cr = -0.7499218750000002;//real(juliaSeed);
				ci = 0.03597656250000002;//imag(juliaSeed);
				zr = real(mapped);
				zi = imag(mapped);
				zrsqr = zr * zr;
				zisqr = zi * zi;

				while (zrsqr + zisqr <= 4.0 && iterationCount < maxIters) {
					zi = zr * zi;
					zi += zi;
					zi += ci;
					zr = zrsqr - zisqr + cr;
					zrsqr = zr * zr;
					zisqr = zi * zi;
					iterationCount++;
				}
				iterationCount += iterationCountOld;
			}
		}
		if (procedure_identifier == PURE_MORPHINGS.id) {
			double_c c = 
				canvas.P().pre_transformation(
				canvas.P().rotation(
				canvas.P().map(x, y)));

			//This uses the inflections to determine the iterationCount. Does the pixel escape during inflection application?
			//More efficient inflection application just for Mandelbrot power 2
			{
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

					iterationCount++;
					if (zrsqr + zisqr > 4.0) {
						break;
					}
				}
			}
			canvas.setPixel(x, y, iterationCount, CALCULATED, false);
			return iterationCount;
		}

		canvas.setPixel(x, y, iterationCount, CALCULATED, iterationCount == maxIters);

		return iterationCount;
	}

	#define setPixelAndThisIter(x, y, iterationCount, mode, isInMinibrot) \
	canvas.setPixel(x, y, iterationCount, mode, isInMinibrot); \
	if (isSame) \
		if (iterationCount != thisIter) { \
			if (thisIter == -1) \
				thisIter = iterationCount; \
			else \
				isSame = false; \
		}

	struct point {
		uint x;
		uint y;
	};

#pragma GCC push_options
#pragma GCC target ("avx")
	inline bool calcPointVectorAVX_M2(vector<point>& points, uint fromPoint, uint toPoint) {
		//AVX for Mandelbrot power 2
		//AVX is used. Length 4 arrays and vectors are constructed to iterate 4 pixels at once. That means 4 x-values, 4 y-values, 4 c-values etc.
		__m256d all_true = _mm256_cmp_pd(_mm256_setzero_pd(), _mm256_setzero_pd(), _CMP_EQ_OS);

		uint thisIter = -1;
		bool isSame = true; //whether all iteration counts of the pixels in this vector are the same

		uint x[4] = {
			points[fromPoint].x
			,points[fromPoint + 1].x
			,points[fromPoint + 2].x
			,points[fromPoint + 3].x
		};
		uint y[4]{
			points[fromPoint].y
			,points[fromPoint + 1].y
			,points[fromPoint + 2].y
			,points[fromPoint + 3].y
		};
		double_c c[4] = {
			map_with_transformations_m2(x[0], y[0])
			,map_with_transformations_m2(x[1], y[1])
			,map_with_transformations_m2(x[2], y[2])
			,map_with_transformations_m2(x[3], y[3])
		};
		uint iterationCounts[4] = { 0, 0, 0, 0 };
		uint nextPixel = fromPoint + 4;

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

		__m256d bailout_v = _mm256_set1_pd(MandelbrotFormula<2>::escapeRadiusSq);
		__m256d zisqr_plus_zrsqr_v = _mm256_setzero_pd();
		__m256d pixel_has_escaped_v = _mm256_setzero_pd();
		double* bailout_p = (double*)&bailout_v;
		double* zisqr_plus_zrsqr_p = (double*)&zisqr_plus_zrsqr_v;
		bool* pixel_has_escaped_p = (bool*)&pixel_has_escaped_v;

		double julia_r;
		double julia_i;

		//note that the order here is different from the arrays x, y and c. Apparently _mm256_set_pd fills the vector in opposite order.
		if (julia) {
			julia_r = real(juliaSeed);
			julia_i = imag(juliaSeed);

			cr = _mm256_set1_pd(julia_r);
			ci = _mm256_set1_pd(julia_i);
			zr = _mm256_set_pd(real(c[3]), real(c[2]), real(c[1]), real(c[0]));
			zi = _mm256_set_pd(imag(c[3]), imag(c[2]), imag(c[1]), imag(c[0]));
			zrsqr = _mm256_mul_pd(zr, zr);
			zisqr = _mm256_mul_pd(zi, zi);

			//Julia can escape with 0 iterations (if z_0 = c is already larger than the escape radius).
			zisqr_plus_zrsqr_v = _mm256_add_pd(zrsqr, zisqr);

			pixel_has_escaped_v = _mm256_xor_pd(
				_mm256_cmp_pd(zisqr_plus_zrsqr_v, bailout_v, _CMP_LE_OQ)
				,all_true //xor with this is required to interpret comparison with NaN as true. When there's a NaN I consider that pixel escaped.
			);
		}
		else { //Mandelbrot
			cr = _mm256_set_pd(real(c[3]), real(c[2]), real(c[1]), real(c[0]));
			ci = _mm256_set_pd(imag(c[3]), imag(c[2]), imag(c[1]), imag(c[0]));
			zr = _mm256_setzero_pd();
			zi = _mm256_setzero_pd();
			zrsqr = _mm256_setzero_pd();
			zisqr = _mm256_setzero_pd();
		}

		bool continue_avx_iteration = true;

		while (true) {
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
						if (nextPixel < toPoint) {
							//There is work left to do. The finished pixel needs to be replaced by a new one.

							setPixelAndThisIter(x[k], y[k], iterationCounts[k], CALCULATED, iterationCounts[k] == maxIters); //it is done so we set it
							iterationCounts[k] = -1; //to mark pixel k in the vectors as done/invalid

							bool pixelIsValid = false;

							while (!pixelIsValid && nextPixel < toPoint) {
								//take new pixel:
								x[k] = points[nextPixel].x;
								y[k] = points[nextPixel].y;
								c[k] = map_with_transformations_m2(x[k], y[k]);
								nextPixel++;

								//verify that this pixel is valid:
								if (julia) {
									iterationCounts[k] = 0;
									crp[k] = julia_r;
									cip[k] = julia_i;
									zrp[k] = real(c[k]);
									zip[k] = imag(c[k]);
									zrsqrp[k] = zrp[k] * zrp[k];
									zisqrp[k] = zip[k] * zip[k];

									zisqr_plus_zrsqr_p[k] = zrsqrp[k] + zisqrp[k];

									if (zisqr_plus_zrsqr_p[k] > bailout_p[0]) {
										//julia escaped at 0 iterations
										setPixelAndThisIter(x[k], y[k], 0, CALCULATED, false);
										pixelIsValid = false;
									}
									else {
										pixelIsValid = true;
									}
								}
								else {
									double zx = real(c[k]);
									double zy = imag(c[k]);
									double zx_backup = zx;
									double zy_backup = zy;

									zx++;
									if (zx * zx + zy * zy < 0.0625) {
										setPixelAndThisIter(x[k], y[k], maxIters, CALCULATED, true);
									}
									else {
										zx = zx_backup;
										zy = zy_backup;
										zx -= 0.25;
										zy *= zy;
										double q = zx * zx + zy;
										if (4 * q*(q + zx) < zy) {
											setPixelAndThisIter(x[k], y[k], maxIters, CALCULATED, true);
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
								continue_avx_iteration = false;
							}
						}
						else {
							continue_avx_iteration = false;
						}
					}
				}
			}

			if (continue_avx_iteration) {
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
					,all_true
				);
			}
			else break;
		}

		//Finish iterating the remaining 3 or fewer pixels without AVX:
		for (int k = 0; k < 4; k++) {
			uint iterationCount = iterationCounts[k];
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
				setPixelAndThisIter(x[k], y[k], iterationCount, CALCULATED, iterationCount == maxIters);
			}
		}
		return isSame;
	}
#pragma GCC pop_options
	
	#undef setPixelAndThisIter

	/*
		including fromPoint; not including toPoint
	*/
	bool calcPointVector(vector<point>& points, uint fromPoint, uint toPoint) {
		assert(fromPoint >= 0);
		assert(toPoint >= fromPoint);
		assert(points.size() > (toPoint-1));

		if (renderID != canvas.lastRenderID) {
			if(debug) cout << "Render " << renderID << " cancelled; terminating thread" << endl;
			return true; //Returning true but it doesn't really matter what the result is when the render is cancelled.
		}

		uint pointCount = toPoint - fromPoint;
		bool isSame = true;

		if (!use_avx || procedure_identifier != M2.id || pointCount < 4) {
			uint thisX = points[fromPoint].x;
			uint thisY = points[fromPoint].y;
			uint thisIter = calcPoint(thisX, thisY);

			for (uint k = fromPoint + 1; k < toPoint; k++) {
				assert(thisX < width); assert(thisY < height);
				thisX = points[k].x;
				thisY = points[k].y;
				if (calcPoint(thisX, thisY) != thisIter) //calculates the point
					isSame = false;
			}
		}
		else { 
			isSame = calcPointVectorAVX_M2(points, fromPoint, toPoint);
		}
		calculatedPixelCount += pointCount;
		return isSame && procedure.guessable;
	}

	/*
		including from; not including to
	*/
	bool isSameHorizontalLine(uint xFrom, uint xTo, uint height) {
		assert(xTo >= xFrom);
		if (procedure.guessable) {
			bool same = true;
			uint thisIter = canvas.getIterationcount(xFrom, height);
			for (uint x = xFrom + 1; x < xTo; x++) {
				if (thisIter != canvas.getIterationcount(x, height)) {
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

	/*
		including from; not including to
	*/
	bool isSameVerticalLine(uint yFrom, uint yTo, uint width) {
		assert(yTo >= yFrom);
		if (procedure.guessable) {
			bool same = true;
			uint thisIter = canvas.getIterationcount(width, yFrom);
			for (uint y = yFrom + 1; y < yTo; y++) {
				if (thisIter != canvas.getIterationcount(width, y)) {
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

	/*
		including from; not including to
	*/
	bool calcHorizontalLine(uint xFrom, uint xTo, uint height) {
		assert(xTo >= xFrom);
		uint size = xTo - xFrom;
		vector<point> hLine(size);
		uint thisIndex = 0;
		for (uint x = xFrom; x < xTo; x++) {
			hLine[thisIndex++] = {x, height};
		}
		return calcPointVector(hLine, 0, thisIndex);
	}

	/*
		including from; not including to
	*/
	bool calcVerticalLine(uint yFrom, uint yTo, uint width) {
		assert(yTo >= yFrom);
		uint size = yTo - yFrom;
		vector<point> vLine(size);
		uint thisIndex = 0;
		for (uint y = yFrom; y < yTo; y++) {
			vLine[thisIndex++] = {width, y};
		}
		return calcPointVector(vLine, 0, thisIndex);
	}

	/*
		Calculate the tile from xmin to xmax (x-coordinates) and from ymin to ymax (y-coordinates). These boundary values belong to the tile. The assumption is that the boundary has already been computed. sameBottom means that the bottom row of points in the tile all have the same iterationcount. iterBottom is that iterationcount. The same goes for sameLeft, sameRight etc.

		The bool inNewThread is only to keep track of the current number of threads.

		bitmap_render_responsibility means that this tile should compute and set the colors of its pixels to the bitmap after it is finished. The function can pass the responsiblity on to subtiles through its recursive calls. It's not entirely clear how long the passing should continue for the best performance. When oversampling is used, it's important not to continue for too long because a tile can be smaller than a pixel. (Each pixel is a raster of oversampling×oversampling calculated points.) There would be double work done if every tile that has overlap with some pixel calculates its color. See the explanation of stop_creating_threads.
	*/
	void renderSilverRect(bool inNewThread, bool bitmap_render_responsibility, uint xmin, uint xmax, uint ymin, uint ymax, bool sameTop, uint iterTop, bool sameBottom, uint iterBottom, bool sameLeft, uint iterLeft, bool sameRight, uint iterRight)
	{
		if (renderID != canvas.lastRenderID) {
			if(debug) cout << "Render " << renderID << " cancelled; terminating thread" << endl;
			return;
		}

		assert(xmax > xmin);
		assert(ymax > ymin);
		uint size = (xmax - xmin - 1)*(ymax - ymin - 1); //the size of the part that still has to be calculated
		
		/*
			This is to prevent creating new threads when the tile becomes so small that it's only a few pixels wide or high. That number pixels is hardcoded as 8 in this part:
			(xmax - xmin) / oversampling < 8
			
			The problem with threads at that scale has to do with oversampling. Oversampling means there are multiple calculated points per pixel. If two tiles overlap with one pixel, and both tiles are being calculated by different threads, it can happen that a pixel color value is changed by 2 threads at the same time, leading to an incorrect result. By not creating new threads, the subdivision into smaller tiles can continue safely.

			 Increasing NEW_TILE_THREAD_MIN_PIXELS can improve performance by allowing for more optimal tile subdivision, but it also causes color values to be calculated in larger groups together making bitmap updates less frequent, and not allowing new threads too early means less usage of all CPU cores.

			Note also that this is only necessary to show progress in the program. Without visible progress, calculating the color values could be postponed until after all the iteration counts have been calculated, but that's not user-friendly.
		*/
		bool stop_creating_threads =
			(xmax - xmin) / oversampling < NEW_TILE_THREAD_MIN_PIXELS
			|| (ymax - ymin) / oversampling < NEW_TILE_THREAD_MIN_PIXELS;

		bool pass_on_bitmap_render_responsibility = bitmap_render_responsibility && !stop_creating_threads;

		if (procedure.guessable) {
			if (sameRight && sameLeft && sameTop && sameBottom && iterRight == iterTop && iterTop == iterLeft && iterLeft == iterBottom && iterRight != 1 && iterRight != 0) {
				//The complete boundary of the tile has the same iterationCount. Fill with that same value:
				bool isInMinibrot = canvas.getIterData(xmin, ymin).inMinibrot();
				for (uint x = xmin + 1; x < xmax; x++) {
					for (uint y = ymin + 1; y < ymax; y++) {
						canvas.setPixel(x, y, iterLeft, GUESSED, isInMinibrot);
					}
				}
				guessedPixelCount += (xmax - xmin - 1)*(ymax - ymin - 1);
				pass_on_bitmap_render_responsibility = false;
				goto returnLabel;
			}
		}

		if (size < MAXIMUM_TILE_SIZE) {
			//The tile is now very small. Stop the recursion and iterate all pixels.
			vector<point> toIterate(size);
			uint thisIndex = 0;
			for (uint x = xmin + 1; x < xmax; x++) {
				for (uint y = ymin + 1; y < ymax; y++) {
					toIterate[thisIndex++] = {x, y};
				}
			}
			calcPointVector(toIterate, 0, thisIndex);
			pass_on_bitmap_render_responsibility = false;
			goto returnLabel;
		}

		//The tile gets split up:
		if (xmax - xmin < ymax - ymin) {
			//The tile is taller than it's wide. Split the tile with a horizontal line. The y-coordinate is:
			int y = ymin + (ymax - ymin) / 2;
			if (!stop_creating_threads) {
				y = y - (y%oversampling); //round to whole pixels
			}

			//compute new line
			bool sameNewLine = calcHorizontalLine(xmin + 1, xmax, y);
			int iterNewLine = canvas.getIterationcount(xmin + 1, y);

			//check right and left for equality
			bool sameRightTop = true;
			bool sameLeftTop = true;
			bool sameLeftBottom = true;
			bool sameRightBottom = true;
			int iterRightTop = canvas.getIterationcount(xmax, ymin);
			int iterRightBottom = canvas.getIterationcount(xmax, y);
			int iterLeftTop = canvas.getIterationcount(xmin, ymin);
			int iterLeftBottom = canvas.getIterationcount(xmin, y);

			if (!sameRight) {
				sameRightTop = isSameVerticalLine(ymin, y, xmax);
				sameRightBottom = isSameVerticalLine(y, ymax, xmax);
			}
			if (!sameLeft) {
				sameLeftTop = isSameVerticalLine(ymin, y, xmin);
				sameLeftBottom = isSameVerticalLine(y, ymax, xmin);
			}

			if (renderID == canvas.lastRenderID) {
				if (threadCount < canvas.number_of_threads && !stop_creating_threads) {
					addToThreadcount(1);
					thread t(&Render::renderSilverRect, this, true, pass_on_bitmap_render_responsibility, xmin, xmax, ymin, y, sameTop, iterTop, sameNewLine, iterNewLine, sameLeftTop, iterLeftTop, sameRightTop, iterRightTop);
					renderSilverRect(inNewThread, pass_on_bitmap_render_responsibility, xmin, xmax, y, ymax, sameNewLine, iterNewLine, sameBottom, iterBottom, sameLeftBottom, iterLeftBottom, sameRightBottom, iterRightBottom);
					t.join();
					inNewThread = false; //t is now the new thread
				}
				else {
					if (height - ymax < 0.5*height) {
						renderSilverRect(false, pass_on_bitmap_render_responsibility, xmin, xmax, ymin, y, sameTop, iterTop, sameNewLine, iterNewLine, sameLeftTop, iterLeftTop, sameRightTop, iterRightTop);
						renderSilverRect(false, pass_on_bitmap_render_responsibility, xmin, xmax, y, ymax, sameNewLine, iterNewLine, sameBottom, iterBottom, sameLeftBottom, iterLeftBottom, sameRightBottom, iterRightBottom);
					}
					else {
						//same in different order. The intention is that the center of the screen receives priority.
						renderSilverRect(false, pass_on_bitmap_render_responsibility, xmin, xmax, y, ymax, sameNewLine, iterNewLine, sameBottom, iterBottom, sameLeftBottom, iterLeftBottom, sameRightBottom, iterRightBottom);
						renderSilverRect(false, pass_on_bitmap_render_responsibility, xmin, xmax, ymin, y, sameTop, iterTop, sameNewLine, iterNewLine, sameLeftTop, iterLeftTop, sameRightTop, iterRightTop);
					}
				}
			}
		}
		else {
			//The tile is wider than it's tall. Split the tile with a vertical line. The x-coordinate is:
			int x = xmin + (xmax - xmin) / 2;
			if (!stop_creating_threads) {
				x = x - (x%oversampling); //round to whole pixels
			}

			//Compute new line
			bool sameNewLine = calcVerticalLine(ymin + 1, ymax, x);
			int iterNewLine = canvas.getIterationcount(x, ymin + 1);

			//Check Top and Bottom for equality
			bool sameRightTop = true;
			bool sameLeftTop = true;
			bool sameLeftBottom = true;
			bool sameRightBottom = true;
			int iterRightTop = canvas.getIterationcount(x, ymin);
			int iterLeftTop = canvas.getIterationcount(xmin, ymin);
			int iterRightBottom = canvas.getIterationcount(x, ymax);
			int iterLeftBottom = canvas.getIterationcount(xmin, ymax);

			if (!sameTop) {
				sameLeftTop = isSameHorizontalLine(xmin, x, ymin);
				sameRightTop = isSameHorizontalLine(x, xmax, ymin);
			}
			if (!sameBottom) {
				sameLeftBottom = isSameHorizontalLine(xmin, x, ymax);
				sameRightBottom = isSameHorizontalLine(x, xmax, ymax);
			}
			if (renderID == canvas.lastRenderID) {
				if (threadCount < canvas.number_of_threads && !stop_creating_threads) {
					addToThreadcount(1);
					thread t(&Render::renderSilverRect, this, true, pass_on_bitmap_render_responsibility, xmin, x, ymin, ymax, sameLeftTop, iterLeftTop, sameLeftBottom, iterLeftBottom, sameLeft, iterLeft, sameNewLine, iterNewLine);
					renderSilverRect(inNewThread, pass_on_bitmap_render_responsibility, x, xmax, ymin, ymax, sameRightTop, iterRightTop, sameRightBottom, iterRightBottom, sameNewLine, iterNewLine, sameRight, iterRight);
					t.join();
					inNewThread = false; //t is now the new thread
				}
				else {
					if (width - xmax < 0.5*width) {
						renderSilverRect(false, pass_on_bitmap_render_responsibility, xmin, x, ymin, ymax, sameLeftTop, iterLeftTop, sameLeftBottom, iterLeftBottom, sameLeft, iterLeft, sameNewLine, iterNewLine);
						renderSilverRect(false, pass_on_bitmap_render_responsibility, x, xmax, ymin, ymax, sameRightTop, iterRightTop, sameRightBottom, iterRightBottom, sameNewLine, iterNewLine, sameRight, iterRight);
					}
					else {
						renderSilverRect(false, pass_on_bitmap_render_responsibility, x, xmax, ymin, ymax, sameRightTop, iterRightTop, sameRightBottom, iterRightBottom, sameNewLine, iterNewLine, sameRight, iterRight);
						renderSilverRect(false, pass_on_bitmap_render_responsibility, xmin, x, ymin, ymax, sameLeftTop, iterLeftTop, sameLeftBottom, iterLeftBottom, sameLeft, iterLeft, sameNewLine, iterNewLine);
					}
				}
			}
		}

	returnLabel:
		if (bitmap_render_responsibility && !pass_on_bitmap_render_responsibility) {
			
			int xborderCorrection = (xmax == width - 1 ? 0 : 1);
			int yborderCorrection = (ymax == height - 1 ? 0 : 1);

			int xfrom = xmin / oversampling;
			int xto = (xmax - xborderCorrection) / oversampling + 1;
			int yfrom = ymin / oversampling;  
			int yto = (ymax - yborderCorrection) / oversampling + 1;
			assert(xfrom <= xto);
			assert(yfrom <= yto);
			
			canvas.renderBitmapRect(false, xfrom, xto, yfrom, yto, canvas.lastBitmapRenderID);
		}
		
		pixelGroupings += 2;
		if (inNewThread) {
			addToThreadcount(-1);
		}
	}

	void renderSilverFull() {
		//This calculates a raster of points first and then launches threads for each tile in the raster.
		//"Silver" refers to the Mariani-Silver algorithm.
		//Use the option "View guessed pixels" in the program while in a sparse area to see how it works.
		assert(width > 0);
		assert(height > 0);
		uint xmin = 0;
		uint xmax = width - 1;
		uint ymin = 0;
		uint ymax = height - 1;

		uint tiles = (uint)(sqrt(canvas.number_of_threads)); //the number of tiles in both horizontal and vertical direction, so in total there are (tiles * tiles)

		//It can happen that there are too many tiles with a high number of threads and a low resolution. The tiles should contain at least one point within their borders. The number of tiles is reduced accordingly, if needed. Because of the restriction, 3x3 is the smallest resolution that this tiling algorithm works for.
		tiles = min({tiles, (width - 1)/2, (height-1)/2});
		if (tiles == 0) {
			cout << "No render takes place. The tiling algorithm only works with a resolution of at least 3x3." << endl;
			return;
		}

		if(debug) cout << "renderSilverFull with tiles: " << tiles << endl;

		uint widthStep = width / tiles;
		uint heightStep = height / tiles;

		vector<bool> isSameList(2 * tiles*(tiles + 1));
		vector<uint> heights(tiles + 1);
		vector<uint> widths(tiles + 1);

		for (uint k = 0; k < tiles; k++) {
			heights[k] = k * heightStep;
			widths[k] = k * widthStep;
		}
		heights[tiles] = ymax;
		widths[tiles] = xmax;

		//This vector is all pixels in the raster in (x-coord, y-coord) format: x-coords are at odd indices (0, 3, ....), y-coords at even indices
		vector<point> rasterCoordinates((tiles + 1)*(height + width));
		uint rasterCoordinateIndex = 0;

		uint thisHeight;
		uint thisWidth;
		for (uint lineNumH = 0; lineNumH < tiles; lineNumH++) {
			for (uint lineNumV = 0; lineNumV < tiles; lineNumV++) {
				thisHeight = heights[lineNumV];
				thisWidth = widths[lineNumH];
				for (uint k = widths[lineNumH] + 1; k < widths[lineNumH + 1]; k++) { // a '+ 1' here for initial k, because otherwise the next for-loop would calculate the same pixel again (this only saves calculating typically 4 - 16 pixels per render)
					rasterCoordinates[rasterCoordinateIndex++] = {k, thisHeight};
				}
				for (uint k = heights[lineNumV]; k < heights[lineNumV + 1]; k++) {
					rasterCoordinates[rasterCoordinateIndex++] = {thisWidth, k};
				}
			}
		}
		for (uint lineNumH = 0; lineNumH < tiles; lineNumH++) {
			for (uint k = widths[lineNumH]; k < widths[lineNumH + 1]; k++) {
				rasterCoordinates[rasterCoordinateIndex++] = {k, ymax};
			}
		}
		for (uint lineNumV = 0; lineNumV < tiles; lineNumV++) {
			for (uint k = heights[lineNumV]; k < heights[lineNumV + 1]; k++) {
				rasterCoordinates[rasterCoordinateIndex++] = {xmax, k};
			}
		}
		rasterCoordinates[rasterCoordinateIndex++] = {xmax, ymax};


		//Calculate the array of raster pixels multithreaded:
		uint usingThreadCount = tiles * tiles;
		uint pixelCount = rasterCoordinateIndex;
		uint thrVectPieceSize = pixelCount / usingThreadCount;
		uint createdRasterThreads = 0;
		vector<thread> threadsRaster(usingThreadCount);

		for (uint k = 0; k < usingThreadCount - 1; k++) {
			threadsRaster[createdRasterThreads++] = thread(&Render::calcPointVector, this, ref(rasterCoordinates), k * thrVectPieceSize, (k + 1)*thrVectPieceSize);
		}
		threadsRaster[createdRasterThreads++] = thread(&Render::calcPointVector, this, ref(rasterCoordinates), (usingThreadCount - 1) * thrVectPieceSize, pixelCount);

		cout << "Calculating initial raster with " << createdRasterThreads << " threads" << endl;
		for (uint k = 0; k < createdRasterThreads; k++)
			threadsRaster[k].join();


		//Check which rectangles in the raster can be guessed:
		for (uint lineNumH = 0; lineNumH < tiles; lineNumH++) {
			for (uint lineNumV = 0; lineNumV < tiles; lineNumV++) {
				isSameList[(lineNumH * tiles + lineNumV) * 2] = isSameHorizontalLine(widths[lineNumH], widths[lineNumH + 1], heights[lineNumV]);
				isSameList[(lineNumH * tiles + lineNumV) * 2 + 1] = isSameVerticalLine(heights[lineNumV], heights[lineNumV + 1], widths[lineNumH]);
			}
		}
		for (uint lineNumH = 0; lineNumH < tiles; lineNumH++) {
			isSameList[(lineNumH * tiles + tiles) * 2] = isSameHorizontalLine(widths[lineNumH], widths[lineNumH + 1], ymax);
		}
		for (uint lineNumV = 0; lineNumV < tiles; lineNumV++) {
			isSameList[(tiles * tiles + lineNumV) * 2 + 1] = isSameVerticalLine(heights[lineNumV], heights[lineNumV + 1], xmax);
		}

		//Launch threads to fully compute each rectangle
		vector<thread> threadsTiles(usingThreadCount);
		uint createdTileThreads = 0;
		threadCount = usingThreadCount;

		for (uint lineNumH = 0; lineNumH < tiles; lineNumH++) {
			for (uint lineNumV = 0; lineNumV < tiles; lineNumV++) {
				uint thisImin = widths[lineNumH];
				uint thisImax = widths[lineNumH + 1];
				uint thisJmin = heights[lineNumV];
				uint thisJmax = heights[lineNumV + 1];

				uint iterTop = canvas.getIterationcount(thisImax, thisJmin);
				uint iterBottom = canvas.getIterationcount(thisImax, thisJmax);
				uint iterLeft = canvas.getIterationcount(thisImin, thisJmax);
				uint iterRight = canvas.getIterationcount(thisImax, thisJmax);

				bool sameTop = isSameList[(lineNumH * tiles + lineNumV) * 2];
				bool sameBottom = isSameList[(lineNumH * tiles + (lineNumV + 1)) * 2];
				bool sameLeft = isSameList[(lineNumH * tiles + lineNumV) * 2 + 1];
				bool sameRight = isSameList[((lineNumH + 1) * tiles + lineNumV) * 2 + 1];

				threadsTiles[createdTileThreads++] = thread(&Render::renderSilverRect, this, true, true, thisImin, thisImax, thisJmin, thisJmax, sameTop, iterTop, sameBottom, iterBottom, sameLeft, iterLeft, sameRight, iterRight);
			}
		}

		cout << "Calculating tiles with " << createdTileThreads << " threads" << endl;
		for (uint k = 0; k < createdTileThreads; k++) {
			threadsTiles[k].join();
		}
		cout << "Calculating tiles finished." << endl;
		return;
	}

	double getElapsedTime() {
		chrono::duration<double> elapsed;
		if (!ended) {
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
		assert( ! ended);
		cout << "in execute " << renderID << endl;

		if (renderID != canvas.lastRenderID)
			return;

		startTime = chrono::high_resolution_clock::now();
		renderSilverFull();
		endTime = chrono::high_resolution_clock::now();
		ended = true;

		for (uint i = 0; i < width; i++) {
			for (uint j = 0; j < height; j++) {
				computedIterations += canvas.iters[i*height + j].iterationCount();
			}
		}
	}
	
	ProgressInfo getProgress() {
		return { guessedPixelCount, calculatedPixelCount, getElapsedTime(), ended };
	}

	void* canvasPtr() {
		return canvas.voidPtr();
	}

	uint getId() {
		return renderID;
	}
};

#endif