#include <cmath>
#include "geographic.h"
#include "SampledProfile.h"
#include <stdexcept>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include "gsl/gsl_errno.h"
#include "gsl/gsl_spline.h"


#define GAM 1.4
#define PI 3.14159
#define R 287.0


//#include "ExceptionWithStack.h"
/**
 * @version 1.2.0
 */

NCPA::SampledProfile::SampledProfile() {
	init_();
}

void NCPA::SampledProfile::init_() {
	origin_ = 0;
	nz_ = 0;
	minZ_ = 1e15;
	maxZ_ = -1e15;
	propAz_ = -999;

	z_ = t_ = u_ = v_ = w_ = rho_ = p_ = c0_ = ceff_ = 0;
	
	t_acc = u_acc = v_acc = w_acc = p_acc = rho_acc = c0_acc = ceff_acc = 0;
	t_spline = u_spline = v_spline = w_spline = p_spline = rho_spline = c0_spline = ceff_spline = 0;
	
	good_ = false;
}

void NCPA::SampledProfile::clearOut() {
        if (t_ != 0) 
                delete [] t_;
        if (u_ != 0) 
                delete [] u_;
        if (v_ != 0) 
                delete [] v_;
	if (w_ != 0) 
                delete [] w_;
        if (p_ != 0) 
                delete [] p_;
        if (rho_ != 0) 
                delete [] rho_;
	if (z_ != 0)
		delete [] z_;
	good_ = false;
	delete origin_;
	delete t_acc;
	delete u_acc;
	delete v_acc;
	delete w_acc;
	delete p_acc;
	delete rho_acc;
	delete c0_acc;
	delete ceff_acc;
	delete t_spline;
	delete u_spline;
	delete v_spline;
	delete w_spline;
	delete p_spline;
	delete rho_spline;
	delete c0_spline;
	delete ceff_spline;
}

NCPA::SampledProfile::~SampledProfile() {
	clearOut();
}

NCPA::SampledProfile::SampledProfile( double lat, double lon, int nz, double *z, double *t,
					double *u, double *v, double *w, double *rho, double *p, 
					double z0 ) {

	init_();
	origin_ = new Location( lat, lon );
	nz_ = nz;
	if (z0 < -10000) {
		z0_ = z[ 0 ];
	} else {
		z0_ = z0;
	}

	z_ = new double[ nz_ ];
	t_ = new double[ nz_ ];
	u_ = new double[ nz_ ];
	v_ = new double[ nz_ ];
	w_ = new double[ nz_ ];
	p_ = new double[ nz_ ];
	rho_ = new double[ nz_ ];
	c0_ = new double[ nz_ ];


	for (unsigned int i = 0; i < nz_; i++) {
		z_[ i ] = z[ i ];
		t_[ i ] = t[ i ];
		u_[ i ] = u[ i ];
		v_[ i ] = v[ i ];
		if (w != 0)
			w_[ i ] = w[ i ];
		else
			w_[ i ] = 0.0;
		if (p != 0)
			p_[ i ] = p[ i ];
		else
			p_[ i ] = 0.0;
		if (rho != 0)
			rho_[ i ] = rho[ i ];
		else
			rho_[ i ] = 0.0;
				
		c0_[ i ] = 1.0e-3 * sqrt( GAM * R * t_[ i ] );
		//ceff_[ i ] = c0_[ i ] + std::sqrt( u_[i]*u_[i] + v_[i]*v_[i] ) * std::cos( NCPA::deg2rad(propAz_) - direction );
		
		minZ_ = (z_[i] < minZ_) ? z_[i] : minZ_;
		maxZ_ = (z_[i] > maxZ_) ? z_[i] : maxZ_;
	}

	//dz_min_ = this->dz_min();
	
	initSplines();
	
	good_ = true;
}

NCPA::SampledProfile::SampledProfile( int nz, double *z, double *t,
                                        double *u, double *v, double *w, double *rho, double *p,
                                        double z0 ) {

        init_();
	origin_ = new Location( 0, 0 );
        nz_ = nz;
        if (z0 < -10000) {
                z0_ = z[ 0 ];
        } else {
                z0_ = z0;
        }

        z_ = new double[ nz_ ];
        t_ = new double[ nz_ ];
        u_ = new double[ nz_ ];
        v_ = new double[ nz_ ];
        w_ = new double[ nz_ ];
        p_ = new double[ nz_ ];
        rho_ = new double[ nz_ ];
	c0_ = new double[ nz_ ];


        for (unsigned int i = 0; i < nz_; i++) {
                z_[ i ] = z[ i ];
                t_[ i ] = t[ i ];
                u_[ i ] = u[ i ];
                v_[ i ] = v[ i ];
                if (w != 0)
                        w_[ i ] = w[ i ];
                else
                        w_[ i ] = 0.0;
                if (p != 0)
                        p_[ i ] = p[ i ];
                else
                        p_[ i ] = 0.0;
                if (rho != 0)
                        rho_[ i ] = rho[ i ];
                else
                        rho_[ i ] = 0.0;
		
		
		c0_[ i ] = 1.0e-3 * sqrt( GAM * R * t_[ i ] );
		//ceff_[ i ] = c0_[ i ] + std::sqrt( u_[i]*u_[i] + v_[i]*v_[i] ) * std::cos( NCPA::deg2rad(propAz_) - direction );
		
		minZ_ = (z_[i] < minZ_) ? z_[i] : minZ_;
		maxZ_ = (z_[i] > maxZ_) ? z_[i] : maxZ_;
        }

        //dz_min_ = this->dz_min();
	initSplines();
        good_ = true;
}


NCPA::SampledProfile::SampledProfile( SampledProfile &p ) {
	init_();
	origin_ = new Location( p.lat(), p.lon() );
	nz_ = p.nz();
	z0_ = p.z0();
	
	z_ = new double[ nz_ ];
        t_ = new double[ nz_ ];
        u_ = new double[ nz_ ];
        v_ = new double[ nz_ ];
        w_ = new double[ nz_ ];
        p_ = new double[ nz_ ];
        rho_ = new double[ nz_ ];
	c0_ = new double[ nz_ ];
	ceff_ = new double[ nz_ ];


        for (unsigned int i = 0; i < nz_; i++) {
                z_[ i ] = p.z_[ i ];
                t_[ i ] = p.t_[ i ];
                u_[ i ] = p.u_[ i ];
                v_[ i ] = p.v_[ i ];
                w_[ i ] = p.w_[ i ];
                p_[ i ] = p.p_[ i ];
                rho_[ i ] = p.rho_[ i ];
		
		c0_[ i ] = 1.0e-3 * sqrt( GAM * R * t_[ i ] );
		//ceff_[ i ] = c0_[ i ] + std::sqrt( u_[i]*u_[i] + v_[i]*v_[i] ) * std::cos( NCPA::deg2rad(propAz_) - direction );
		minZ_ = (z_[i] < minZ_) ? z_[i] : minZ_;
		maxZ_ = (z_[i] > maxZ_) ? z_[i] : maxZ_;
        }

        //dz_min_ = this->dz_min();
	initSplines();
	setPropagationAzimuth(p.getPropagationAzimuth());
        good_ = p.good();
}

NCPA::SampledProfile::SampledProfile( std::string filename, const char *order, int skiplines ) {
	init_();

	// check to see if z, t, u, and v are all present in order
	if (!((std::strpbrk(order,"Zz") != NULL) && (std::strpbrk(order,"Tt") != NULL)
		&& (std::strpbrk(order,"Uu") != NULL) && (std::strpbrk(order,"Vv") != NULL))) {
		throw std::invalid_argument( "Z, T, U, and V are all required!" );
	}

	origin_ = new Location( 0, 0 );
	
	std::ifstream instream( filename.c_str(), std::ios_base::in );
	if (!instream.good()) {
		throw std::runtime_error( "Error reading from atmosphere file!" );
	}
	
	char buffer[4096] = "";
	std::string strbuf;
	
	for (int i = 0; i < skiplines; i++) {
		//instream.getline( buffer, 4096 );
		safe_getline( instream, strbuf );
		if (!instream.good()) {
			throw std::runtime_error( "Error reading from atmosphere file!" );
		}
	}
	nz_ = 0;
	std::streampos getpos = instream.tellg();
	
	//while (instream.getline( buffer, 4096 ) && (!instream.fail())) {
	while (instream.good()) {
		safe_getline( instream, strbuf );
		if (deblank(strbuf).length() > 0) {
			nz_++;
		}
	}
	
	if (nz_ == 0) {
		clearOut();
		throw std::runtime_error( "Couldn't construct atmosphere! Check the profile file format." );
	}
	
	/*do {
		memset(buffer,'\0',4096);
		instream.getline( buffer, 4096 );
		if (strlen(buffer) > 0)
			nz_++;
	} while (!instream.eof());
	*/

        z_ = new double[ nz_ ];
        t_ = new double[ nz_ ];
        u_ = new double[ nz_ ];
        v_ = new double[ nz_ ];
        w_ = new double[ nz_ ];
        p_ = new double[ nz_ ];
        rho_ = new double[ nz_ ];
	c0_ = new double[ nz_ ];

	//instream.seekg( 0 );
	instream.close();
	instream.open( filename.c_str(), std::ios_base::in );
	//for (int i = 0; i < skiplines; i++) {
        //        instream.getline( buffer, 4096 );
        //}
	instream.seekg( getpos );
	char *tok;
	unsigned int tokens = 0;
	bool z = false, t = false, u = false, v = false;
	unsigned int nfields = std::strlen( order );
	for (unsigned int i = 0; i < nz_; i++) {
		tokens = 0;
		int strpos = 0;
		memset( buffer, '\0', 4096 );
		safe_getline( instream, strbuf );
		std::strcpy( buffer, strbuf.c_str() );   // turn into char* for strtok to operate on
		//instream.getline( buffer, 4096 );
		tok = std::strtok( buffer, " ," );
		while (tok != NULL) {
			tokens++;
			if (tokens <= std::strlen( order )) {
				switch (order[ strpos++ ]) {
					case 'z':
					case 'Z':
						z_[ i ] = std::atof( tok );
						z = true;
						break;
					case 't':
					case 'T':
						t_[ i ] = std::atof( tok );
						t = true;
						break;
					case 'U':
					case 'u':
						u_[ i ] = std::atof( tok );
						u = true;
						break;
					case 'V':
					case 'v':
						v_[ i ] = std::atof( tok );
						v = true;
						break;
					case 'W':
					case 'w':
						w_[ i ] = std::atof( tok );
						break;
					case 'P':
					case 'p':
						p_[ i ] = std::atof( tok );
						break;
					case 'D':
					case 'd':
						rho_[ i ] = std::atof( tok );
						break;
					default:
						std::ostringstream oss("");
						oss << "Unknown data code provided: " << order[ strpos-1 ];
						throw std::invalid_argument( oss.str() );
				}
			}
			tok = strtok( NULL, " ," );
		}
		if (tokens != nfields) {
			std::ostringstream tok_oss("");
			tok_oss << "Number of tokens in line does not match the number of reported inputs!" << std::endl
				<< "Line in question: " << strbuf << std::endl;
			
			throw std::invalid_argument( tok_oss.str() );
		}
	}

	instream.close();
	if (!z || !u || !v || !t) {
		throw std::invalid_argument( "Z, U, V, and T are all required!" );
	}
	
	// fill with zeroes anything that wasn't specified
	char *trimmed = new char[ tokens + 1 ];
	std::strncpy( trimmed, order, tokens );
	trimmed[ tokens ] = '\0';
	for (unsigned int i = 0; i < nz_; i++) {
		
		//double direction = PI/2 - std::atan2( v_[i], u_[i] );
		//while (direction < 0) {
		//	direction += 2 * PI;
		//}
		
		c0_[ i ] = 1.0e-3 * sqrt( GAM * R * t_[ i ] );
		//ceff_[ i ] = c0_[ i ] + std::sqrt( u_[i]*u_[i] + v_[i]*v_[i] ) * std::cos( NCPA::deg2rad(propAz_) - direction );
		if (std::strpbrk( trimmed, "wW" ) == NULL) {
			w_[ i ] = 0;
		}
		if (std::strpbrk( trimmed, "pP" ) == NULL) {
			p_[ i ] = 0;
		}
		if (std::strpbrk( trimmed, "dD" ) == NULL) {
			rho_[ i ] = 0;
		}
		
		minZ_ = (z_[i] < minZ_) ? z_[i] : minZ_;
		maxZ_ = (z_[i] > maxZ_) ? z_[i] : maxZ_;
	}
	z0_ = z_[ 0 ];
	//dz_min_ = this->dz_min();
	initSplines();
	good_ = true;
}

void NCPA::SampledProfile::initSplines() {
	
	t_acc = gsl_interp_accel_alloc();
	t_spline = gsl_spline_alloc( gsl_interp_cspline, nz_ );
	u_acc = gsl_interp_accel_alloc();
	u_spline = gsl_spline_alloc( gsl_interp_cspline, nz_ );
	v_acc = gsl_interp_accel_alloc();
	v_spline = gsl_spline_alloc( gsl_interp_cspline, nz_ );
	w_acc = gsl_interp_accel_alloc();
	w_spline = gsl_spline_alloc( gsl_interp_cspline, nz_ );
	p_acc = gsl_interp_accel_alloc();
	p_spline = gsl_spline_alloc( gsl_interp_cspline, nz_ );
	rho_acc = gsl_interp_accel_alloc();
	rho_spline = gsl_spline_alloc( gsl_interp_cspline, nz_ );
	c0_acc = gsl_interp_accel_alloc();
	c0_spline = gsl_spline_alloc( gsl_interp_cspline, nz_ );
	ceff_acc = gsl_interp_accel_alloc();
	ceff_spline = gsl_spline_alloc( gsl_interp_cspline, nz_ );
	
	gsl_spline_init( t_spline, z_, t_, nz_ );
	gsl_spline_init( u_spline, z_, u_, nz_ );
	gsl_spline_init( v_spline, z_, v_, nz_ );
	gsl_spline_init( w_spline, z_, w_, nz_ );
	if (p_ != 0)
		gsl_spline_init( p_spline, z_, p_, nz_ );
	if (rho_ != 0)
		gsl_spline_init( rho_spline, z_, rho_, nz_ );
	if (c0_ != 0)
		gsl_spline_init( c0_spline, z_, c0_, nz_ );
	if (ceff_ != 0)
		gsl_spline_init( ceff_spline, z_, ceff_, nz_ );
}

double NCPA::SampledProfile::getPropagationAzimuth() const {
	return propAz_;
}

void NCPA::SampledProfile::setPropagationAzimuth( double newPropAz ) {
	if (newPropAz != propAz_) {
		delete [] ceff_;
		ceff_ = new double[ nz_ ];
		
		propAz_ = newPropAz;
		for (unsigned int i = 0; i < nz_; i++) {
			double direction = PI/2 - std::atan2( v_[i], u_[i] );
			while (direction < 0.0) {
				direction += 2 * PI;
			}
		
			ceff_[ i ] = c0_[ i ] + std::sqrt( u_[i]*u_[i] + v_[i]*v_[i] ) * std::cos( NCPA::deg2rad(propAz_) - direction );
		}
		
		gsl_spline_init( ceff_spline, z_, ceff_, nz_ );
	}
}

unsigned int NCPA::SampledProfile::z2ind_( double z_in ) const {
	
	// Use the bisection algorithm from Numerical Recipes
	int bottom, top, middle;
	bool ascending = (z_[ nz_-1 ] >= z_[0]);
	bottom = 0;
	top = nz_ - 1;
	if (z_in < z_[0])
                return 0;
	
	while (top-bottom > 1) {
		middle = (top + bottom) >> 1;
		if ((z_in >= z_[ middle ]) == ascending)
			bottom = middle;
		else
			top = middle;
	}
	
	int floor = NCPA::max( 0, NCPA::min( nz_-1, bottom ) );
	if (fabs(z_in - z_[ floor ]) < fabs(z_in - z_[floor+1])) {
		return floor;
	} else {
		return floor+1;
	}
	
	/*
        unsigned int index = 0;
        double bottom, top;
        

        for (index = 0; index < (nz_-1); index++) {
                if (index == 0) {
                        bottom = z_[0];
                } else {
                        bottom = (z_[index] + z_[index-1]) / 2;
                }
                top = (z_[index] + z_[index+1]) / 2;
                if (z_in >= bottom && z_in < top)
                        return index;
        }
        return nz_-1;
	*/
}

int NCPA::SampledProfile::nz() const {
        return nz_;
}

/*
double NCPA::SampledProfile::dz_min() const {
	double dz = 1e10;
	for (int i = 1; i < nz_; i++) {
		dz = (z_[i]-z_[i-1] < dz) ? z_[i]-z_[i-1] : dz;
	}
	return dz;
}
*/

unsigned int NCPA::SampledProfile::z2ind_floor_( double z_in ) const {
		
	// Use the bisection algorithm from Numerical Recipes
	int bottom, top, middle;
	bool ascending = (z_[ nz_-1] >= z_[0]);
	bottom = 0;
	top = nz_ - 1;
	if (z_in < z_[0])
                return 0;
	
	while (top-bottom > 1) {
		middle = (top + bottom) >> 1;
		if ((z_in >= z_[ middle ]) == ascending)
			bottom = middle;
		else
			top = middle;
	}
	
	return NCPA::max( 0, NCPA::min( nz_-1, bottom ) );
	
	/*
        unsigned int index = 0;
        for (index = 1; index < (unsigned int)nz_; index++) {
                if (z_[ index ] > z_in) {
                        return index - 1;
                }
        }
        return nz_-1;
	*/
}


double NCPA::SampledProfile::lat() const { return origin_->lat(); }
double NCPA::SampledProfile::lon() const { return origin_->lon(); }
double NCPA::SampledProfile::z0() { 
	return z0_;
}
/*
unsigned int NCPA::SampledProfile::firstValidIndex() {
	return z2ind_( z0() ) + 1;
}
*/

double NCPA::SampledProfile::t( double z ) {
	if ((!good_) || (t_ == 0)) {
		std::runtime_error e( "Profile is not ready!" );
		throw e;
	} else if (z < minZ_ ) {
		return t_[0];
	} else if (z > maxZ_) {
		std::ostringstream es("Requested altitude ");
		es << z;
		es << " is out of bounds in t()!";
		std::range_error e2( es.str() );
		throw e2;
	} else {
		//return t_[ z2ind_( z ) ];
		return gsl_spline_eval( t_spline, z, t_acc );
	}
}

double NCPA::SampledProfile::u( double z ) {
	if ((!good_) || (u_ == 0)) {
		std::runtime_error e( "Profile is not ready!" );
		throw e;
	} else if (z < minZ_ ) {
		return u_[0];
	} else if ( z > maxZ_) {
		std::ostringstream es("Requested altitude ");
		es << z;
		es << " is out of bounds in u()!";
		std::range_error e2( es.str() );
		throw e2;
	} else {
		return gsl_spline_eval( u_spline, z, u_acc );
	}
}

double NCPA::SampledProfile::v( double z ) {
	if ((!good_) || (v_ == 0)) {
		std::runtime_error e( "Profile is not ready!" );
		throw e;
	} else if (z < minZ_ ) {
		return v_[0];
	} else if ( z > maxZ_) {
		std::ostringstream es("Requested altitude ");
		es << z;
		es << " is out of bounds in v()!";
		std::range_error e2( es.str() );
		throw e2;
	} else {
		return gsl_spline_eval( v_spline, z, v_acc );
	}
}

double NCPA::SampledProfile::w( double z ) {
	if ((!good_) || (w_ == 0)) {
		std::runtime_error e( "Profile is not ready!" );
		throw e;
	} else if (z < minZ_ ) {
		return w_[0];
	} else if ( z > maxZ_) {
		std::ostringstream es("Requested altitude ");
		es << z;
		es << " is out of bounds in w()!";
		std::range_error e2( es.str() );
		throw e2;
	} else {
		return gsl_spline_eval( w_spline, z, w_acc );
	}
}

double NCPA::SampledProfile::p( double z ) {
	if ((!good_) || (p_ == 0)) {
		std::runtime_error e( "Profile is not ready!" );
		throw e;
	} else if (z < minZ_ ) {
		return p_[0];
	} else if ( z > maxZ_) {
		std::ostringstream es("Requested altitude ");
		es << z;
		es << " is out of bounds in p()!";
		std::range_error e2( es.str() );
		throw e2;
	} else {
		return gsl_spline_eval( p_spline, z, p_acc );
	}
}

double NCPA::SampledProfile::rho( double z ) {
	if ((!good_) || (rho_ == 0)) {
		std::runtime_error e( "Profile is not ready!" );
		throw e;
	} else if (z < minZ_ ) {
		return rho_[0];
	} else if ( z > maxZ_) {
		std::ostringstream es("Requested altitude ");
		es << z;
		es << " is out of bounds in rho()!";
		std::range_error e2( es.str() );
		throw e2;
	} else {
		return gsl_spline_eval( rho_spline, z, rho_acc );
	}
}


double NCPA::SampledProfile::c0( double z ) {
	if ((!good_) || (c0_ == 0)) {
		std::runtime_error e( "Profile is not ready!" );
		throw e;
	} else if (z < minZ_ ) {
		return c0_[0];
	} else if ( z > maxZ_) {
		std::ostringstream es("Requested altitude ");
		es << z;
		es << " is out of bounds in c0()!";
		std::range_error e2( es.str() );
		throw e2;
	} else {
		return gsl_spline_eval( c0_spline, z, c0_acc );
	}
}


double NCPA::SampledProfile::ceff( double z, double phi ) {
	this->setPropagationAzimuth(phi);
	if ((!good_) || (ceff_ == 0)) {
		std::runtime_error e( "Profile is not ready!" );
		throw e;
	} else if (z < minZ_ ) {
		return ceff_[0];
	} else if ( z > maxZ_) {
		std::ostringstream es("Requested altitude ");
		es << z;
		es << " is out of bounds in ceff()!";
		std::range_error e2( es.str() );
		throw e2;
	} else {
		return gsl_spline_eval( ceff_spline, z, ceff_acc );
	}
}

double NCPA::SampledProfile::z( unsigned int index ) const {
	if (index < 0 || index >= nz_) {
		std::range_error e("Index out of bounds!");
		throw e;
	}

	return z_[ index ];
}

double NCPA::SampledProfile::d_dz( double z, double *q ) {
/*
	unsigned int zind = z2ind_floor_( z );
	if (zind+1 == nz_)
		//return 0;
		zind--;
	if (z == this->z( zind ))  // if we're right on a boundary, average the derivatives immediately on either side
		return 0.5 * (this->d_dz( z + dz_min_*0.5, q ) + this->d_dz( z - dz_min_*0.5, q ));

	double dz = z_[ zind+1 ] - z_[ zind ];
	double dq = q[ zind+1 ] - q[ zind ];
	return dq / dz;
*/
	unsigned int zind = z2ind_( z );
	double dz, dq, d1, d2;
	if (zind == 0) {
			dz = z_[ 1 ] - z_[ 0 ];
			dq = q[ 1 ] - q[ 0 ];
			return dq/dz;
	} else if (zind == nz_-1) {
			dz = z_[ nz_-1 ] - z_[ nz_-2 ];
			dq = q[ nz_-1 ] - q[ nz_-2 ];
			return dq/dz;
	} else {
			d1 = (q[zind+1] - q[zind]) / (z_[zind+1] - z_[zind]);
			d2 = (q[zind] - q[zind-1]) / (z_[zind] - z_[zind-1]);
			return (d1+d2)/2;
	}
			
}

double NCPA::SampledProfile::dd_dzdz( double z, double *q ) {
/*
	unsigned int zind = z2ind_floor_( z );
	if (zind+1 == nz_)
                //return 0;
		zind--;
	if (z == this->z( zind ))  // if we're right on a boundary, average the derivatives immediately on either side
                return 0.5 * (this->dd_dzdz( z + dz_min_*0.5, q ) + this->dd_dzdz( z - dz_min_*0.5, q ));

	double dz = z_[ zind+1 ] - z_[ zind ];
	double ddqdz = d_dz( z_[ zind+1 ], q ) - d_dz(z_[ zind ], q );
	return ddqdz / dz;
*/
	unsigned int zind = z2ind_( z );
        double dz, dq;
	if (zind == 0) {
			dz = z_[1] - z_[0];
			dq = q[ 2 ] - 2*q[ 1 ] + q[ 0 ];
			return dq / (dz*dz);
	} else if (zind == nz_-1) {
			dz = z_[ nz_-1 ] - z_[ nz_-2 ];
			dq = q[ nz_-1 ] - 2*q[ nz_-2 ] + q[ nz_-3 ];
			return dq / (dz*dz);
	} else {
			dz = z_[zind+1] + z_[zind];
			dq = q[ zind+1 ] - 2*q[ zind ] + q[ zind-1 ];
			return dq / (dz*dz);
	}
}

double NCPA::SampledProfile::dtdz( double z ) {
	if ((!good_) || (t_ == 0)) {
		std::runtime_error e( "Profile is not ready!" );
		throw e;
	} else if (z < minZ_ ) {
		return 0;
	} else if ( z > maxZ_) {
		std::ostringstream es("Requested altitude ");
		es << z;
		es << " is out of bounds in dtdz()!";
		std::range_error e2( es.str() );
		throw e2;
	} else {
		return gsl_spline_eval_deriv( t_spline, z, t_acc );
	}
}

double NCPA::SampledProfile::dudz( double z ) {
	if ((!good_) || (u_ == 0)) {
		std::runtime_error e( "Profile is not ready!" );
		throw e;
	} else if (z < minZ_ ) {
		return 0;
	} else if ( z > maxZ_) {
		std::ostringstream es("Requested altitude ");
		es << z;
		es << " is out of bounds in dudz()!";
		std::range_error e2( es.str() );
		throw e2;
	} else {
		return gsl_spline_eval_deriv( u_spline, z, u_acc );
	}
}

double NCPA::SampledProfile::dvdz( double z ) {
	if ((!good_) || (v_ == 0)) {
		std::runtime_error e( "Profile is not ready!" );
		throw e;
	} else if (z < minZ_ ) {
		return 0;
	} else if ( z > maxZ_) {
		std::ostringstream es("Requested altitude ");
		es << z;
		es << " is out of bounds in dvdz()!";
		std::range_error e2( es.str() );
		throw e2;
	} else {
		return gsl_spline_eval_deriv( v_spline, z, v_acc );
	}
}
	
double NCPA::SampledProfile::dwdz( double z ) {
	if ((!good_) || (w_ == 0)) {
		std::runtime_error e( "Profile is not ready!" );
		throw e;
	} else if (z < minZ_ ) {
		return 0;
	} else if ( z > maxZ_) {
		std::ostringstream es("Requested altitude ");
		es << z;
		es << " is out of bounds in dwdz()!";
		std::range_error e2( es.str() );
		throw e2;
	} else {
		return gsl_spline_eval_deriv( w_spline, z, w_acc );
	}
}

double NCPA::SampledProfile::dpdz( double z ) {
	if ((!good_) || (p_ == 0)) {
		std::runtime_error e( "Profile is not ready!" );
		throw e;
	} else if (z < minZ_ ) {
		return 0;
	} else if ( z > maxZ_) {
		std::ostringstream es("Requested altitude ");
		es << z;
		es << " is out of bounds in dpdz()!";
		std::range_error e2( es.str() );
		throw e2;
	} else {
		return gsl_spline_eval_deriv( p_spline, z, p_acc );
	}
}

double NCPA::SampledProfile::drhodz( double z ) {
	if ((!good_) || (rho_ == 0)) {
		std::runtime_error e( "Profile is not ready!" );
		throw e;
	} else if (z < minZ_ ) {
		return 0;
	} else if ( z > maxZ_) {
		std::ostringstream es("Requested altitude ");
		es << z;
		es << " is out of bounds in drhodz()!";
		std::range_error e2( es.str() );
		throw e2;
	} else {
		return gsl_spline_eval_deriv( rho_spline, z, rho_acc );
	}
}

double NCPA::SampledProfile::ddtdzdz( double z ) {
	if ((!good_) || (t_ == 0)) {
		std::runtime_error e( "Profile is not ready!" );
		throw e;
	} else if (z < minZ_ ) {
		return 0;
	} else if ( z > maxZ_) {
		std::ostringstream es("Requested altitude ");
		es << z;
		es << " is out of bounds in ddtdzdz()!";
		std::range_error e2( es.str() );
		throw e2;
	} else {
		return gsl_spline_eval_deriv2( t_spline, z, t_acc );
	}
}

double NCPA::SampledProfile::ddudzdz( double z ) {
	if ((!good_) || (u_ == 0)) {
		std::runtime_error e( "Profile is not ready!" );
		throw e;
	} else if (z < minZ_ ) {
		return 0;
	} else if ( z > maxZ_) {
		std::ostringstream es("Requested altitude ");
		es << z;
		es << " is out of bounds in ddudzdz()!";
		std::range_error e2( es.str() );
		throw e2;
	} else {
		return gsl_spline_eval_deriv2( u_spline, z, u_acc );
	}
}

double NCPA::SampledProfile::ddvdzdz( double z ) {
	if ((!good_) || (v_ == 0)) {
		std::runtime_error e( "Profile is not ready!" );
		throw e;
	} else if (z < minZ_ ) {
		return 0;
	} else if ( z > maxZ_) {
		std::ostringstream es("Requested altitude ");
		es << z;
		es << " is out of bounds in ddvdzdz()!";
		std::range_error e2( es.str() );
		throw e2;
	} else {
		return gsl_spline_eval_deriv2( v_spline, z, v_acc );
	}
}

double NCPA::SampledProfile::ddwdzdz( double z ) {
	if ((!good_) || (w_ == 0)) {
		std::runtime_error e( "Profile is not ready!" );
		throw e;
	} else if (z < minZ_ ) {
		return 0;
	} else if ( z > maxZ_) {
		std::ostringstream es("Requested altitude ");
		es << z;
		es << " is out of bounds in ddwdzdz()!";
		std::range_error e2( es.str() );
		throw e2;
	} else {
		return gsl_spline_eval_deriv2( w_spline, z, w_acc );
	}
}

double NCPA::SampledProfile::ddpdzdz( double z ) {
	if ((!good_) || (p_ == 0)) {
		std::runtime_error e( "Profile is not ready!" );
		throw e;
	} else if (z < minZ_ ) {
		return 0;
	} else if ( z > maxZ_) {
		std::ostringstream es("Requested altitude ");
		es << z;
		es << " is out of bounds in ddpdzdz()!";
		std::range_error e2( es.str() );
		throw e2;
	} else {
		return gsl_spline_eval_deriv2( p_spline, z, p_acc );
	}
}

double NCPA::SampledProfile::ddrhodzdz( double z ) {
	if ((!good_) || (rho_ == 0)) {
		std::runtime_error e( "Profile is not ready!" );
		throw e;
	} else if (z < minZ_ ) {
		return 0;
	} else if ( z > maxZ_) {
		std::ostringstream es("Requested altitude ");
		es << z;
		es << " is out of bounds in ddrhodzdz()!";
		std::range_error e2( es.str() );
		throw e2;
	} else {
		return gsl_spline_eval_deriv2( rho_spline, z, rho_acc );
	}
}

double NCPA::SampledProfile::dceffdz( double z, double phi ) {
	this->setPropagationAzimuth(phi);
	if ((!good_) || (ceff_ == 0)) {
		std::runtime_error e( "Profile is not ready!" );
		throw e;
	} else if (z < minZ_ ) {
		return 0;
	} else if ( z > maxZ_) {
		std::ostringstream es("Requested altitude ");
		es << z;
		es << " is out of bounds in dceffdz()!";
		std::range_error e2( es.str() );
		throw e2;
	} else {
		return gsl_spline_eval_deriv( ceff_spline, z, ceff_acc );
	}
}

double NCPA::SampledProfile::ddceffdzdz( double z, double phi ) {
	this->setPropagationAzimuth(phi);
	if ((!good_) || (ceff_ == 0)) {
		std::runtime_error e( "Profile is not ready!" );
		throw e;
	} else if (z < minZ_ ) {
		return 0;
	} else if ( z > maxZ_) {
		std::ostringstream es("Requested altitude ");
		es << z;
		es << " is out of bounds in ddceffdzdz()!";
		std::range_error e2( es.str() );
		throw e2;
	} else {
		return gsl_spline_eval_deriv2( ceff_spline, z, ceff_acc );
	}
}

double NCPA::SampledProfile::dc0dz( double z ) {
	if ((!good_) || (c0_ == 0)) {
		std::runtime_error e( "Profile is not ready!" );
		throw e;
	} else if (z < minZ_ ) {
		return 0;
	} else if ( z > maxZ_) {
		std::ostringstream es("Requested altitude ");
		es << z;
		es << " is out of bounds in dc0dz()!";
		std::range_error e2( es.str() );
		throw e2;
	} else {
		return gsl_spline_eval_deriv( c0_spline, z, c0_acc );
	}
}

double NCPA::SampledProfile::ddc0dzdz( double z ) {
	if ((!good_) ) {
		return 0;
	} else if ( (c0_ == 0)) {
		std::runtime_error e( "Profile is not ready!" );
		throw e;
	} else if (z < minZ_ ) {
		return 0;
	} else if (  z > maxZ_) {
		std::ostringstream es("Requested altitude ");
		es << z;
		es << " is out of bounds in ddc0dzdz()!";
		std::range_error e2( es.str() );
		throw e2;
	} else {
		return gsl_spline_eval_deriv2( c0_spline, z, c0_acc );
	}
}



void NCPA::SampledProfile::get_z( double *target, int nz_requested ) const {
	int nz = nz_requested < this->nz() ? nz_requested : this->nz();
	
	for (int i = 0; i < nz; i++) {
		target[ i ] = z_[ i ];
	}
}

void NCPA::SampledProfile::get_t( double *target, int nz_requested ) const {
	int nz = nz_requested < this->nz() ? nz_requested : this->nz();
	
	for (int i = 0; i < nz; i++) {
		target[ i ] = t_[ i ];
	}
}

void NCPA::SampledProfile::get_u( double *target, int nz_requested ) const {
	int nz = nz_requested < this->nz() ? nz_requested : this->nz();
	
	for (int i = 0; i < nz; i++) {
		target[ i ] = u_[ i ];
	}
}

void NCPA::SampledProfile::get_v( double *target, int nz_requested ) const {
	int nz = nz_requested < this->nz() ? nz_requested : this->nz();
	
	for (int i = 0; i < nz; i++) {
		target[ i ] = v_[ i ];
	}
}

void NCPA::SampledProfile::get_w( double *target, int nz_requested ) const {
	int nz = nz_requested < this->nz() ? nz_requested : this->nz();
	
	for (int i = 0; i < nz; i++) {
		target[ i ] = w_[ i ];
	}
}

void NCPA::SampledProfile::get_p( double *target, int nz_requested ) const {
	int nz = nz_requested < this->nz() ? nz_requested : this->nz();
	
	for (int i = 0; i < nz; i++) {
		target[ i ] = p_[ i ];
	}
}

void NCPA::SampledProfile::get_rho( double *target, int nz_requested ) const {
	int nz = nz_requested < this->nz() ? nz_requested : this->nz();
	
	for (int i = 0; i < nz; i++) {
		target[ i ] = rho_[ i ];
	}
}

void NCPA::SampledProfile::get_c0( double *target, int nz_requested ) const {
	int nz = nz_requested < this->nz() ? nz_requested : this->nz();
	
	for (int i = 0; i < nz; i++) {
		target[ i ] = c0_[ i ];
	}
}

void NCPA::SampledProfile::get_ceff( double *target, int nz_requested ) const {
	int nz = nz_requested < this->nz() ? nz_requested : this->nz();
	
	for (int i = 0; i < nz; i++) {
		target[ i ] = ceff_[ i ];
	}
}


/*
void NCPA::SampledProfile::set_t( double *source, int nz_requested ) const {
	int nz = nz_requested < this->nz() ? nz_requested : this->nz();
	
	for (int i = 0; i < nz; i++) {
		t_[ i ] = source[ i ];
	}
}

void NCPA::SampledProfile::set_u( double *source, int nz_requested ) const {
	int nz = nz_requested < this->nz() ? nz_requested : this->nz();
	
	for (int i = 0; i < nz; i++) {
		u_[ i ] = source[ i ];
	}
}

void NCPA::SampledProfile::set_v( double *source, int nz_requested ) const {
	int nz = nz_requested < this->nz() ? nz_requested : this->nz();
	
	for (int i = 0; i < nz; i++) {
		v_[ i ] = source[ i ];
	}
}

void NCPA::SampledProfile::set_w( double *source, int nz_requested ) const {
	int nz = nz_requested < this->nz() ? nz_requested : this->nz();
	
	for (int i = 0; i < nz; i++) {
		w_[ i ] = source[ i ];
	}
}

void NCPA::SampledProfile::set_p( double *source, int nz_requested ) const {
	int nz = nz_requested < this->nz() ? nz_requested : this->nz();
	
	for (int i = 0; i < nz; i++) {
		p_[ i ] = source[ i ];
	}
}

void NCPA::SampledProfile::set_rho( double *source, int nz_requested ) const {
	int nz = nz_requested < this->nz() ? nz_requested : this->nz();
	
	for (int i = 0; i < nz; i++) {
		rho_[ i ] = source[ i ];
	}
}


void NCPA::SampledProfile::resample( double newDZ ) {
	gsl_interp_accel *t_acc = gsl_interp_accel_alloc();
	gsl_spline *t_spline = gsl_spline_alloc( gsl_interp_cspline, nz_ );
	gsl_interp_accel *u_acc = gsl_interp_accel_alloc();
	gsl_spline *u_spline = gsl_spline_alloc( gsl_interp_cspline, nz_ );
	gsl_interp_accel *v_acc = gsl_interp_accel_alloc();
	gsl_spline *v_spline = gsl_spline_alloc( gsl_interp_cspline, nz_ );
	gsl_interp_accel *w_acc = gsl_interp_accel_alloc();
	gsl_spline *w_spline = gsl_spline_alloc( gsl_interp_cspline, nz_ );
	gsl_interp_accel *p_acc = gsl_interp_accel_alloc();
	gsl_spline *p_spline = gsl_spline_alloc( gsl_interp_cspline, nz_ );
	gsl_interp_accel *rho_acc = gsl_interp_accel_alloc();
	gsl_spline *rho_spline = gsl_spline_alloc( gsl_interp_cspline, nz_ );
	
	double minZ = z_[0];
	double maxZ = z_[nz_-1];
	int new_nz = ((int)std::floor((maxZ - minZ)/newDZ)) + 1;
	
	// Allocate new arrays
	double *new_z = new double[ new_nz ];
	double *new_t = new double[ new_nz ];
	double *new_u = new double[ new_nz ];
	double *new_v = new double[ new_nz ];
	double *new_w = new double[ new_nz ];
	double *new_p = new double[ new_nz ];
	double *new_rho = new double[ new_nz ];
	
	gsl_spline_init( t_spline, z_, t_, nz_ );
	gsl_spline_init( u_spline, z_, u_, nz_ );
	gsl_spline_init( v_spline, z_, v_, nz_ );
	gsl_spline_init( w_spline, z_, w_, nz_ );
	gsl_spline_init( p_spline, z_, p_, nz_ );
	gsl_spline_init( rho_spline, z_, rho_, nz_ );
	
	// get the new z vector
	for (int i = 0; i < new_nz; i++) {
		new_z[ i ] = minZ + i*newDZ;
	}
	
	// Now compute the new values
	for (int i = 0; i < new_nz; i++) {
		new_t[ i ] = gsl_spline_eval( t_spline, new_z[i], t_acc );
		new_u[ i ] = gsl_spline_eval( u_spline, new_z[i], u_acc );
		new_v[ i ] = gsl_spline_eval( v_spline, new_z[i], v_acc );
		new_w[ i ] = gsl_spline_eval( w_spline, new_z[i], w_acc );
		new_p[ i ] = gsl_spline_eval( p_spline, new_z[i], p_acc );
		new_rho[ i ] = gsl_spline_eval( rho_spline, new_z[i], rho_acc );
	}
	
	// free memory
	gsl_spline_free( u_spline );
	gsl_spline_free( t_spline );
	gsl_spline_free( v_spline );
	gsl_spline_free( w_spline );
	gsl_spline_free( p_spline );
	gsl_spline_free( rho_spline );
	gsl_interp_accel_free( t_acc );
	gsl_interp_accel_free( u_acc );
	gsl_interp_accel_free( v_acc );
	gsl_interp_accel_free( w_acc );
	gsl_interp_accel_free( p_acc );
	gsl_interp_accel_free( rho_acc );
	
	
	delete [] z_;
	z_ = new_z;
	delete [] u_;
	u_ = new_u;
	delete [] t_;
	t_ = new_t;
	delete [] v_;
	v_ = new_v;
	delete [] w_;
	w_ = new_w;
	delete [] p_;
	p_ = new_p;
	delete [] rho_;
	rho_ = new_rho;
	
	nz_ = new_nz;
	dz_min_ = newDZ;
}
*/
