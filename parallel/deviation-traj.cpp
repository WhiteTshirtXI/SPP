#include "../shared/parameters.h"
#include "../shared/c2dvector.h"
#include "../shared/particle.h"
#include "../shared/cell.h"
#include "../shared/vector-set.h"
#include "separate-lyapunovbox.h"
#include "mpi.h"

inline void timing_information(const int node_id, clock_t start_time, int i_step, int total_step)
{
	if (node_id == 0)
	{
		clock_t current_time = clock();
		int lapsed_time = (current_time - start_time) / (CLOCKS_PER_SEC);
		int remaining_time = (lapsed_time*(total_step - i_step)) / (i_step + 1);
		if (i_step % 1000 == 0)
			cout << "\r" << round(100.0*i_step / total_step) << "% lapsed time: " << lapsed_time << " s		remaining time: " << remaining_time << " s" << flush;
	}
}

inline Real equilibrium(LyapunovBox& box, long int equilibrium_step, int saving_period)
{
	clock_t start_time, end_time;
	start_time = clock();

	if (box.thisnode == 0)
	{
		cout << "equilibrium:" << endl;

		for (long int i = 0; i < equilibrium_step; i+=cell_update_period)
		{
			box.Multi_Step(cell_update_period);
			timing_information(box.thisnode,start_time,i,equilibrium_step);
		}

		cout << "Finished" << endl;
	}

	end_time = clock();

	Real t = (Real) (end_time - start_time) / CLOCKS_PER_SEC;
	return(t);
}

bool Init_Box(LyapunovBox& box, int argc, char *argv[])
{
	if (argc < 6)
	{
		if (box.thisnode == 0)
			cout << "arguments are: \n" << "density,\tkappa,\tmu+,\tmu-,\tDphi" << endl;
		exit(0);
	}
	box.density = atof(argv[1]);
	Real input_kapa = atof(argv[2]);
	Real input_mu_plus = atof(argv[3]);
	Real input_mu_minus = atof(argv[4]);
	Real input_D = atof(argv[5]);

	Real t_eq,t_sim;

	box.N = (int) round(Lx2*Ly2*box.density);

	if (box.thisnode == 0)
	{
// Positioning the particles
		Polar_Formation(box.particle,box.N);
//		Triangle_Lattice_Formation(box.particle, box.N, 1);
//		Random_Formation(box.particle, box.N, 0); // Positioning partilces Randomly, but distant from walls (the last argument is the distance from walls)
//		Random_Formation_Circle(box.particle, box.N, Lx-1); // Positioning partilces Randomly, but distant from walls
//		Single_Vortex_Formation(box.particle, box.N);
		//	Four_Vortex_Formation(box.particle, box.N);
	}
	MPI_Barrier(MPI_COMM_WORLD);

	MarkusParticle::kapa = input_kapa;
	MarkusParticle::mu_plus = input_mu_plus;
	MarkusParticle::mu_minus = input_mu_minus;

	Particle::D_phi = input_D;
	Particle::noise_amplitude = sqrt(2*input_D) / sqrt(dt); // noise amplitude depends on the step (dt) because of ito calculation. If we have epsilon in our differential equation and we descritise it with time steps dt, the noise in each step that we add is epsilon times sqrt(dt) if we factorise it with a dt we have dt*(epsilon/sqrt(dt)).
	box.info.str("");
	box.info << "rho=" << box.density <<  "-k=" << Particle::kapa << "-mu+=" << Particle::mu_plus << "-mu-=" << Particle::mu_minus << "-Dphi=" << Particle::D_phi << "-L=" << Lx;

	if (box.thisnode == 0)
	{
		cout << "Number of particles is: " << box.N << endl;
		stringstream address;
		address.str("");
		address << box.info.str() << "-r-v.bin";
		box.trajfile.open(address.str().c_str());
	}

	if (box.thisnode == 0)
		cout << " Box information is: " << box.info.str() << endl;

	MPI_Barrier(MPI_COMM_WORLD);
	t_eq = equilibrium(box, equilibrium_step, saving_period);
	MPI_Barrier(MPI_COMM_WORLD);

	if (box.thisnode == 0)
		cout << " Done in " << floor(t_eq / 60.0) << " minutes and " << t_eq - 60*floor(t_eq / 60.0) << " s" << endl;
}

bool Init_Box_From_File(LyapunovBox& box, const string input_name)
{
	Real input_kapa;
	Real input_mu_plus;
	Real input_mu_minus;
	Real input_Dphi;
	Real input_L;

	string name = input_name;
	stringstream address(name);
	ifstream is;
	is.open(address.str().c_str());
	if (!is.is_open())
		return false;

	boost::replace_all(name, "-r-v.bin", "");
	boost::replace_all(name, "rho=", "");
	boost::replace_all(name, "-k=", "");
	boost::replace_all(name, "-mu+=", "\t");
	boost::replace_all(name, "-mu-=", "\t");
	boost::replace_all(name, "-Dphi=", "\t");
	boost::replace_all(name, "-L=", "\t");

	stringstream ss_name(name);
	ss_name >> box.density;
	ss_name >> input_kapa;
	ss_name >> input_mu_plus;
	ss_name >> input_mu_minus;
	ss_name >> input_Dphi;
	ss_name >> input_L;
	if ((input_L != Lx_int))
	{
		cout << "The specified box size " << input_L << " is not the same as the size in binary file which is " << Lx_int << " please recompile the code with the right Lx_int in parameters.h file." << endl;
		return false;
	}

	Particle::kapa = input_kapa;
	Particle::mu_plus = input_mu_plus;
	Particle::mu_minus = input_mu_minus;
	Particle::D_phi = input_Dphi;
	Particle::noise_amplitude = sqrt(2*Particle::D_phi) / sqrt(dt);

	is.read((char*) &box.N, sizeof(int) / sizeof(char));
	if (box.N < 0 || box.N > 1000000)
		return (false);

	if (box.thisnode == 0)
	{
		for (int i = 0; i < box.N; i++)
		{
			is >> box.particle[i].r;
			is >> box.particle[i].v;
		}
		box.Update_Cells();
	}

	is.close();

	MPI_Barrier(MPI_COMM_WORLD);

	if (box.thisnode == 0)
	{
		cout << "number_of_particles = " << box.N << endl; // Printing number of particles.
	}
	MPI_Barrier(MPI_COMM_WORLD);

	box.info.str("");
	box.info << "rho=" << box.density <<  "-k=" << Particle::kapa << "-mu+=" << Particle::mu_plus << "-mu-=" << Particle::mu_minus << "-Dphi=" << Particle::D_phi << "-L=" << Lx;

	return (true);
}

Real Find_Traj(LyapunovBox& box, const Real& Dphi, const int& sample_num, const int&)
{
	Particle::D_phi = Dphi;
	Particle::noise_amplitude = sqrt(2*Particle::D_phi) / sqrt(dt);

	MPI_Barrier(MPI_COMM_WORLD);
	equilibrium(box, equilibrium_step, saving_period);



	MPI_Barrier(MPI_COMM_WORLD);
}

void Send_Trajectory(vector<Particle> send_traj, int N, int dest, int tag)
{
// Allocation
	double* data_buffer = new double[3*N];

// Assigning the buffer arrays
	int counter = 0; // count particles. We can not use a shift very simply becasue we need to write both index_buffer and data_buffer.
	for (int i = 0; i < N; i++)
	{
		data_buffer[3*i] = send_traj[i].r.x;
		data_buffer[3*i+1] = send_traj[i].r.y;
		data_buffer[3*i+2] = send_traj[i].theta;
		counter++;
	}

	MPI_Send(data_buffer, 3*send_traj.size(), MPI_DOUBLE, dest, 0,MPI_COMM_WORLD);
	delete [] data_buffer;
}

void Recv_Traj(vector<Particle>& recv_traj, int N, int source, int tag)
{
// Allocation
	double* data_buffer = new double[3*N];

	MPI_Status status;
	MPI_Recv(data_buffer, 3*N, MPI_DOUBLE, source, 0,MPI_COMM_WORLD, &status);

// Assigning the buffer arrays
	int counter = 0; // count particles. We can not use a shift very simply becasue we need to write both index_buffer and data_buffer.
	Particle p;
	for (int i = 0; i < N; i++)
	{
		p.r.x = data_buffer[3*i];
		p.r.y = data_buffer[3*i+1];
		p.theta = data_buffer[3*i+2];
		recv_traj.push_back(p);
		counter++;
	}

	delete [] data_buffer;
}

void Save_Traj(std::ostream& os, vector<Particle>* trajectory, int sample_num)
{
		int N = sample_num;

		for (int i = 0; i < trajectory[0].size(); i++)
		{
			os.write((char*) &N, sizeof(N) / sizeof(char));

			for (int j = 0; j < sample_num; j++)
			{
				trajectory[j][i].r.write(os);
				C2DVector v;
				v.x = cos(trajectory[j][i].theta);
				v.y = sin(trajectory[j][i].theta);
				v.write(os);
			}
		}
}

void Run(int argc, char *argv[])
{
	Real t_sim;
	int sample_num = 10;
	int tau = cell_update_period*saving_period;
	int point_num = total_step / tau;
	vector<Particle> trajectory[sample_num];

	LyapunovBox box;
	Init_Box(box, argc, argv);
	box.track_id = 15;

	box.Init_Deviation(sample_num);
	for (int i = 0; i < sample_num; i++)
	{
		box.us.v[i].Null();
		for (int j = 0; j < 20; j++)
		{
			int n = rand() % box.N;
			box.us.v[i].particle[n].r.x = gsl_ran_flat(C2DVector::gsl_r, -1e-2,1e-2);
			box.us.v[i].particle[n].r.y = gsl_ran_flat(C2DVector::gsl_r, -1e-2,1e-2);
		}
	}

	if (box.thisnode == 0)
	{
		box.Save(box.vs.v[0]);
		box.dvs = box.us;
	}

	MPI_Barrier(MPI_COMM_WORLD);

	box.Root_Bcast_State_Hyper_Vector(box.vs.v[0]);
	box.Root_Bcast_Vector_Set(box.dvs);

	for (int i = 0; i < box.us.direction_num; i++)
	{
		if (i % box.totalnode == box.thisnode)
		{
			box.vs.v[i] = box.vs.v[0] + box.dvs.v[i];
			box.Load(box.vs.v[i]);
			cout << i << "\t" << box.us.direction_num << endl;
			for (int n = 0; n < point_num; n++)
			{ 
				box.Multi_Step(saving_period*cell_update_period, cell_update_period);
				box.Track_Particle(trajectory[i]);
			}
		}
	}

	for (int i = 0; i < box.us.direction_num; i++)
	{
		if (box.thisnode != 0 && i % box.totalnode == box.thisnode)
				Send_Trajectory(trajectory[i], point_num, 0, i);

		if (box.thisnode == 0 && i % box.totalnode != 0)
			Recv_Traj(trajectory[i], point_num, i % box.totalnode, i);
	}

	MPI_Barrier(MPI_COMM_WORLD);

	if (box.thisnode == 0)
	{
		ofstream out_traj;
		out_traj.open("traj-r-v.bin");
		Save_Traj(out_traj,trajectory,sample_num);
		out_traj.close();
	}
}


void Init_Nodes(int input_seed = seed)
{
	C2DVector::Init_Rand(input_seed);
	MPI_Barrier(MPI_COMM_WORLD);
}

int main(int argc, char *argv[])
{
	int this_node_id, total_nodes;
	MPI_Status status;
	MPI_Init(&argc, &argv);

	Init_Nodes(time(NULL));

	Run(argc, argv);

	MPI_Barrier(MPI_COMM_WORLD);
	MPI_Finalize();

}

