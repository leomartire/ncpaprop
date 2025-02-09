#include "Atmosphere.h"
#include "util.h"
#include "anyoption.h"
#include "EquationSet.h"
#include "Acoustic2DEquationSet.h"
#include "BreakConditions.h"
#include "ODESystem.h"
#include "ReflectionCondition2D.h"
#include "Slice.h"

#include <iostream>
#include <cmath>
#include <fstream>
#include <vector>
#include <stdexcept>
#include <sstream>

/**
 * @version 1.1.8
 * @date 2015-10-09
 * @author Claus Hetzer, claus@olemiss.edu
 * 
 * Changelog:
 * 1.1.8: Changed error text for no atmospheric file specified
 * 1.1.7: Changed example atmospheric profile to canonical model
 * 1.1.6: Removed help text for inactive atmosphere input options
 * 1.1.5: Added additional help text
 * 1.1.4: Changed --asciifile option to --atmosfile, --asciiorder to --atmosfileorder
 * 1.1.3: Called triggered() function in bouncer instead of counting bounces
 * 1.1.2: Fixed bug in bounce counter, allowing final bounce to be reported
 * 1.1.1: Allowed final step in calculation to be included, to correct the number of skips reported.
 */


using namespace NCPA;
using namespace std;

#ifndef Pi
#define Pi 3.14159
#endif

// Function to parse the options from the command line/config file
AnyOption *parseInputOptions( int argc, char **argv );

int main( int argc, char **argv ) {

	AnyOption *opt = parseInputOptions( argc, argv );
	
	// Declare and populate variables
	// First, file type
	enum AtmosphericFileType { ATMOSFILE, JETFILE, SLICEFILE };   // Expand this enum as we put in more file types
	AtmosphericFileType filetype;
	int numTypesDeclared = 0;
	string atmosfile = "";
	if ( opt->getValue( "jetfile" ) != NULL ) {
                atmosfile = opt->getValue( "jetfile" );
                filetype = JETFILE;
                numTypesDeclared++;
        }
	if ( opt->getValue( "atmosfile" ) != NULL ) {
                atmosfile = opt->getValue( "atmosfile" );
                filetype = ATMOSFILE;
                numTypesDeclared++;
        }
        if (opt->getValue( "slicefile" ) != NULL ) {
		atmosfile = opt->getValue( "slicefile" );
		filetype = SLICEFILE;
		numTypesDeclared++;
	}

	// Make sure only one file type was requested
	if (numTypesDeclared != 1) {
		delete opt;
		throw invalid_argument( "Atmospheric file must be specified!");
	}

	// Parse arguments based on file type selected and set defaults
	double elev0 = 0, maxelev = 0, delev = 1, azimuth0 = -1, maxazimuth = -1, dazimuth = 1,
		maxraylength = 0, stepsize = 0.01, maxrange = 1e12, sourceheight = -1, maxheight = 150;
	//double lat = 0, lon = 0;
	unsigned int maxskips = 0;
	bool inMPS = true;

	// Elevation angle parameters
	if (opt->getValue( "elev" ) != NULL) {
		elev0 = atof( opt->getValue("elev") );
	} else {
		delete opt;
		throw invalid_argument( "Option --elev is required!" );
	}
	if (fabs(elev0) >= 90.0) {
		delete opt;
		throw invalid_argument( "Option --elev must be in the range (-90,90)!" );
	}
	maxelev = elev0;
	if (opt->getValue( "maxelev" ) != NULL) {
		maxelev = atof( opt->getValue( "maxelev" ) );
	}
	if (opt->getValue( "delev" ) != NULL) {
		delev = atof( opt->getValue( "delev" ) );
	}

	// Convert elevation angles to radians
	elev0 = deg2rad( elev0 );
	delev = deg2rad( delev );
	maxelev = deg2rad( maxelev );

	// Azimuth angle parameters.  We'll keep azimuths in degrees cause that's what they are, 
	// and convert when necessary
	if (filetype != SLICEFILE) {
		if (opt->getValue( "azimuth" ) != NULL) {
			azimuth0 = atof( opt->getValue("azimuth") );
		} else {
			delete opt;
			throw invalid_argument( "Option --azimuth is required!" );
		}

		while (azimuth0 >= 360.0) {
			azimuth0 -= 360.0;
		}
		while (azimuth0 < 0.0) {
			azimuth0 += 360.0;
		}
		maxazimuth = azimuth0;
		if (opt->getValue( "maxazimuth" ) != NULL) {
			maxazimuth = atof( opt->getValue( "maxazimuth" ) );
		}
		if (opt->getValue( "dazimuth" ) != NULL) {
			dazimuth = atof( opt->getValue( "dazimuth" ) );
		}
		while (maxazimuth >= 360.0) {
			maxazimuth -= 360.0;
		}
		while (maxazimuth < 0.0) {
			maxazimuth += 360.0;
		}
	}
	
	// Calculation parameters
	maxraylength = -1.0;
	if (opt->getValue( "maxraylength" ) != NULL) {
		maxraylength = atof( opt->getValue( "maxraylength" ) );
	} else {
		delete opt;
		throw invalid_argument( "Option --maxraylength is required!" );
	}
	/*
	if (opt->getValue( "lat" ) != NULL) {
		lat = atof( opt->getValue( "lat" ) );
	} 
	if (opt->getValue( "lon" ) != NULL) {
		lon = atof( opt->getValue( "lon" ) );
	}
	*/
	if (opt->getValue( "stepsize" ) != NULL) {
		stepsize = atof( opt->getValue( "stepsize" ) );
	}
	if (opt->getValue( "maxrange" ) != NULL) {
		maxrange = atof( opt->getValue( "maxrange" ) );
	}
	sourceheight = -10;
	if (opt->getValue( "sourceheight" ) != NULL) {
		sourceheight = atof( opt->getValue( "sourceheight" ) );
	}
	maxheight = 150;
	if (opt->getValue( "maxheight" ) != NULL) {
		maxheight = atof( opt->getValue( "maxheight" ) );
	}
	if (opt->getValue( "skips" ) != NULL) {
		int tempskips = atoi( opt->getValue( "skips" ));
		maxskips = tempskips < 0 ? 0 : (unsigned int)tempskips;
	}
	if (opt->getValue( "wind_units" ) != NULL) {
		if (strcmp(opt->getValue( "wind_units" ),"kmpersec") == 0) {
			inMPS = false;
		}
	}
	
	// Process azimuth vector.  Since azimuths can wrap around from 360 to 0, we can't
	// easily put it into a for loop.
	int naz = 0;
	double *azvec;
	if (filetype != SLICEFILE) {
		bool azwraps = (maxazimuth < azimuth0);
		if (azwraps) {
			maxazimuth += 360.0;
		}
		naz = (int)floor((maxazimuth-azimuth0)/dazimuth + 0.5) + 1;
		azvec = new double[ naz ];
		int i = 0;
		for (double d = azimuth0; d <= maxazimuth; d += dazimuth) {
			azvec[ i++ ] = (d >= 360.0 ? d - 360.0 : d );
		}
		if (azwraps) {
			maxazimuth -= 360.0;
		}
	}
	

	// Flags
	//bool rangeDependent = !opt->getFlag( "norange" );
	bool rangeDependent = false; // this will be set to true depending on the type of atmosphere used
	bool partial = opt->getFlag( "partial" );
	
	// Initialize the atmospheric profile
	AtmosphericSpecification *spec = 0;
	
	// Declare these but don't allocate yet.  We'll only use one of them, but if we declare
	// them inside the switch statement then they go out of scope too fast.
	JetProfile *jet;
	SampledProfile *sound;
	Slice *slice;
	string order;
	int skiplines;
	
	switch (filetype) {
		case JETFILE:
			jet = new JetProfile( atmosfile );
			spec = new Sounding( jet );
			break;
			
		case ATMOSFILE:
			skiplines = 0;
			if (opt->getValue("skiplines") != NULL) {
				skiplines = atoi( opt->getValue( "skiplines" ) );
			}
			if (opt->getValue( "atmosfileorder" ) != NULL) {
				order = opt->getValue( "atmosfileorder" );
			} else {
				delete opt;
				throw invalid_argument( "Option --atmosfileorder is required for ATMOSFILE files!" );
			}
			sound = new SampledProfile( atmosfile, order.c_str(), skiplines, inMPS );  // Will be deleted when spec is deleted
			//sound->resample( 0.01 * stepsize );
			spec = new Sounding( sound );
			break;
			
		case SLICEFILE:
			slice = new Slice();
			slice->readSummaryFile( atmosfile );
			slice->strict(true);
			spec = slice;
			naz = 1;
			azvec = new double[ 1 ];
			azvec[ 0 ] = normalizeAzimuth( slice->pathAzimuth() );
			rangeDependent = true;
			break;
	}

	// Adjust the source height to ground level
	if (sourceheight < spec->z0(0,0)) {
		sourceheight = spec->z0(0,0);
	}
	
	// semi-secret flag: output the atmosphere on a 1-meter resolution if requested
	if (opt->getFlag( "atmosout" )) {
		ofstream atmosoutfile("atmospheric_profile.out",ofstream::out);
		atmosoutfile << "z t u v w p rho ceff dtdz dudz dvdz dwdz dpdz drhodz dtdx dudx dvdx dwdx dpdx drhodx dtdy dudy dvdy dwdy dpdy drhody" << endl;
		for (double za = sourceheight; za <= 150.0; za += 0.001) {
			atmosoutfile << 
				za << " " << 
				spec->t(0,0,za) << " " << 
				spec->u(0,0,za) << " " << 
				spec->v(0,0,za) << " " << 
				spec->w(0,0,za) << " " << 
				spec->p(0,0,za) << " " << 
				spec->rho(0,0,za) << " " << 
				spec->c0(0,0,za) << " " <<
				spec->dtdz(0,0,za) << " " << 
				spec->dudz(0,0,za) << " " << 
				spec->dvdz(0,0,za) << " " << 
				spec->dwdz(0,0,za) << " " << 
				spec->dpdz(0,0,za) << " " << 
				spec->drhodz(0,0,za) << " " << 
				spec->dtdx(0,0,za) << " " << 
				spec->dudx(0,0,za) << " " << 
				spec->dvdx(0,0,za) << " " << 
				spec->dwdx(0,0,za) << " " << 
				spec->dpdx(0,0,za) << " " << 
				spec->drhodx(0,0,za) << " " << 
				spec->dtdy(0,0,za) << " " << 
				spec->dudy(0,0,za) << " " << 
				spec->dvdy(0,0,za) << " " << 
				spec->dwdy(0,0,za) << " " << 
				spec->dpdy(0,0,za) << " " << 
				spec->drhody(0,0,za) << " " << endl;
				
		}
		atmosoutfile.close();
	}
	



	// Calculation-related variables
	int k = 0;                                 //use k to track loop progressions
	int steps = maxraylength/stepsize;   //set number of steps for the calculation
	double range, turningHeight;

	// Set up break conditions
	//ODESystemBreakCondition *condition1 = 0, *condition2 = 0, *condition3 = 0, *condition4 = 0;
	ODESystemBreakCondition *condition;
	vector< ODESystemBreakCondition * > breakConditions;
	ReflectionCondition2D *bouncer = new ReflectionCondition2D( spec, azimuth0, maxskips );
	//condition1 = new MinimumBreakCondition( 1, 0, "Ray hit the ground." );
	breakConditions.push_back( bouncer );
	/*
		condition = new HitGroundCondition( spec, 0, -1, 1, "Ray hit the ground." );
		breakConditions.push_back( condition );
	*/
	//condition = new UpwardRefractionCondition( 1, "Ray turned back upward", skips );
	//breakConditions.push_back( condition );
	condition = new MaximumBreakCondition( 1, maxheight, "Ray entering thermosphere...stopping solver." );
	breakConditions.push_back( condition );
        condition = new MaximumRangeCondition( 0, -1, 1, maxrange, "Ray reached maximum length." );
	breakConditions.push_back( condition );

	
	// Initialize the solution matrix.  We'll only have to do this once, and we won't
	// need to zero it out between runs because the solver doesn't add to what's 
	// already there.  The solver returns the number of steps taken, so we don't have
	// to worry about accidentally running over into old solutions either.
	//double c0 = profile->c( x, y, zmin, az );
	double *initialConditions;
	int zindex;   // which variable is z?
	Acoustic2DEquationSet *equations = new Acoustic2DEquationSet( spec, elev0, azimuth0, rangeDependent );
	initialConditions = new double[ 6 ];
	zindex = 1;

	// Pass the equations on to the System solver
	//ODESystem *system = new GSL_ODESystem( equations );
	ODESystem *system = new ODESystem( equations );
	
	double **solution = new double*[ steps + 1 ];
        for (int i = 0; i <= steps; i++) {
                solution[ i ] = new double[ equations->numberOfEquations() ];
        }

        // Output to screen the parameters under which we'll be working
	cout 	<< "Ray Trace Parameters:" << endl
		<< "Atmospheric File Name: " << atmosfile << endl
		<< "Maximum Height: " << maxheight << " km" << endl;
	if (naz > 1) {
		cout << "Launch Azimuth: [" << azvec[ 0 ] << "," << dazimuth << "," << azvec[ naz-1 ] 
			<< "]" << endl;
	} else {
		cout << "Launch Azimuth: " << azvec[ 0 ] << endl;
	}
	if (delev > 0) {
		cout << "Launch Elevation: [" << rad2deg(elev0) << "," << rad2deg(delev) << "," << rad2deg(maxelev) << "]" << endl;
	} else {
		cout << "Launch Elevation: " << rad2deg(elev0) << endl;
	}
	cout << endl;
	cout << "Starting calculation..." << endl;

	// loop through the range of elevation angles and azimuths
	for (int azind = 0; azind < naz; azind++) {

		equations->changeAzimuth( azvec[ azind ] );
		bouncer->setLaunchAzimuthDegrees( azvec[ azind ] );
		double c0 = spec->ceff( 0, 0, sourceheight, azvec[ azind ] );

		for (double theta = elev0; theta <= maxelev; theta += delev) {

			// Set up the system of equations to be solved
			double raymin = 0.0;
			equations->changeTakeoffAngle( theta );
			bouncer->setLaunchAngleRadians( theta );
	
			// Initial conditions change when you change the takeoff or azimuth angles
			//equations->setupInitialConditions( initialConditions, sourceheight );
			equations->setupInitialConditions( initialConditions, sourceheight, c0 );
			cout << endl << endl << "Calculating theta = " << rad2deg(theta) << " degrees..." << endl;
			
			// the magic happens here
			k = system->rk4( equations, solution, steps, initialConditions, raymin, maxraylength, breakConditions ); 
			
			// get set up to analyze the results for turning height, amplitude, etc.
			bool therm = false;
			ostringstream fileholder("");
			ostringstream currentSkip("");
			turningHeight = 0.0;
			//int caustics = 0;
			//double *jac = new double[ k+1 ];
			double *amp = new double[ k+1 ];
			double A0 = 0.0, dist1km = 1e15;
			int k_end = 0;
			unsigned int skips = 0;
			
			// Iterate through the solution to output the raypath to a file and do some summary calculations
			for (int i = 0; i <= k; i++) {
				
				if (i > 1 && solution[ i-2 ][ 1 ] > solution[ i-1 ][ 1 ] && solution[ i ][ 1 ] > solution[ i-1 ][ 1 ]) {
					fileholder << currentSkip.str();
					currentSkip.str("");
					k_end = i-1;
					skips++;
				}
				
				// Output (r,z) or (x,y) to buffer
				currentSkip << solution[ i ][ 0 ] << "   " << solution[ i ][ 1 ];
				
				// Calculate the relative amplitude
				amp[ i ] = equations->calculateAmplitude(solution, i);

					
				// Output the relative amplitude to the raypath file
				currentSkip << "   " << amp[ i ];
				
				// Get distance from 1km reference point and see if it's the closest point yet
				if (fabs( solution[i][0] - 1 ) < dist1km) {
					dist1km = fabs(solution[i][0] - 1);
					A0 = amp[ i ];
				}
				currentSkip << endl;
				
				// Check for the highest point in the raypath
				if (solution[i][zindex] > turningHeight) {
					turningHeight = solution[i][zindex];
				}
			}
			
			// report partially-complete bounces if requested
			if (partial || k_end == 0 || bouncer->triggered()) {
				fileholder << currentSkip.str();
				currentSkip.str("");
				k_end = k;
			}
			
			// If the ray died in the thermosphere, there is no applicable range
			if(turningHeight > maxheight || solution[ k ][zindex] > maxheight){
				range = 0.0;
				therm  = true;
			} else {
				// otherwise read or calculate the range of the endpoint
				range = solution[k_end][0];
			}

			// name and open the raypath file
			char pathfile[4096];
			sprintf(pathfile,"raypath_az%06.2f_elev%06.2f.txt",azvec[azind],rad2deg(theta));
			ofstream raypath(pathfile,ios_base::out);

			double tau = equations->calculateTravelTime( solution, k_end, stepsize, azvec[ azind ] );
			double A = amp[ k_end ];
			//double dB = 20 * log10( A / A0 );
			double refDist = 1.0;   // normalize amplitudes to 1 km
			double dB = equations->transmissionLoss( solution, k_end, refDist );
			//double arrival_az = 0, deviation = 0;
			/*
			if (in3d) {
				arrival_az = ((Acoustic3DEquationSet *)equations)->calculateArrivalAzimuth( solution, k, 5 );
				deviation = arrival_az - azvec[ azind ];
				if (deviation > 180.0)
					deviation -= 360.0;
			}
			*/
			
			// Output summary to screen for immediate sanity check
	        	cout << "Ray trace for angle theta = " << theta*180/Pi << " completed." << endl
				<< "Max Turning Height: " << turningHeight << " km" << endl
				<< "Skips: " << bouncer->countBounces() << endl;
			if (!therm) {
		     		cout << "Range: " << range << " km" << endl
				<< "Travel Time: " << tau << " seconds (" << tau / 60.0 << " minutes)" << endl
				<< "Celerity: " << range / tau << " km/s" << endl;
				
				if (spec->rho(0,0,sourceheight) > 0) {
					cout << "Transmission Loss re ~1km: " << dB << " dB" << endl;
					//cout << "Amplitude: " << A << " Pa (" << dB1 << " dB transmission loss re " << refDist << " km)" << endl;
				}
			}
			
			// Output quantities to file
			raypath << "# Azimuth: " << azvec[ azind ] << endl
				<< "# Elevation: " << theta * 180/Pi << endl;
			if (!therm) {
				raypath << "# Turning Height: " << turningHeight << endl
					<< "# Range: " << range << endl 
					<< "# Travel Time: " << tau << endl
					<< "# Celerity: " << range/tau << endl
					<< "# Skips: " << bouncer->countBounces() << endl;
				
				if (spec->rho(0,0,sourceheight) > 0) {
					raypath << "# Final Amplitude: " << A << endl
						<< "# Amplitude @ ~1km: " << A0 << endl
						<< "# Transmission Loss (re 1 km): " << dB << endl;
				}
			}
			
			// Now output the raypath itself
			raypath << fileholder.str() << endl;
			
			raypath.close();
			bouncer->reset();
			//delete [] jac;
			delete [] amp;
			
			// If we died in the thermosphere, we can stop the calculation
			if (therm) break;

		}
	}

	// Clean up memory allocations
	delete system;
	delete equations;
	for (unsigned int j = 0; j < breakConditions.size(); j++) {
		delete breakConditions[ j ];
	}
	for (int j = 0; j <= steps; j++) {
		delete [] solution[j];
        }
	delete [] solution;
	delete [] azvec;
	delete [] initialConditions;
	
	delete spec;
}


AnyOption *parseInputOptions( int argc, char **argv ) {
	
	
	// parse input options
	AnyOption *opt = new AnyOption();

	opt->addUsage( "Usage: " );
	opt->addUsage( "" );
	opt->addUsage( "The options below can be specified in a colon-separated file \"raytrace.2d.options\" or at the command line.  Command-line options override file options." );
	opt->addUsage( " --help -h                Print this message and exit" );
	//opt->addUsage( " --atmosout               Output the atmosphere and all its derivatives to atmospheric_profile.out");
	opt->addUsage( "" );
	opt->addUsage( "To use an arbitrary 1-D atmospheric profile in ASCII format (space or comma-separated) the following options apply:" );
	opt->addUsage( "REQUIRED (no default values):" );
	opt->addUsage( " --atmosfile  <filename>  Uses an ASCII atmosphere file" );
	opt->addUsage( " --atmosfileorder         The order of the (z,t,u,v,w,p,d) fields in the ASCII file (Ex: 'ztuvpd')" );
	opt->addUsage( " --elev                   Value in range (-90,90)" );
	opt->addUsage( " --azimuth                Value in range [0,360), clockwise from north" );
	opt->addUsage( " --maxraylength           Maximum ray length to calculate (km) [none]" );
	opt->addUsage( "OPTIONAL [defaults]:" );
	opt->addUsage( " --skiplines              Lines at the beginning of the ASCII file to skip [0]" );
	opt->addUsage( " --maxelev                Maximum elevation angle to calculate [--elev value]" );
	opt->addUsage( " --delev                  Elevation angle step [1]" );
	opt->addUsage( " --maxazimuth             Maximum azimuth to calculate [--azimuth value]" );
	opt->addUsage( " --dazimuth               Azimuth angle step [1]" );
	opt->addUsage( " --sourceheight           Height at which to begin raytrace [ground level]" );
	opt->addUsage( " --maxheight              Height at which to cut off calculation [150 km]" );
	opt->addUsage( " --maxrange               Maximum distance from origin to calculate (km) [no maximum]" );
	opt->addUsage( " --stepsize               Ray length step size for computation, km [0.01]" );
	opt->addUsage( " --skips                  Maximum number of skips to allow.  Use 0 for no limits.  [0]" );
	opt->addUsage( " --wind_units             Specify 'kmpersec' if the winds are given in km/s [mpersec]" );
	opt->addUsage( "FLAGS (no value required):" );
	opt->addUsage( " --partial                Report the final, incomplete raypath as well as the complete bounces." );
	opt->addUsage( "" );
/*
	
	opt->addUsage( "To use a set of ASCII files that form a 2-D slice of the atmosphere the following options apply:" );
	opt->addUsage( "REQUIRED (no default values):" );
	opt->addUsage( " --slicefile  <filename>  The name of the summary file describing the path (see documentation)" );
	opt->addUsage( " --elev                   Value in range (-90,90)" );
	opt->addUsage( " --maxraylength           Maximum ray length to calculate (km) [none]" );
	opt->addUsage( "OPTIONAL [defaults]:" );
	opt->addUsage( " --maxelev                Maximum elevation angle to calculate [--elev value]" );
	opt->addUsage( " --delev                  Elevation angle step [1]" );
	opt->addUsage( " --maxazimuth             Maximum azimuth to calculate [--azimuth value]" );
	opt->addUsage( " --dazimuth               Azimuth angle step [1]" );
	opt->addUsage( " --sourceheight           Height at which to begin raytrace [ground level]" );
	opt->addUsage( " --maxheight              Height at which to cut off calculation [150 km]" );
	opt->addUsage( " --maxrange               Maximum distance from origin to calculate (km) [no maximum]" );
	opt->addUsage( " --stepsize               Ray length step size for computation, km [0.01]" );
	opt->addUsage( " --skips                  Maximum number of skips to allow.  Use 0 for no limits.  [0]" );
	opt->addUsage( "FLAGS (no value required):" );
	opt->addUsage( " --partial                Report the final, incomplete raypath as well as the complete bounces." );
	opt->addUsage( "" );
	
	
	
	opt->addUsage( "To use an analytic profile with Gaussian wind jets the following options apply:" );
	opt->addUsage( "REQUIRED (no default values):" );
	opt->addUsage( " --jetfile  <filename>    The parameter file for the profile" );
	opt->addUsage( " --elev                   Value in range (-90,90)" );
	opt->addUsage( " --azimuth                Value in range [0,360), clockwise from north (optional for --envfile)" );
	opt->addUsage( " --maxraylength           Maximum ray length to calculate (km) [none]" );
	opt->addUsage( "OPTIONAL [defaults]:" );
	opt->addUsage( " --maxelev                Maximum elevation angle to calculate [--elev value]" );
	opt->addUsage( " --delev                  Elevation angle step [1]" );
	opt->addUsage( " --maxazimuth             Maximum azimuth to calculate [--azimuth value]" );
	opt->addUsage( " --dazimuth               Azimuth angle step [1]" );
	opt->addUsage( " --sourceheight           Height at which to begin raytrace [ground level]" );
	opt->addUsage( " --maxheight              Height at which to cut off calculation [150 km]" );
	opt->addUsage( " --maxrange               Maximum distance from origin to calculate (km) [no maximum]" );
	opt->addUsage( " --stepsize               Ray length step size for computation, km [0.1]" );
	opt->addUsage( " --skips                  Maximum number of skips to allow.  Use 0 for no limits.  [0]" );
	opt->addUsage( "FLAGS (no value required):" );
	opt->addUsage( " --partial                Report the final, incomplete raypath as well as the complete bounces." );
*/
	opt->addUsage( "" );
	opt->addUsage( "Examples (run from 'samples' directory):" );
	opt->addUsage( "../bin/raytrace.2d --azimuth 90 --elev 1 --delev 1 --maxelev 45 --skips 1 --atmosfile NCPA_canonical_profile_zuvwtdp.dat --atmosfileorder zuvwtdp --maxraylength 800 --maxheight 140" );
	opt->addUsage( "cat raypath_az090* > raypaths.2d.dat" );
	opt->addUsage( "rm raypath_az090*" );

	// Set up the actual flags and stuff
	opt->setFlag( "help", 'h' );
	opt->setFlag( "norange" );
	opt->setFlag( "atmosout" );
	opt->setFlag( "partial" );
	opt->setOption( "slicefile" );
	opt->setOption( "atmosfile" );
	opt->setOption( "jetfile" );
	opt->setOption( "elev" );
	opt->setOption( "azimuth" );
	opt->setOption( "maxraylength" );
	opt->setOption( "lat" );
	opt->setOption( "lon" );
	opt->setOption( "maxrange" );
	opt->setOption( "maxelev" );
	opt->setOption( "delev" );
	opt->setOption( "maxazimuth" );
	opt->setOption( "dazimuth" );
	opt->setOption( "sourceheight" );
	opt->setOption( "maxheight" );
	opt->setOption( "stepsize" );
	opt->setOption( "dZ" );
	opt->setOption( "dR" );
	opt->setOption( "atmosfileorder" );
	opt->setOption( "skiplines" );
	opt->setOption( "skips" );
	opt->setOption( "wind_units" );

	// Process the command-line arguments
	opt->processFile( "./raytrace.2d.options" );
	opt->processCommandArgs( argc, argv );
	
	if( ! opt->hasOptions()) { // print usage if no options
		opt->printUsage();
		delete opt;
		exit( 1 );
        }

	// Check to see if help text was requested
	if ( opt->getFlag( "help" ) || opt->getFlag( 'h' ) ) {
		opt->printUsage();
		exit( 1 );
	}

	return opt;
}
