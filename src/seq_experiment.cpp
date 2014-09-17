#include "seq_experiment.h"

#include "population.h"
#include "timer.h"

#include <assert.h>
#include <omp.h>

#include <iostream>
#include <sstream>

using namespace NEAT;
using namespace std;

#define NACTIVATES_PER_INPUT 10

static void init_env() {
    const bool DELETE_NODES = true;
    const bool DELETE_LINKS = true;

    NEAT::compat_threshold = 10.0;

    if(DELETE_NODES) {
        NEAT::mutate_delete_node_prob = 0.001;
    }

    if(DELETE_LINKS) {
        NEAT::mutate_delete_link_prob = 0.01;

        NEAT::mutate_toggle_enable_prob = 0.0;
        NEAT::mutate_gene_reenable_prob = 0.0;
    }
}

struct Step {
    vector<real_t> input;
    vector<real_t> output;
    real_t weight;

    real_t err(Network *net,
               float **details_act,
               float **details_err) {
        real_t result = 0.0;

        for(size_t i = 0; i < output.size(); i++) {
            real_t diff = net->get_output(i) - output[i];
            real_t err = diff * diff;

            if(err < (0.05 * 0.05)) {
                err = 0.0;
            }

            result += err;

            **details_act = net->get_output(i);
            **details_err = err;

            (*details_act)++;
            (*details_err)++;
        }

        return result * weight;
    }
};

struct Test {
    vector<Step> steps;
    Test(const vector<Step> &steps_) : steps(steps_) {
        // Insert 1.0 for bias sensor
        for(auto &step: steps) {
            step.input.insert(step.input.begin(), 1.0f);
        }
    }
};

const float S = 1.0; // Signal
const float Q = 1.0; // Query
const float _ = 0.0; // Null

const float A = 0.0;
const float B = 1.0;

const real_t weight_seq = 4;
const real_t weight_delay = 25;
const real_t weight_query = 55;

vector<Test> tests = {
    Test({
            {{S, _, _, A, _}, {_, _, _}, weight_seq},
            {{_, _, _, _, _}, {_, _, _}, weight_seq},
            {{S, _, _, A, _}, {_, _, _}, weight_seq},
            {{_, _, _, _, _}, {_, _, _}, weight_seq},
            {{S, _, _, A, _}, {_, _, _}, weight_seq},
            {{_, _, _, _, _}, {_, _, _}, weight_delay},
            {{_, _, Q, _, _}, {A, A, A}, weight_query}
        }),
    Test({
            {{S, _, _, A, _}, {_, _, _}, weight_seq},
            {{_, _, _, _, _}, {_, _, _}, weight_seq},
            {{S, _, _, A, _}, {_, _, _}, weight_seq},
            {{_, _, _, _, _}, {_, _, _}, weight_seq},
            {{S, _, _, B, _}, {_, _, _}, weight_seq},
            {{_, _, _, _, _}, {_, _, _}, weight_delay},
            {{_, _, Q, _, _}, {A, A, B}, weight_query}
        }),
    Test({
            {{S, _, _, A, _}, {_, _, _}, weight_seq},
            {{_, _, _, _, _}, {_, _, _}, weight_seq},
            {{S, _, _, B, _}, {_, _, _}, weight_seq},
            {{_, _, _, _, _}, {_, _, _}, weight_seq},
            {{S, _, _, A, _}, {_, _, _}, weight_seq},
            {{_, _, _, _, _}, {_, _, _}, weight_delay},
            {{_, _, Q, _, _}, {A, B, A}, weight_query}
        }),
    Test({
            {{S, _, _, A, _}, {_, _, _}, weight_seq},
            {{_, _, _, _, _}, {_, _, _}, weight_seq},
            {{S, _, _, B, _}, {_, _, _}, weight_seq},
            {{_, _, _, _, _}, {_, _, _}, weight_seq},
            {{S, _, _, B, _}, {_, _, _}, weight_seq},
            {{_, _, _, _, _}, {_, _, _}, weight_delay},
            {{_, _, Q, _, _}, {A, B, B}, weight_query}
        }),
    Test({
            {{S, _, _, B, _}, {_, _, _}, weight_seq},
            {{_, _, _, _, _}, {_, _, _}, weight_seq},
            {{S, _, _, A, _}, {_, _, _}, weight_seq},
            {{_, _, _, _, _}, {_, _, _}, weight_seq},
            {{S, _, _, A, _}, {_, _, _}, weight_seq},
            {{_, _, _, _, _}, {_, _, _}, weight_delay},
            {{_, _, Q, _, _}, {B, A, A}, weight_query}
        }),
    Test({
            {{S, _, _, B, _}, {_, _, _}, weight_seq},
            {{_, _, _, _, _}, {_, _, _}, weight_seq},
            {{S, _, _, A, _}, {_, _, _}, weight_seq},
            {{_, _, _, _, _}, {_, _, _}, weight_seq},
            {{S, _, _, B, _}, {_, _, _}, weight_seq},
            {{_, _, _, _, _}, {_, _, _}, weight_delay},
            {{_, _, Q, _, _}, {B, A, B}, weight_query}
        }),
    Test({
            {{S, _, _, B, _}, {_, _, _}, weight_seq},
            {{_, _, _, _, _}, {_, _, _}, weight_seq},
            {{S, _, _, B, _}, {_, _, _}, weight_seq},
            {{_, _, _, _, _}, {_, _, _}, weight_seq},
            {{S, _, _, A, _}, {_, _, _}, weight_seq},
            {{_, _, _, _, _}, {_, _, _}, weight_delay},
            {{_, _, Q, _, _}, {B, B, A}, weight_query}
        }),
    Test({
            {{S, _, _, B, _}, {_, _, _}, weight_seq},
            {{_, _, _, _, _}, {_, _, _}, weight_seq},
            {{S, _, _, B, _}, {_, _, _}, weight_seq},
            {{_, _, _, _, _}, {_, _, _}, weight_seq},
            {{S, _, _, B, _}, {_, _, _}, weight_seq},
            {{_, _, _, _, _}, {_, _, _}, weight_delay},
            {{_, _, Q, _, _}, {B, B, B}, weight_query}
        }),
};

const size_t nsteps = []() {
    size_t n = 0;
    for(auto &test: tests) n += test.steps.size();
    return n;
}();
const size_t nouts = []() {
    return nsteps * tests[0].steps[0].output.size();
}();
const real_t max_err = []() {
    real_t total = 0.0;
    for(auto &t: tests) for(auto &s: t.steps) total += s.weight;
    return total;
}();

static real_t score(real_t errorsum) {
    return 1.0 - errorsum/max_err;
};

static void print(Population *pop, int gen) {
    char filename[1024];
    sprintf(filename, "gen_%d", gen);
    ofstream out(filename);
    pop->write(out);
}

static int epoch(NEAT::Population *pop,
                 int generation,
                 int &winnernum,
                 int &winnergenes,
                 int &winnernodes);

//Perform evolution on SEQ_EXPERIMENT, for gens generations
void seq_experiment(rng_t &rng, int gens) {
    init_env();

    Genome *start_genome;

    int evals[NEAT::num_runs];
    int genes[NEAT::num_runs];
    int nodes[NEAT::num_runs];
    int winnernum;
    int winnergenes;
    int winnernodes;
    //For averaging
    int totalevals=0;
    int totalgenes=0;
    int totalnodes=0;
    int totalgens=0;
    int expcount;
    int samples;  //For averaging

    memset (evals, 0, NEAT::num_runs * sizeof(int));
    memset (genes, 0, NEAT::num_runs * sizeof(int));
    memset (nodes, 0, NEAT::num_runs * sizeof(int));

    cout<<"START SEQ_EXPERIMENT TEST"<<endl;

    start_genome = Genome::create_seed_genome(rng,
                                              1,
                                              tests[0].steps[0].input.size() - 1,
                                              tests[0].steps[0].output.size(),
                                              3);

    for(expcount=0;expcount<NEAT::num_runs;expcount++) {
        //Spawn the Population
        Population *pop = new Population(rng, start_genome,NEAT::pop_size);
      
        bool success = false;
        int gen;
        for (gen=1; !success && (gen <= gens); gen++) {
            cout<<"Epoch "<<gen<<endl;	

            static Timer timer("epoch");
            timer.start();
            //Check for success
            if (epoch(pop,gen,winnernum,winnergenes,winnernodes)) {
                success = true;
                //Collect Stats on end of experiment
                evals[expcount]=NEAT::pop_size*(gen-1)+winnernum;
                genes[expcount]=winnergenes;
                nodes[expcount]=winnernodes;
                totalgens += gen;
            }
            timer.stop();
            Timer::report();

            //Don't print on success because we'll exit the loop and print then.
            if(!success && (gen % NEAT::print_every == 0))
                print(pop, gen);
        }

        print(pop, gen - 1);

        delete pop;
    }

    delete start_genome;


    //Average and print stats
    cout<<"Nodes: "<<endl;
    for(expcount=0;expcount<NEAT::num_runs;expcount++) {
        cout<<nodes[expcount]<<endl;
        totalnodes+=nodes[expcount];
    }
    
    cout<<"Genes: "<<endl;
    for(expcount=0;expcount<NEAT::num_runs;expcount++) {
        cout<<genes[expcount]<<endl;
        totalgenes+=genes[expcount];
    }
    
    cout<<"Evals "<<endl;
    samples=0;
    for(expcount=0;expcount<NEAT::num_runs;expcount++) {
        cout<<evals[expcount]<<endl;
        if (evals[expcount]>0)
        {
            totalevals+=evals[expcount];
            samples++;
        }
    }

    cout<<"Failures: "<<(NEAT::num_runs-samples)<<" out of "<<NEAT::num_runs<<" runs"<<endl;
    cout<<"Average Generations: "<<real_t(totalgens)/expcount<<endl;
    cout<<"Average Nodes: "<<(samples>0 ? (real_t)totalnodes/samples : 0)<<endl;
    cout<<"Average Genes: "<<(samples>0 ? (real_t)totalgenes/samples : 0)<<endl;
    cout<<"Average Evals: "<<(samples>0 ? (real_t)totalevals/samples : 0)<<endl;
}

bool evaluate(Organism *org, float *details_act, float *details_err) {
    Network *net;

    net=&org->net;

    auto activate = [net] (vector<real_t> &input) {
        net->load_sensors(input);

        for(size_t i = 0; i < NACTIVATES_PER_INPUT; i++) {
            net->activate();
        }
    };

    float errorsum = 0.0;

    {
        for(auto &test: tests) {
            for(auto &step: test.steps) {
                activate(step.input);
                errorsum += step.err( net, &details_act, &details_err );
            }

            net->flush();
        }
    }
 
    org->fitness = score(errorsum);
    org->error=errorsum;

    org->winner = org->fitness >= 0.9999999;
    if(org->winner) {
        cout << "FOUND A WINNER: " << org->fitness << endl;
        cout.flush();
    }

    return org->winner;
}

int epoch(Population *pop,
          int generation,
          int &winnernum,
          int &winnergenes,
          int &winnernodes) {

    static float best_fitness = 0.0f;

    vector<Species*>::iterator curspecies;

    bool win=false;
    //Evaluate each organism on a test

    bool best = false;
    size_t i_best;

    static Timer timer("evaluate");
    timer.start();

    const size_t n = pop->size();
    //todo: should only allocate once
    float *details_act = new float[n * nouts];
    float *details_err = new float[n * nouts];
#pragma omp parallel for
    for(size_t i = 0; i < n; i++) {
        Organism *org = pop->get(i);
        size_t details_offset = i * nouts;
        if (evaluate(org, details_act + details_offset, details_err + details_offset)) {
#pragma omp critical
            {
                win=true;
                winnernum=org->genome.genome_id;
                winnergenes=org->genome.extrons();
                winnernodes=org->genome.nodes.size();
            }
        }

        if(org->fitness > best_fitness) {
#pragma omp critical
            if(org->fitness > best_fitness) {
                best = true;
                i_best = i;
                best_fitness = org->fitness;
            }
        }
    }

    timer.stop();

    if(best) {
        float *best_act = details_act + i_best * nouts;
        float *best_err = details_err + i_best * nouts;
        Organism *org = pop->get(i_best);
        
        printf("new best_fitness=%f; fitness=%f, errorsum=%f -- activation (err)\n",
               (float)best_fitness, (float)org->fitness, (float)org->error);

        for(auto &test: tests) {
            for(auto &step: test.steps) {
                for(size_t i = 0; i < step.output.size(); i++) {
                    printf("%f (%f) ", *best_act++, *best_err++);
                }
                printf("\n");
            }
            printf("---\n");
        }
    }

    delete [] details_act;
    delete [] details_err;
  
    pop->next_generation();

    if (win) return 1;
    else return 0;

}
