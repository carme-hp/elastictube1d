#include "FluidSolver.h"
#include "precice/SolverInterface.hpp"
#include <iostream>
#include <mpi.h>

using namespace precice;
using namespace precice::constants;

int main(int argc, char** argv)
{

  std::cout << "Starting Fluid Solver..." << std::endl;
  if (argc != 5) {
    std::cout << std::endl;
    std::cout << "Fluid: Usage: mpiexec -np <#procs> " << argv[0] << " <configurationFileName> <N> <tau> <kappa>" << std::endl;
    std::cout << std::endl;
    std::cout << "N:     Number of mesh elements, needs to be equal for fluid and structure solver." << std::endl;
    std::cout << "tau:   Dimensionless time step size." << std::endl;
    std::cout << "kappa: Dimensionless structural stiffness." << std::endl;

    return -1;
  }

  MPI_Init(&argc, &argv);

  int domainSize, gridOffset, rank, size, chunkLength;
  double *grid, tau, kappa; // Declare dataset

  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  domainSize = atoi(argv[2]);
  tau = atof(argv[3]);
  kappa = atof(argv[4]);

  if ((domainSize + 1) % size == 0) {
    chunkLength = (domainSize + 1) / size;
    gridOffset = rank * chunkLength;
  } else if (rank < (domainSize + 1) % size) {
    chunkLength = (domainSize + 1) / size + 1;
    gridOffset = rank * chunkLength;
  } else {
    chunkLength = (domainSize + 1) / size;
    gridOffset = ((domainSize + 1) % size) * ((domainSize + 1) / size + 1) + (rank - ((domainSize + 1) % size)) * (domainSize + 1) / size;
  }

  std::vector<double> pressure(chunkLength);
  std::vector<double> pressure_n(chunkLength);
  std::vector<double > crossSectionLength(chunkLength);
  std::vector<double > crossSectionLength_n(chunkLength);
  std::vector<double> velocity(chunkLength);
  std::vector<double> velocity_n(chunkLength);
  std::vector<int> vertexIDs(chunkLength);

  //fluidDataDisplay(pressure, chunkLength);

  std::string configFileName(argv[1]);
  std::string solverName = "FLUID";

  SolverInterface interface(solverName, rank, size);
  interface.configure(configFileName);

  int meshID = interface.getMeshID("Fluid_Nodes");
  int pressureID = interface.getDataID("Pressure", meshID);
  int crossSectionLengthID = interface.getDataID("CrossSectionLength", meshID);

  int dimensions = interface.getDimensions();
  grid = new double[dimensions * chunkLength];
  
  for (int i = 0; i < chunkLength; i++) {
    pressure[i] = 0.0;
    pressure_n[i] = 0.0;
    crossSectionLength[i] = 1.0;
    crossSectionLength_n[i] = 1.0;
    velocity[i] = 1.0 / kappa;
    velocity_n[i] = 1.0 / kappa;
    for (int j = 0; j < dimensions; j++) {
      grid[i * dimensions + j] = j == 0 ? gridOffset + (double)i : 0.0;
    }
  }

  interface.setMeshVertices(meshID, chunkLength, grid, vertexIDs.data());

  interface.initialize();

  double t = 0.0;
  double dt = 0.01;

  if (interface.isActionRequired(actionWriteInitialData())) {
    interface.writeBlockScalarData(pressureID, chunkLength, vertexIDs.data(), pressure.data());
    interface.fulfilledAction(actionWriteInitialData());
  }

  interface.initializeData();

  if (interface.isReadDataAvailable()) {
    interface.readBlockScalarData(crossSectionLengthID, chunkLength, vertexIDs.data(), crossSectionLength.data());
  }

  while (interface.isCouplingOngoing()) {
    int convergenceCounter = 0;
    if (interface.isActionRequired(actionWriteIterationCheckpoint())) {
      interface.fulfilledAction(actionWriteIterationCheckpoint());
    }

     // Call "Solver"
    fluidComputeSolution(rank, size, domainSize, chunkLength, kappa, tau, 0.0, t+dt,
                         pressure.data(), pressure_n.data(), pressure.data(),
                         crossSectionLength.data(), crossSectionLength_n.data(),
                         velocity.data(), velocity_n.data());

    //fluidDataDisplay(pressure, chunkLength);
    //fluidDataDisplay(crossSectionLength, chunkLength);

    interface.writeBlockScalarData(pressureID, chunkLength, vertexIDs.data(), pressure.data());

    interface.advance(dt);

    interface.readBlockScalarData(crossSectionLengthID, chunkLength, vertexIDs.data(), crossSectionLength.data());

    if (interface.isActionRequired(actionReadIterationCheckpoint())) { // i.e. not yet converged
      interface.fulfilledAction(actionReadIterationCheckpoint());
      convergenceCounter++;
    } else {
      t += dt;
      for (int i = 0; i < chunkLength; i++) {
        pressure_n[i] = pressure[i];
        velocity_n[i] = velocity[i];
        crossSectionLength_n[i] = crossSectionLength[i];
      }
    }
  }

  delete [] grid;
  interface.finalize();
  MPI_Finalize();

  return 0;
}
