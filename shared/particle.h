#ifndef _PARTICLE_
#define _PARTICLE_

#include "c2dvector.h"
#include "parameters.h"
#include <vector>

class BasicParticle0{
public:
	C2DVector r;
	Real theta;
};

class BasicParticle{
public:
	C2DVector r,v;
};

class BasicDynamicParticle: public BasicParticle {  // This is an abstract class for all of dynamic particles
public:
	int neighbor_size;
	Real theta;
	static Real noise_amplitude;
	static Real speed;
	vector<int> neighbor_id; // id of neighboring particles

	void Init();
	void Init(C2DVector);
	void Init(C2DVector, C2DVector);
	virtual void Reset();
	void Move();
	void Interact();
};

void BasicDynamicParticle::Init()
{
	r.Rand();
	v.Rand(1.0);
	theta = atan(v.y/v.x);
	if (v.x < 0)
		theta += PI;
	theta -= 2*PI * (int (theta / (2*PI)));v.x = cos(theta);
	v.y = sin(theta);
	Reset();
}

void BasicDynamicParticle::Init(C2DVector position)
{
	r = position;
	v.Rand(1.0);
	theta = atan(v.y/v.x);
	if (v.x < 0)
		theta += PI;
	theta -= 2*PI * (int (theta / (2*PI)));
	v.x = cos(theta);
	v.y = sin(theta);
	Reset();
}

void BasicDynamicParticle::Init(C2DVector position, C2DVector velocity)
{
	r = position;
	v = velocity;
	theta = atan(v.y/v.x);
	if (v.x < 0)
		theta += PI;
	theta -= 2*PI * (int (theta / (2*PI)));
	v.x = cos(theta);
	v.y = sin(theta);
	Reset();
}

void BasicDynamicParticle::Reset() {}

class VicsekParticle: public BasicDynamicParticle {
public:
	Real average_theta;
	void Move()
	{
		if (neighbor_size > 0)
			average_theta /= neighbor_size;
		else
			average_theta = theta;
		theta = average_theta + noise_amplitude*gsl_ran_flat(C2DVector::gsl_r,-M_PI,M_PI);
		C2DVector old_v = v;
		v.x = cos(theta);
		v.y = sin(theta);
		r += v*(dt*speed);
		#ifdef PERIODIC_BOUNDARY_CONDITION
			#ifndef NonPeriodicCompute
				r.Periodic_Transform();
			#endif
		#endif
		Reset();
	}

	void Reset()
	{
		neighbor_size = 0;
		average_theta = 0;
	}

	void Interact(VicsekParticle& p)
	{
		C2DVector dr = r - p.r;
		#ifdef PERIODIC_BOUNDARY_CONDITION
			dr.Periodic_Transform();
		#endif
		Real d2 = dr.Square();

		if (d2 < 1)
		{
			neighbor_size++;
			p.neighbor_size++;
			Real dtheta = p.theta - theta;
			dtheta -= 2*PI * (int (dtheta / PI));
			average_theta += p.theta;
			p.average_theta += theta;
		}
	}
};

class VicsekParticle2: public BasicDynamicParticle {
public:
	C2DVector average_v;
	static Real beta;
	static Real rc;
	void Move()
	{
		if (neighbor_size == 0)
			average_v = v;
		Real rand_angle = gsl_ran_flat(C2DVector::gsl_r,-M_PI,M_PI);
		average_v.x += neighbor_size*noise_amplitude*cos(rand_angle);
		average_v.y += neighbor_size*noise_amplitude*sin(rand_angle);
		theta = atan2(average_v.y,average_v.x);
		v.x = cos(theta);
		v.y = sin(theta);
		r += v*(dt*speed);
		#ifdef PERIODIC_BOUNDARY_CONDITION
			#ifndef NonPeriodicCompute
				r.Periodic_Transform();
			#endif
		#endif
		Reset();
	}

	void Reset()
	{
		neighbor_size = 0;
		average_v.Null();
	}

	void Interact(VicsekParticle2& p)
	{
		C2DVector dr = r - p.r;
		#ifdef PERIODIC_BOUNDARY_CONDITION
			dr.Periodic_Transform();
		#endif
		Real d2 = dr.Square();
		Real d = sqrt(d2);
		C2DVector ehat = dr / d;

		if (d2 < 1)
		{
			neighbor_size++;
			p.neighbor_size++;
			Real dtheta = p.theta - theta;
			dtheta -= 2*PI * (int (dtheta / PI));
			average_v += p.v;
			p.average_v += v;
			if (d < rc)
			{
				Real force_amplitude = beta/(1 + exp(d/rc-2));
				average_v += ehat*force_amplitude;
				p.average_v -= ehat*force_amplitude;
			}
		}
	}
};

class ContinuousParticle: public BasicDynamicParticle {
public:
	Real torque;
	static Real g, gw;
	static Real alpha;
	static Real Dr,K,Kamp;

	ContinuousParticle();
	void Move()
	{
		#ifdef COMPARE
			torque = round(digits*torque)/digits;
		#endif
		torque = g*torque + gsl_ran_gaussian(C2DVector::gsl_r,noise_amplitude);
		theta += torque*dt;
//		theta -= 2*PI * ((int) (theta / (PI)));
		C2DVector old_v = v;
		v.x = cos(theta);
		v.y = sin(theta);

		r += v*(speed*dt);
		r.x += gsl_ran_gaussian(C2DVector::gsl_r,Kamp);
		r.y += gsl_ran_gaussian(C2DVector::gsl_r,Kamp);
		#ifdef PERIODIC_BOUNDARY_CONDITION
			#ifndef NonPeriodicCompute
				r.Periodic_Transform();
			#endif
		#endif
		#ifdef TRACK_PARTICLE
			if (this == track_p && flag)
			{
				cout << "Particle:         " << setprecision(50)  << r << "\t" << theta << "\t" << torque << endl << flush;
			}
		#endif
		Reset();
	}

	virtual void Reset();
	void Interact(ContinuousParticle& p)
	{
		C2DVector dr = r - p.r;
		#ifdef PERIODIC_BOUNDARY_CONDITION
			dr.Periodic_Transform();
		#endif
		Real d2 = dr.Square();

		Real torque_interaction;
		if (d2 < 1)
		{
			neighbor_size++;
			p.neighbor_size++;

			Real d = sqrt(d2);

			torque_interaction = (1-alpha)*sin(p.theta - theta)/(PI);

			torque += torque_interaction;
			p.torque -= torque_interaction;

			torque -= alpha*(dr.x*v.y - dr.y*v.x) /(PI*d);
			p.torque += alpha*(dr.x*p.v.y - dr.y*p.v.x) /(PI*d);

			#ifdef TRACK_PARTICLE
				if (this == track_p && flag)
				{
//					if (abs(torque) > 0.1)
//						cout << "Intthis:     " << setprecision(100) << d2 << "\t" << torque_interaction << endl << flush;
//						cout << "Intthis:     " << setprecision(100) << r << "\t" << d2 << endl << flush;
				}
			#endif

			#ifdef TRACK_PARTICLE
				if (&p == track_p && flag)
				{
//						cout << "Intthat:     " << setprecision(100) <<  d2 << "\t" << torque_interaction << endl << flush;
//					if (abs(p.torque) > 0.1)
//						cout << "Intthat:     " << setprecision(100) << p.r << "\t" << d2 << "\t" << p.theta << "\t" << torque_interaction << endl << flush;
				}
			#endif
		}
	}
};

ContinuousParticle::ContinuousParticle()
{
	Init();
}

void ContinuousParticle::Reset()
{
	neighbor_size = 1;
	torque = 0;
}

Real ContinuousParticle::g = 4;
Real ContinuousParticle::alpha = 0.5;
Real ContinuousParticle::Dr = 0;
Real ContinuousParticle::K = 0;
Real ContinuousParticle::Kamp = 0;


class MarkusParticle: public BasicDynamicParticle {
public:
	Real torque;
	static Real mu_plus;
	static Real mu_minus;
	static Real kapa;
	static Real D_phi;
	static Real kisi_r, kisi_a, kisi;

	MarkusParticle();
	void Move()
	{
		#ifdef COMPARE
			torque = round(digits*torque)/digits;
		#endif
		torque = torque + gsl_ran_gaussian(C2DVector::gsl_r,noise_amplitude);
		theta += torque*dt;
//		theta -= 2*PI * ((int) (theta / (PI)));
		C2DVector old_v = v;
		v.x = cos(theta);
		v.y = sin(theta);

		r += v*(dt*speed);
		#ifdef PERIODIC_BOUNDARY_CONDITION
			#ifndef NonPeriodicCompute
				r.Periodic_Transform();
			#endif
		#endif
		#ifdef TRACK_PARTICLE
			if (this == track_p && flag)
			{
				cout << "Particle:         " << setprecision(50)  << r << "\t" << theta << "\t" << torque << endl << flush;
			}
		#endif
		Reset();
	}

	virtual void Reset();
	void Interact(MarkusParticle& p)
	{
		C2DVector dr = r - p.r;
		#ifdef PERIODIC_BOUNDARY_CONDITION
			dr.Periodic_Transform();
		#endif
		Real d2 = dr.Square();
		Real d = sqrt(d2);

		Real torque_interaction;
		if (d < kisi)
		{
			if (d < kisi_a)
			{
				neighbor_size++;
				p.neighbor_size++;
				torque_interaction = mu_plus*(1-(d2/(kisi_a*kisi_a)))*sin(p.theta - theta);
				torque += torque_interaction;
				p.torque -= torque_interaction;
			}
			else
			{
				neighbor_size++;
				p.neighbor_size++;
				torque_interaction = mu_minus*4*(d - kisi_a)*(1-d)*sin(theta - p.theta) / ((1-kisi_a)*(1-kisi_a));
				torque += torque_interaction;
				p.torque -= torque_interaction;
			}

			if (d < kisi_r)
			{
				Real alpha = atan2(-dr.y,-dr.x);
				Real factor = (1.0 - d / kisi_r);
				torque += factor*kapa*sin(theta - alpha);
				p.torque -= factor*kapa*sin(p.theta - alpha);
			}

			#ifdef TRACK_PARTICLE
				if (this == track_p && flag)
				{
//					if (abs(torque) > 0.1)
//						cout << "Intthis:     " << setprecision(100) << d2 << "\t" << torque_interaction << endl << flush;
//						cout << "Intthis:     " << setprecision(100) << r << "\t" << d2 << endl << flush;
				}
			#endif

			#ifdef TRACK_PARTICLE
				if (&p == track_p && flag)
				{
//						cout << "Intthat:     " << setprecision(100) <<  d2 << "\t" << torque_interaction << endl << flush;
//					if (abs(p.torque) > 0.1)
//						cout << "Intthat:     " << setprecision(100) << p.r << "\t" << d2 << "\t" << p.theta << "\t" << torque_interaction << endl << flush;
				}
			#endif
		}
	}
};

MarkusParticle::MarkusParticle()
{
	Init();
}

void MarkusParticle::Reset()
{
	neighbor_size = 1;
	torque = 0;
}

Real MarkusParticle::mu_plus = 1;
Real MarkusParticle::mu_minus = 1;
Real MarkusParticle::kapa = 40;//40.0;
Real MarkusParticle::D_phi = 1;
Real MarkusParticle::kisi_r = 0.1;
Real MarkusParticle::kisi_a = 1;//0.2;
Real MarkusParticle::kisi = 1;

class RepulsiveParticle: public BasicDynamicParticle {
public:
	Real torque;
	C2DVector f;
	static Real g;
	static Real kesi;
	static Real Dr;
	static Real A_p;		// interaction strength
	static Real sigma_p;		// sigma in Yukawa Potential
	static Real r_f_p;		// flocking radius with particles
	static Real r_c_p;		// repulsive cutoff radius with particles

	static Real A_w;
	static Real sigma_w;
	static Real r_f_w;		// aligning radius with walls
	static Real r_c_w;		// repulsive cutoff radius with walls

	RepulsiveParticle();
	void Move()
	{
		#ifdef COMPARE
			torque = round(digits*torque)/digits;
		#endif
		torque = torque + gsl_ran_gaussian(C2DVector::gsl_r,noise_amplitude);
		theta += torque*dt;
		C2DVector old_v = v;
		#ifdef COMPARE
			theta = round(digits*theta)/digits;
		#endif
		v.x = cos(theta);
		v.y = sin(theta);
		#ifdef COMPARE
			v.x = round(digits*v.x)/digits;
			v.y = round(digits*v.y)/digits;
			f.x = round(digits*f.x)/digits;
			f.y = round(digits*f.y)/digits;
		#endif
		v *= speed;
		v += f;
		r += v*dt;
		#ifdef PERIODIC_BOUNDARY_CONDITION
			#ifndef NonPeriodicCompute
				r.Periodic_Transform();
			#endif
		#endif
		#ifdef TRACK_PARTICLE
			if (this == track_p && flag)
			{
				cout << "Particle:         " << setprecision(50)  << v.y << endl << flush;
			}
		#endif
		Reset();
	}

	virtual void Reset();

	void Interact(RepulsiveParticle& p)
	{
		C2DVector dr = r - p.r;
		#ifdef PERIODIC_BOUNDARY_CONDITION
			dr.Periodic_Transform();
		#endif
		Real d2 = dr.Square();
		Real d = sqrt(d2);

		C2DVector interaction_force;
		if (d < r_c_p)
		{
			dr /= d;
			Real r_c_p2 = r_c_p*r_c_p; 
//			interaction_force = dr * A_p * ( exp(- d / sigma_p ) * ( 1. / d2 + 1. / (sigma_p * d)) - exp(- r_c_p / sigma_p ) * ( 1. / r_c_p2 + 1. / (sigma_p * r_c_p)) );
			interaction_force = dr * A_p * ((r_c_p - d)*(r_c_p - d) / ((d-sigma_p)*(d-sigma_p)));

			f += interaction_force;
			p.f -= interaction_force;
		}

		Real torque_interaction;
		if (d < r_f_p)
		{
			neighbor_size++;
			p.neighbor_size++;

			torque_interaction = g*sin(p.theta - theta)/(PI);

			torque += torque_interaction;
			p.torque -= torque_interaction;

			#ifdef TRACK_PARTICLE
				if (this == track_p && flag)
				{
//					if (abs(torque) > 0.1)
//						cout << "Intthis:     " << setprecision(100) << d2 << "\t" << torque_interaction << endl << flush;
//						cout << "Intthis:     " << setprecision(100) << r << "\t" << d2 << endl << flush;
				}
			#endif

			#ifdef TRACK_PARTICLE
				if (&p == track_p && flag)
				{
//						cout << "Intthat:     " << setprecision(100) <<  d2 << "\t" << torque_interaction << endl << flush;
//					if (abs(p.torque) > 0.1)
//						cout << "Intthat:     " << setprecision(100) << p.r << "\t" << d2 << "\t" << p.theta << "\t" << torque_interaction << endl << flush;
				}
			#endif
		}
	}
};

RepulsiveParticle::RepulsiveParticle()
{
	Init();
}

void RepulsiveParticle::Reset()
{
	neighbor_size = 1;
	torque = 0;
	f.Null();
}


class EjtehadiParticle: public ContinuousParticle {
public:
	Real torque, torque_phi;
	Real phi;
	Real speed;
	static Real g, gw, g_phi;
	static Real alpha;
	static Real Dr,K,Kamp,D_phi;
	static Real vmin,vmax,vmid,vamp;
	static Real Rphi;
	static Real omega;
	static Real noise_amplitude_phi;

	EjtehadiParticle();
	void Move()
	{
		#ifdef COMPARE
			torque = round(digits*torque)/digits;
		#endif
		torque = g*torque + gsl_ran_gaussian(C2DVector::gsl_r,noise_amplitude);
		torque_phi = torque_phi + gsl_ran_gaussian(C2DVector::gsl_r,noise_amplitude_phi);
		theta += torque*dt;
		phi += torque_phi*dt;

		C2DVector old_v = v;
		v.x = cos(theta);
		v.y = sin(theta);

		speed = vmid + vamp*cos(phi);
		r += v*(speed*dt);
		r.x += gsl_ran_gaussian(C2DVector::gsl_r,Kamp);
		r.y += gsl_ran_gaussian(C2DVector::gsl_r,Kamp);
		#ifdef PERIODIC_BOUNDARY_CONDITION
			#ifndef NonPeriodicCompute
				r.Periodic_Transform();
			#endif
		#endif
		#ifdef TRACK_PARTICLE
			if (this == track_p && flag)
			{
				cout << "Particle:         " << setprecision(50)  << r << "\t" << theta << "\t" << torque << endl << flush;
			}
		#endif
		Reset();
	}

	virtual void Reset();
	void Interact(EjtehadiParticle& p)
	{
		C2DVector dr = r - p.r;
		#ifdef PERIODIC_BOUNDARY_CONDITION
			dr.Periodic_Transform();
		#endif
		Real d2 = dr.Square();
		Real d = sqrt(d2);

		Real torque_interaction;
		if (d2 < 1)
		{
			neighbor_size++;
			p.neighbor_size++;

			torque_interaction = (1-alpha)*sin(p.theta - theta)/(PI);

			torque += torque_interaction;
			p.torque -= torque_interaction;

			torque -= alpha*(dr.x*v.y - dr.y*v.x) /(PI*d);
			p.torque += alpha*(dr.x*p.v.y - dr.y*p.v.x) /(PI*d);

			#ifdef TRACK_PARTICLE
				if (this == track_p && flag)
				{
//					if (abs(torque) > 0.1)
//						cout << "Intthis:     " << setprecision(100) << d2 << "\t" << torque_interaction << endl << flush;
//						cout << "Intthis:     " << setprecision(100) << r << "\t" << d2 << endl << flush;
				}
			#endif

			#ifdef TRACK_PARTICLE
				if (&p == track_p && flag)
				{
//						cout << "Intthat:     " << setprecision(100) <<  d2 << "\t" << torque_interaction << endl << flush;
//					if (abs(p.torque) > 0.1)
//						cout << "Intthat:     " << setprecision(100) << p.r << "\t" << d2 << "\t" << p.theta << "\t" << torque_interaction << endl << flush;
				}
			#endif
		}
		if (d < Rphi)
		{
			torque_interaction = g_phi*sin(p.phi - phi)/(PI);
			torque_phi += torque_interaction;
			p.torque_phi -= torque_interaction;
		}
	}
	static void Set_Variables();
};

EjtehadiParticle::EjtehadiParticle()
{
	Init();
}

void EjtehadiParticle::Reset()
{
	neighbor_size = 1;
	torque = 0;
	torque_phi = omega;
}

void EjtehadiParticle::Set_Variables()
{
	Kamp = sqrt(2*K*dt);
	noise_amplitude = sqrt(2*Dr/dt);
	noise_amplitude_phi = sqrt(2*D_phi/dt);
	vamp = (vmax - vmin) / 2.0;
	vmid = (vmax + vmin) / 2.0;
}

Real EjtehadiParticle::g = 1;
Real EjtehadiParticle::gw = 20;
Real EjtehadiParticle::g_phi = 1;
Real EjtehadiParticle::alpha = 0.5;
Real EjtehadiParticle::Rphi = 1;
Real EjtehadiParticle::D_phi = 0;
Real EjtehadiParticle::Dr = 0;
Real EjtehadiParticle::K = 0.2;
Real EjtehadiParticle::Kamp = 0.2;
Real EjtehadiParticle::noise_amplitude_phi = 0;
Real EjtehadiParticle::omega = 1;
Real EjtehadiParticle::vmin = 0.1;
Real EjtehadiParticle::vamp = 1.0;
Real EjtehadiParticle::vmax = 1.1;
Real EjtehadiParticle::vmid = 0.5;

Real VicsekParticle2::beta = 2.5;
Real VicsekParticle2::rc = 0.127;

Real RepulsiveParticle::g = .5;
Real RepulsiveParticle::kesi = .5;
Real RepulsiveParticle::Dr = 0;

// Interactions for repulsive particles
Real RepulsiveParticle::A_p = 10.;		// interaction strength
Real RepulsiveParticle::sigma_p = 0.9;		// sigma in Yukawa Potential
Real RepulsiveParticle::r_f_p = 1.;		// flocking radius with particles
Real RepulsiveParticle::r_c_p = 1.1;		// repulsive cutoff radius with particles

Real RepulsiveParticle::A_w = 50.;		// interaction strength with walls
Real RepulsiveParticle::sigma_w = 1.;
Real RepulsiveParticle::r_f_w = 1.;		// aligning radius with walls
Real RepulsiveParticle::r_c_w = 1.; 		// repulsive cutoff radius with walls

Real ContinuousParticle::gw = 20;

Real BasicDynamicParticle::noise_amplitude = .1;
Real BasicDynamicParticle::speed = 1;

#endif
