#!/bin/bash
cd ${0%/*} || exit 1    		    		# Run from this directory

# Solid participant

# Run this script in one terminal in order to start this participant.
# Run this script with "-parallel" for parallel simulations

# 1 for true, 0 for false
parallel=0
if [ "$1" = "-parallel" ]; then
    parallel=1
fi

echo "Preparing and running the Solid participant..."

# Run
if [ $parallel -eq 1 ]; then
    mpiexec -np 4 ./build/SolidSolver ../precice-config.xml 100 -parallel
else
    ./build/SolidSolver ../precice-config.xml 100
fi

echo ""
