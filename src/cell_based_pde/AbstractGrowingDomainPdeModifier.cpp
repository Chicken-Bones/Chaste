/*

Copyright (c) 2005-2015, University of Oxford.
All rights reserved.

University of Oxford means the Chancellor, Masters and Scholars of the
University of Oxford, having an administrative office at Wellington
Square, Oxford OX1 2JD, UK.

This file is part of Chaste.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
 * Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.
 * Neither the name of the University of Oxford nor the names of its
   contributors may be used to endorse or promote products derived from this
   software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#include "AbstractGrowingDomainPdeModifier.hpp"
#include "NodeBasedCellPopulation.hpp"
#include "VertexBasedCellPopulation.hpp"
#include "MeshBasedCellPopulation.hpp"
#include "MeshBasedCellPopulationWithGhostNodes.hpp"
#include "PottsBasedCellPopulation.hpp"
#include "CaBasedCellPopulation.hpp"
#include "TetrahedralMesh.hpp"
#include "VtkMeshWriter.hpp"
#include "CellBasedPdeSolver.hpp"
#include "SimpleLinearEllipticSolver.hpp"
#include "AveragedSourcePde.hpp"

template<unsigned DIM>
AbstractGrowingDomainPdeModifier<DIM>::AbstractGrowingDomainPdeModifier()
    : AbstractCellBasedSimulationModifier<DIM>(),
      mDeleteMesh(false),
      mSolution(NULL),
      mpFeMesh(NULL),
      mOutputDirectory(""),
      mCachedDependentVariableName("")
{
    assert(DIM==2);
}

template<unsigned DIM>
AbstractGrowingDomainPdeModifier<DIM>::~AbstractGrowingDomainPdeModifier()
{
    if (this->mDeleteMesh)
    {
        delete mpFeMesh;
    }
}

template<unsigned DIM>
void AbstractGrowingDomainPdeModifier<DIM>::UpdateAtEndOfOutputTimeStep(AbstractCellPopulation<DIM,DIM>& rCellPopulation)
{
#ifdef CHASTE_VTK
    if (DIM>1)
    {
        std::ostringstream time_string;
        time_string << SimulationTime::Instance()->GetTimeStepsElapsed();
        std::string results_file = "pde_results_"+time_string.str();
        VtkMeshWriter<DIM,DIM>* p_vtk_mesh_writer = new VtkMeshWriter<DIM,DIM>(mOutputDirectory, results_file, false);

        ReplicatableVector solution_repl(mSolution);
        std::vector<double> pde_solution;
        for (unsigned i=0; i<mpFeMesh->GetNumNodes(); i++)
        {
           pde_solution.push_back(solution_repl[i]);
        }

        p_vtk_mesh_writer->AddPointData(mCachedDependentVariableName,pde_solution);

        p_vtk_mesh_writer->WriteFilesUsingMesh(*mpFeMesh);
        delete p_vtk_mesh_writer;
    }
#endif //CHASTE_VTK
}

template<unsigned DIM>
void AbstractGrowingDomainPdeModifier<DIM>::GenerateFeMesh(AbstractCellPopulation<DIM,DIM>& rCellPopulation)
{
    // Get FE mesh from Cell Population different for each type of Cell Population
    if(dynamic_cast<MeshBasedCellPopulation<DIM>*>(&rCellPopulation) != NULL)
    {
        if(dynamic_cast<MeshBasedCellPopulationWithGhostNodes<DIM>*>(&rCellPopulation) != NULL)
        {
            EXCEPTION("Currently can't solve PDEs on meshes with ghost nodes");
        }
        else
        {
            mpFeMesh = &(static_cast<MeshBasedCellPopulation<DIM>*>(&rCellPopulation)->rGetMesh());
        }
    }
    else if (dynamic_cast<NodeBasedCellPopulation<DIM>*>(&rCellPopulation) != NULL)
    {
        std::vector<Node<DIM> *> nodes;

        // Get the nodes of the NodesOnlyMesh
        for (typename AbstractMesh<DIM,DIM>::NodeIterator node_iter = rCellPopulation.rGetMesh().GetNodeIteratorBegin();
                 node_iter != rCellPopulation.rGetMesh().GetNodeIteratorEnd();
                 ++node_iter)
        {
                nodes.push_back(new Node<DIM>(node_iter->GetIndex(), node_iter->rGetLocation()));
        }

        mDeleteMesh=true;
        mpFeMesh = new MutableMesh<DIM,DIM>(nodes);
        assert(mpFeMesh->GetNumNodes() == rCellPopulation.GetNumRealCells());

    }
    else if (dynamic_cast<VertexBasedCellPopulation<DIM>*>(&rCellPopulation) != NULL)
    {
        mpFeMesh = static_cast<VertexBasedCellPopulation<DIM>*>(&rCellPopulation)->GetTetrahedralMeshUsingVertexMesh();
    }
    else if (dynamic_cast<PottsBasedCellPopulation<DIM>*>(&rCellPopulation) != NULL)
    {
        std::vector<Node<DIM> *> nodes;

        // Create nodes at the centre of the cells
        for (typename AbstractCellPopulation<DIM>::Iterator cell_iter = rCellPopulation.Begin();
             cell_iter != rCellPopulation.End();
             ++cell_iter)
        {
            nodes.push_back(new Node<DIM>(rCellPopulation.GetLocationIndexUsingCell(*cell_iter), rCellPopulation.GetLocationOfCellCentre(*cell_iter)));
        }

        mDeleteMesh=true;
        mpFeMesh = new MutableMesh<DIM,DIM>(nodes);
        assert(mpFeMesh->GetNumNodes() == rCellPopulation.GetNumRealCells());
    }
    else if (dynamic_cast<CaBasedCellPopulation<DIM>*>(&rCellPopulation) != NULL)
    {
        std::vector<Node<DIM> *> nodes;

        // Create nodes at the centre of the cells
        unsigned cell_index = 0;
        for (typename AbstractCellPopulation<DIM>::Iterator cell_iter = rCellPopulation.Begin();
             cell_iter != rCellPopulation.End();
             ++cell_iter)
        {
            nodes.push_back(new Node<DIM>(cell_index, rCellPopulation.GetLocationOfCellCentre(*cell_iter)));
            cell_index++;
        }

        mDeleteMesh=true;
        mpFeMesh = new MutableMesh<DIM,DIM>(nodes);
        assert(mpFeMesh->GetNumNodes() == rCellPopulation.GetNumRealCells());
    }
    else
    {
        NEVER_REACHED;
    }
}


template<unsigned DIM>
void AbstractGrowingDomainPdeModifier<DIM>::UpdateCellData(AbstractCellPopulation<DIM,DIM>& rCellPopulation)
{
    // Store the PDE solution in an accessible form
    ReplicatableVector solution_repl(this->mSolution);

    // local cell index used by the CA simulation
    unsigned cell_index = 0;

    for (typename AbstractCellPopulation<DIM>::Iterator cell_iter = rCellPopulation.Begin();
                 cell_iter != rCellPopulation.End();
                 ++cell_iter)
    {
        unsigned tet_node_index = rCellPopulation.GetLocationIndexUsingCell(*cell_iter);

        if (dynamic_cast<VertexBasedCellPopulation<DIM>*>(&rCellPopulation) != NULL)
        {
            // Offset to relate elements in vertex mesh to nodes in tetrahedral mesh.
            tet_node_index += rCellPopulation.GetNumNodes();
        }

        if (dynamic_cast<CaBasedCellPopulation<DIM>*>(&rCellPopulation) != NULL)
        {
            // here local cell index corresponds to tet node
            tet_node_index = cell_index;
            cell_index++;
        }

        double solution_at_node = solution_repl[tet_node_index];

        cell_iter->GetCellData()->SetItem(mCachedDependentVariableName, solution_at_node);
    }
}


template<unsigned DIM>
void AbstractGrowingDomainPdeModifier<DIM>::OutputSimulationModifierParameters(out_stream& rParamsFile)
{
    // No parameters to output, so just call method on direct parent class
    AbstractCellBasedSimulationModifier<DIM>::OutputSimulationModifierParameters(rParamsFile);
}

/////////////////////////////////////////////////////////////////////////////
// Explicit instantiation
/////////////////////////////////////////////////////////////////////////////

template class AbstractGrowingDomainPdeModifier<1>;
template class AbstractGrowingDomainPdeModifier<2>;
template class AbstractGrowingDomainPdeModifier<3>;

