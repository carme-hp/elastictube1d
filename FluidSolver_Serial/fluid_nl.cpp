#include "fluid_nl.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

/* 
   Function for solving the linear system 
   LAPACK is used DGESV computes the solution to a real system of linear equations
   A * x = b,
   where A is an N-by-N matrix and x and b are N-by-NRHS matrices.
*/
extern "C" {
void dgesv_(
    int* n,
    int* nrhs,
    double* A,
    int* lda,
    int* ipiv,
    double* b,
    int* ldb,
    int* info);
}

/* Function for fluid_nl i.e. non-linear */
int fluid_nl(
    double* crossSectionLength,
    double* crossSectionLength_n,
    double* velocity,
    double* velocity_n,
    double* pressure,
    double* pressure_n,
    double* pressure_old,
    double scaled_t,
    int N,
    double kappa,
    double tau,
    double gamma)
{
  /* fluid_nl Variables */
  int i, j = 0, k, ampl;
  double alpha, dx;
  double tmp, tmp2;
  double* Res;
  double** LHS;
  double temp_sum;
  double norm_1, norm_2;
  double norm = 1.0;

  // Used as Ax = b
  // i.e. LHS*x = Res
  Res = (double*)calloc((2 * N + 2), sizeof(double));
  LHS = (double**)calloc((2 * N + 2), sizeof(double*));
  for (i = 0; i < (2 * N + 2); ++i)
    LHS[i] = (double*)calloc((2 * N + 2), sizeof(double));

  /* LAPACK Variables */
  double* A = (double*)calloc((2 * N + 2) * (2 * N + 2), sizeof(double));
  ;
  int* ipiv = (int*)calloc((2 * N + 2), sizeof(int));
  ;
  int nlhs = (2 * N + 2);
  int nrhs = 1;
  int info;

  /* Stabilization Intensity */
  alpha = (N * kappa * tau) / (N * tau + 1);
  dx = 1.0 / (N * kappa * tau);
  ampl = 100;

  k = 0;
  /* Stopping Criteria */
  // TODO: Include the residual
  while (1) {
    for (i = 0; i < (2 * N + 2); i++)
      Res[i] = 0.0;

    for (i = 1; i < N; i++) {
      /* Momentum */
      Res[i] = velocity_n[i] * crossSectionLength[i] * dx;
      Res[i] = Res[i] - 0.25 * crossSectionLength[i + 1] * velocity[i] * velocity[i + 1] - 0.25 * crossSectionLength[i] * velocity[i] * velocity[i + 1];
      Res[i] = Res[i] - crossSectionLength[i] * dx * velocity[i] - 0.25 * crossSectionLength[i + 1] * velocity[i] * velocity[i] - 0.25 * crossSectionLength[i] * velocity[i] * velocity[i] + 0.25 * crossSectionLength[i] * velocity[i - 1] * velocity[i] + 0.25 * crossSectionLength[i - 1] * velocity[i - 1] * velocity[i];
      Res[i] = Res[i] + 0.25 * crossSectionLength[i - 1] * velocity[i - 1] * velocity[i - 1] + 0.25 * crossSectionLength[i] * velocity[i - 1] * velocity[i - 1];
      Res[i] = Res[i] + 0.25 * crossSectionLength[i - 1] * pressure[i - 1] + 0.25 * crossSectionLength[i] * pressure[i - 1] + 0.25 * crossSectionLength[i - 1] * pressure[i] - 0.25 * crossSectionLength[i + 1] * pressure[i] - 0.25 * crossSectionLength[i] * pressure[i + 1] - 0.25 * crossSectionLength[i + 1] * pressure[i + 1];

      /* Continuity */
      Res[i + N + 1] = -(crossSectionLength[i] - crossSectionLength_n[i]) * dx + pressure_old[i] * gamma * dx;
      Res[i + N + 1] = Res[i + N + 1] + 0.25 * crossSectionLength[i - 1] * velocity[i - 1] + 0.25 * crossSectionLength[i] * velocity[i - 1] + 0.25 * crossSectionLength[i - 1] * velocity[i] - 0.25 * crossSectionLength[i + 1] * velocity[i] - 0.25 * crossSectionLength[i] * velocity[i + 1] - 0.25 * crossSectionLength[i + 1] * velocity[i + 1];
      Res[i + N + 1] = Res[i + N + 1] + alpha * pressure[i - 1] - 2 * alpha * pressure[i] - gamma * pressure[i] * dx + alpha * pressure[i + 1];
    }

    i = N - 1;

    /* Boundary */

    /* Velocity Inlet is prescribed */
    tmp = sin(PI * scaled_t);
    Res[0] = (1.0 / kappa) + (1.0 / (kappa * ampl)) * tmp * tmp - velocity[0];

    /* Pressure Inlet is lineary interpolated */
    Res[N + 1] = -pressure[0] + 2 * pressure[1] - pressure[2];

    /* Velocity Outlet is lineary interpolated */
    Res[N] = -velocity[N] + 2 * velocity[N - 1] - velocity[N - 2];

    /* Pressure Outlet is "non-reflecting" */
    tmp2 = sqrt(1 - pressure_n[N] / 2) - (velocity[N] - velocity_n[N]) / 4;
    Res[2 * N + 1] = -pressure[N] + 2 * (1 - tmp2 * tmp2);

    /* Stopping Criteria */
    k += 1; // Iteration Count

    temp_sum = 0;
    for (i = 0; i < (2 * N + 2); i++) {
      temp_sum += Res[i] * Res[i];
    }
    norm_1 = sqrt(temp_sum);

    temp_sum = 0;
    for (i = 0; i < (N + 1); i++) {
      temp_sum += (pressure[i] * pressure[i]) + (velocity[i] * velocity[i]);
    }
    norm_2 = sqrt(temp_sum);

    norm = norm_1 / norm_2; // Norm

    if ((norm < 1e-15 && k > 1) || k > 50) {
      printf("Nonlinear Solver break, Its: %i, norm: %e\n", k, norm);
      break;
    }

    /* Initilizing the the LHS i.e. Left Hand Side */
    for (i = 0; i <= (2 * N + 1); i++)
      for (j = 0; j <= (2 * N + 1); j++)
        LHS[i][j] = 0.0;

    for (i = 1; i < N; i++) {
      // Momentum, Velocity
      LHS[i][i - 1] = LHS[i][i - 1] - 0.25 * crossSectionLength[i - 1] * velocity[i - 1] * 2 - 0.25 * crossSectionLength[i] * velocity[i - 1] * 2 - 0.25 * crossSectionLength[i] * velocity[i] + 0.25 * crossSectionLength[i - 1] * velocity[i];
      LHS[i][i] = LHS[i][i] + 0.25 * crossSectionLength[i + 1] * velocity[i + 1] + 0.25 * crossSectionLength[i] * velocity[i + 1] + crossSectionLength[i] * dx + 0.25 * crossSectionLength[i + 1] * velocity[i] * 2 + 0.25 * crossSectionLength[i] * velocity[i] * 2 - 0.25 * crossSectionLength[i] * velocity[i - 1] - 0.25 * crossSectionLength[i - 1] * velocity[i - 1];
      LHS[i][i + 1] = LHS[i][i + 1] + 0.25 * crossSectionLength[i + 1] * velocity[i] + 0.25 * crossSectionLength[i] * velocity[i];

      // Momentum, Pressure
      LHS[i][N + 1 + i - 1] = LHS[i][N + 1 + i - 1] - 0.25 * crossSectionLength[i - 1] - 0.25 * crossSectionLength[i];
      LHS[i][N + 1 + i] = LHS[i][N + 1 + i] + 0.25 * crossSectionLength[i - 1] - 0.25 * crossSectionLength[i + 1];
      LHS[i][N + 1 + i + 1] = LHS[i][N + 1 + i + 1] + 0.25 * crossSectionLength[i] + 0.25 * crossSectionLength[i + 1];

      // Continuity, Velocity
      LHS[i + N + 1][i - 1] = LHS[i + N + 1][i - 1] - 0.25 * crossSectionLength[i - 1] - 0.25 * crossSectionLength[i];
      LHS[i + N + 1][i] = LHS[i + N + 1][i] - 0.25 * crossSectionLength[i - 1] + 0.25 * crossSectionLength[i + 1];
      LHS[i + N + 1][i + 1] = LHS[i + N + 1][i + 1] + 0.25 * crossSectionLength[i] + 0.25 * crossSectionLength[i + 1];

      // Continuity, Pressure
      LHS[i + N + 1][N + 1 + i - 1] = LHS[i + N + 1][N + 1 + i - 1] - alpha;
      LHS[i + N + 1][N + 1 + i] = LHS[i + N + 1][N + 1 + i] + 2 * alpha + gamma * dx;
      LHS[i + N + 1][N + 1 + i + 1] = LHS[i + N + 1][N + 1 + i + 1] - alpha;
    }

    /* Boundary */

    // Velocity Inlet is prescribed
    LHS[0][0] = 1;
    // Pressure Inlet is lineary interpolated
    LHS[N + 1][N + 1] = 1;
    LHS[N + 1][N + 2] = -2;
    LHS[N + 1][N + 3] = 1;
    // Velocity Outlet is lineary interpolated
    LHS[N][N] = 1;
    LHS[N][N - 1] = -2;
    LHS[N][N - 2] = 1;
    // Pressure Outlet is Non-Reflecting
    LHS[2 * N + 1][2 * N + 1] = 1;
    LHS[2 * N + 1][N] = -(sqrt(1 - pressure_n[N] / 2) - (velocity[N] - velocity_n[N]) / 4);

    // Solve Linear System using LAPACK

    /* LAPACK requires a 1D array 
       i.e. Linearizing 2D 
    */
    int counter = 0;
    for (i = 0; i <= (2 * N + 1); i++) {
      for (j = 0; j <= (2 * N + 1); j++) {
        A[counter] = LHS[j][i];
        //printf("%15.10e\t",LHS[i][j]);
        counter++;
      }
      //printf("\n");
    }
    /*printf("\n");
      for ( i = 0; i <= (2*N + 1); i++ ){
      printf("%15.10e\t",Res[i]);
      }
      printf("\n");*/

    /* LAPACK Function call to solve the linear system */
    dgesv_(&nlhs, &nrhs, A, &nlhs, ipiv, Res, &nlhs, &info);

    if (info != 0) {
      printf("Linear Solver not converged!!!, Info: %i\n", info);
    }

    /*printf("\n");
      for ( i = 0; i <= (2*N + 1); i++ ){
      printf("%15.10e\t",Res[i]);
      }
      printf("\n");*/

    //printf("Linear Solver Info (0 if converged) %i\n", info);

    /* LAPACK solves the result back in Res i.e. b of A*x = b */
    /*for ( i = 0; i <=N; i++ )
      x[i] = Res[i];
				
      for ( i = 0; i <= N; i++ )
      du[i] = x[i];
		
      for ( i = 0; i <= N; i++ ) 
      dp[i] = x[i+N+2];
    		
      for( i = 0; i <=N; ++i )
      {
      u[i] = u[i] + du[i];		
      p[i] = p[i] + dp[i+N+2];
      }*/

    // i do not understand the above version
    for (i = 0; i <= N; i++) {
      velocity[i] = velocity[i] + Res[i];
      pressure[i] = pressure[i] + Res[i + N + 1];
    }

  } // END OF WHILE

  /* Writing (i.e. overwriting at every timestep) the file for pressure and velocity values */
  /*FILE *fpP, *fpU;

    fpP = fopen("p.csv", "w");
    fpU = fopen("u.csv", "w");

    if ( fpP == NULL )
    {
    printf(" Could NOT open file for Pressure \n ");
    return -1;
    }
    if ( fpU == NULL )
    {
    printf(" Could NOT open file for Velocity \n ");
    return -1;
    }

    for ( i = 0; i <=N; i++ )
    {
    fprintf(fpP, "%15.10e \n", p[i]);
    fprintf(fpU, "%15.10e \n", u[i]);
    }


    fclose(fpP);
    fclose(fpU);*/

  return 0;
}
