/*
  Copyright 2001 The University of Texas at Austin

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

  http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/
#ifndef _INNOVATION_H_
#define _INNOVATION_H_

#include "neat.h"
#include <functional>
#include <map>
#include <vector>

namespace NEAT {

	enum innovtype {
		NEWNODE = 0,
		NEWLINK = 1
	};

    class InnovationId {
    public:
		innovtype innovation_type;
		int node_in_id;
		int node_out_id;
		double old_innov_num;
		bool recur_flag;

        // Node
        InnovationId(int nin, int nout, double oldinnov);
        // Link
        InnovationId(int nin, int nout, bool recur);

        bool operator<(const InnovationId &other) const;
        bool operator==(const InnovationId &other) const;
    };

    class Innovation;

    class InnovationParms {
    public:
		double new_weight;
		int new_trait_id;

        InnovationParms();
        InnovationParms(double w, int t);
    };

    class IndividualInnovation {
    public:
        typedef std::function<void (const Innovation *innov)> ApplyFunc;

        int population_index;
        InnovationId id;
        InnovationParms parms;
        ApplyFunc apply;

		//Constructor for the new node case
        IndividualInnovation(int population_index_,
                             InnovationId id_,
                             InnovationParms parms_,
                             ApplyFunc apply_);
    };

	class Innovation {
	public:
        InnovationId id;
        InnovationParms parms;

		double innovation_num1;  //The number assigned to the innovation
		double innovation_num2;  // If this is a new node innovation, then there are 2 innovations (links) added for the new node 
		int newnode_id;  // If a new node was created, this is its node_id 

        // Link
        Innovation(InnovationId id_,
                   InnovationParms parms_,
                   double innovation_num1_);

        // Node
        Innovation(InnovationId id_,
                   InnovationParms parms_,
                   double innovation_num1_,
                   double innovation_num2_,
                   int newnode_id_);
	};

    class PopulationInnovations {
        std::vector<IndividualInnovation> innovations;  // For holding the genetic innovations of the newest generation
        std::map<InnovationId, std::vector<IndividualInnovation>> id2inds;
		int cur_node_id;
		double cur_innov_num;

    public:
        void init(int node_id, double innov_num);
        void add(const IndividualInnovation &innov);
        void apply();
    };
} // namespace NEAT

#endif
