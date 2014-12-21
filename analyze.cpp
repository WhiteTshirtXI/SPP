#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>


#include<iostream>
#include<cstdlib>
#include<vector>

#include"analyze.h"

using namespace std;

int main(int argc, char** argv)
{
	C2DVector::Init_Rand(321);
	srand(time(NULL));

	for (int i = 1; i < argc; i++)
	{
		string name = argv[i];
		SceneSet* sceneset = new SceneSet(name);
		sceneset->Read();
		sceneset->L -= 0.5;
		Real box_dim = sceneset->L;

		boost::replace_all(name, "-r-v.bin", "");

//		sceneset->Plot_Fields(21, 40, name);
//		sceneset->Plot_Averaged_Fields(81, name);
//		sceneset->Plot_Averaged_Fields_Section(41, 40, name);
//		sceneset->Plot_Averaged_Fields_Section(41, 38, name);
//		sceneset->Plot_Averaged_Fields_Section(41, 20, name);
//		sceneset->Plot_Density_Contour(101, 0.2, name);


		boost::replace_all(name, "rho=", "");
		boost::replace_all(name, "-noise=", "\t");

//		Stat<double>	angular_momentum_data;
//		Compute_Angular_Momentum(sceneset, &angular_momentum_data);
//		cout << name << "\t" << fabs(angular_momentum_data.mean) << "\t" << angular_momentum_data.error << endl;
//		angular_momentum_data.Reset();

//		Time_AutoCorrelation(sceneset);
//		Spatial_AutoCorrelation(sceneset, 50, 5);

//		int r = rand() % Scene::number_of_particles;
//		Trajectory(sceneset,r);
//		Angle_Time(sceneset, 12);
		for (int j = 0; j < Scene::number_of_particles; j++)
			Angular_Velocity_Time(sceneset, j);

//		Compute_Fluctuation(sceneset);

		delete sceneset;
	}

	return 0;
}

