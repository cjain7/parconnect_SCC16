/**
 * @file    deBruijnGraphGen.hpp 
 * @ingroup graphGen
 * @author  Chirag Jain <cjain7@gatech.edu>
 * @brief   Builds the edgelist for de bruijn graph using BLISS library.
 *
 * Copyright (c) 2015 Georgia Institute of Technology. All Rights Reserved.
 */

#ifndef DE_BRUIJN_GEN_HPP
#define DE_BRUIJN_GEN_HPP

//Includes
#include <mpi.h>
#include <iostream>
#include <vector>

//Own includes
#include "graphGen/common/timer.hpp"

//External includes
#include "debruijn/de_bruijn_node_trait.hpp"
#include "debruijn/de_bruijn_construct_engine.hpp"
#include "debruijn/de_bruijn_nodes_distributed.hpp"

namespace conn 
{
  namespace graphGen
  {
    /**
     * @class                     conn::graphGen::deBruijnGraph
     * @brief                     Builds the edgelist of de Bruijn graph 
     */
    class deBruijnGraph
    {
      public:

        //Kmer size set to 31, and alphabets set to 4 nucleotides
        using Alphabet = bliss::common::DNA;
        using KmerType = bliss::common::Kmer<31, Alphabet>;


        //BLISS internal data structure for storing de bruijn graph
        template <typename EdgeEnc>
          using NodeMapType = typename bliss::de_bruijn::de_bruijn_nodes_distributed<
          KmerType, bliss::de_bruijn::node::edge_exists<EdgeEnc>, int,
          bliss::kmer::transform::lex_less,
          bliss::kmer::hash::farm>;


        //Parser type, depends on the sequence file format
        //We restrict the usage to FASTQ format
        template <typename baseIter>
          using SeqParser = typename bliss::io::FASTQParser<baseIter>;


        /** 
         * @brief                 populates the edge list vector 
         * @param[in]   fileName
         * @param[out]  edgelist
         */
        template <typename E>
        void populateEdgeList( std::vector< std::pair<E, E> > &edgeList, 
            std::string &fileName,
            const mxx::comm &comm)
        {
          Timer timer;

          //Initialize the map
          bliss::de_bruijn::de_bruijn_engine<NodeMapType> idx(comm);

          //Build the de Bruijn graph as distributed map
          idx.template build<SeqParser>(fileName, comm);

          auto it = idx.cbegin();

          //Deriving data type of de Bruijn graph storage container
          using mapPairType = typename std::iterator_traits<decltype(it)>::value_type;
          using constkmerType =  typename std::tuple_element<0, mapPairType>::type; 
          using kmerType = typename std::remove_const<constkmerType>::type; //Remove const from nodetype
          using edgeCountInfoType = typename std::tuple_element<1, mapPairType>::type;

          //Temporary storage for each kmer's neighbors in the graph
          std::vector<kmerType> tmpNeighborVector1;
          std::vector<kmerType> tmpNeighborVector2;

          //Read the index and populate the edges inside edgeList
          for(; it != idx.cend(); it++)
          {
            auto sourceKmer = it->first;

            //Get incoming neighbors
            bliss::de_bruijn::node::node_utils<kmerType, edgeCountInfoType>::get_in_neighbors(sourceKmer, it->second, tmpNeighborVector1);

            //Get outgoing neigbors
            bliss::de_bruijn::node::node_utils<kmerType, edgeCountInfoType>::get_out_neighbors(sourceKmer, it->second, tmpNeighborVector2);

            //Push the edges to our edgeList
            for(auto &e : tmpNeighborVector1)
            {
              static_assert(std::is_same<typename kmerType::KmerWordType, uint64_t>::value, "Kmer word type should be set to uint64_t");

              //Get word data of the canonical form of kmers
              const typename kmerType::KmerWordType* sourceVertexData = std::min(sourceKmer, sourceKmer.reverse_complement()).getData();
              const typename kmerType::KmerWordType* destVertexData = std::min(e, e.reverse_complement()).getData();

              //Push the edge in our data structure
              edgeList.emplace_back(sourceVertexData[0], destVertexData[0]);
            }

            //Same procedure for the outgoing edges
            for(auto &e : tmpNeighborVector2)
            {
              const typename kmerType::KmerWordType* sourceVertexData = std::min(sourceKmer, sourceKmer.reverse_complement()).getData();
              const typename kmerType::KmerWordType* destVertexData = std::min(e, e.reverse_complement()).getData();

              edgeList.emplace_back(sourceVertexData[0], destVertexData[0]);
            }
          }

          timer.end_section("graph generation completed");
        }

    };
  }
}

#endif