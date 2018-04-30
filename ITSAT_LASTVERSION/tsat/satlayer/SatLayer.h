#ifndef SATLAYER_H
#define SATLAYER_H

#include <fstream>
#include <vector>
#include "precosat/precosat.h"
#include "minisat/simp/SimpSolver.h"
#include "minisat/utils/System.h"
#include "tsat/graph/ITGraph.h" // for Mutex

#include <iostream>

using namespace std;

namespace nsSatLayer
{

/// candidate return values of a generic sat solver, generated by SatLayer::Solve()
enum Outcome
{
	SAT, UNSAT, TIMEOUT, MEMOUT, INDET, TIMEOUT_FORMULA
};

/// mainly used for reporting, the only necessary part is "outcome", generated by SatLayer::Solve()
struct SATResults
{
	int T;                  // associated makespan (in terms of layers)
	Outcome outcome;        // outcome gained with that particular T
	int varProp, varAction; // total number of variables for propositions, actions
	int varTotal;           // total number of variables for propositions, actions, and automata
	double time_overall;    // total time used to encode and solve the SAT formula
	
	// counters are used for reporting and to analyse the performance of the planner and approach
	long long ccEvent;
	long long ccInit, ccGoal, ccNotApplicable, ccCondition, ccEffect, ccAxiomDelete, ccAxiomAdd;
	long long ccMutexStart, ccMutexEnd, ccMutexStartEnd, ccMutexOverall;
	long long totalMutexes, totalClauses;
};

/// each event in the "leveled plan" or "event plan" can have one of these types:
enum ActionPart
{
	ACTION_START, ACTION_END, ACTION_OVERALL
};

/// Leveled Plan consists of PlanEvent items, look into SatLayer::GeneratePlan() for more info
struct PlanEvent
{
	int id;          // id of associated action in MyParser::PProblem.pAllActions
	ActionPart part; // type of the event (start, end, overall)
	int t;           // the level that the event takes place
	int pair;        // if it is of type start or end, "pair" contains the index of its pair in the plan
	Minisat::Lit var;         // FIXME: associated variable in Minisat
	double doubleD;  // after solving the Simple Temporal Network, it contains the real time associated with event
	                 // meaningful only for start and end events
	inline bool operator< (const PlanEvent& other) const
	{
		if(fabs(doubleD - other.doubleD) > 0.00001) // i.e. doubleD != other.doubleD
			return doubleD < other.doubleD;
		else
			return false;
	}
};

//class SatLayer; // forward declaration
//class SatLayerMinisat; // forward declaration

// abstract class, contains must of implementations of encoding phase
class SatLayer
{
	
	ofstream *cnf, *readcnf;
	string detailsFileNameTemplate;

	bool GEN_CNF, GEN_READABLE_CNF; // associated with "-cnf" and "-readablecnf" switches
	int CONFLICTS_METHOD;           // associated with "-conflicts" switch, 1==nlogn, 2==n2, 3==n2all

	// counters are used for reporting and to analyse the performance of the planner and approach
	long long ccEvent, ccInit, ccGoal, ccNotApplicable, ccCondition, ccEffect, ccAxiomDelete, 
		ccAxiomAdd, ccMutexStart, ccMutexEnd, ccMutexStartEnd, ccMutexOverall;

protected:
	
	int T;                      // current makespan (in terms of layers)
	float startTime, totalTime; // time when the encoding is started, and time when we are ready to return
	bool DONT_SOLVE;            // associated with "-dontsolve" switch
	SATResults *satresults;     // contains the info that is returned to Alg2Layer()
	string planFileName;        // eg. "pfile01.sol", later an index is append to it, eg. "pfile01.sol.1"
	int nProp, nAction;         // number of propositions and actions
	int nvProp, nvEvent;        // number of variables for propositions and events
	int *index2var;             // gets an special id of a proposition or action, and returns its corresponding variable
	vector<int> var2index;      // gets a variable, and returns the special id of associated proposition or action
	                            // more info at PropTime2Var() and ActionTime2Var()

	void AddFactMutexes(vector< vector<Mutex> > *fact_mutex); // accepts the mutex between facts (and probably the overall variables)
	                                                          // from planning graph, and appends them to the encoding
	void AddPropForbidClauses(vector<int> *prop_layer);       // accepts the layers in which each proposition is first appeared
	                                                          // in the planning graph, and adds the required unit clauses
	                                                          // to forbid the corresponding propositions to become true before
	                                                          // that layer
	void AddActionVariableRelations(bool *compress);       // adds the clauses to ensure that the necessary constraints between events hold
	                                         // eg. of a constraint: when an action is closed, its end event must have happened
	                                         // encoding of that constraint: a_open(i) && ~a_open(i+1) => a_end(i)
	void AddInitialState();                  // adds the propositions in initial state in form of unit clause to encoding
	                                         // initial state holds at time == 0 (includes both positive and negative literals)
	void AddGoals();                         // adds the propositions specified as goals in form of unit clause to encoding
	                                         // goals hold at time == T (includes just positive literals)
	void AddConditionsEffects();             // adds the clauses to encode the preconditions and effects of each event
	                                         // contains a newly introduced variable Pre(p)
	void Old_AddConditionsEffects();         // same as the new method, but without Pre(p), requires the Old_AddMutexStart_End(),
	                                         // and is not compatible with AddMutexStart_End_n2() and AddMutexStart_End_nlogn()
	                                         // because the two latters ignore some kind of mutex that the Pre(p) prevents automatically
	                                         
	void AddExplanatoryAxioms();             // adds frame axioms for propositions;
	                                         // frame axioms for event variables are added by AddActionVariableRelations()
	void AddMutexStart_End_n2();             // inherently similar to Old_AddMutexStart_End() but different in two aspects:
	                                         // 1. it is much faster since it uses the list of actions that alter or need a proposition
	                                         //    these lists are available from each proposition beforehand
	                                         // 2. it igonores any mutex that the Pre(p) prevents automatically
	void AddMutexStart_End_nlogn();          // it is much like the AddMutexStart_End_n2(), but provides a much more compact encoding
	void Old_AddMutexStart_End(bool **mutextable, bool *compress);            // much slower that the other two, since it scans the whole set of propositions for each action
	                                         // also it includes all mutexes including those that Pre(p) can prevent
	void Report_CNFStats(bool silent=false); // after the encoding is done, it reports the stats and saves them to satresults structure

public:
PrecoSat::Solver *precosatSolver;

	virtual void restart()=0;	
	vector<PlanEvent> plan; // when the encoding is solved, it is filled with the event plan, by SatLayer::GeneratePlan()

	virtual int SATNewVar(int isAction)=0; // used in ActionTime2Var() and PropTime2Var()

	// must of the usage is within the SatLayer class, but also AddNegCycleAutomaton() and AddNegCycleAutomaton_General() use them
	virtual void ResetClause()=0;
	virtual void AddToClause(int n, bool polarity)=0;
	virtual void AddClause(long long &counter)=0;
	virtual int PropTime2Var(int p, int t)=0;
	virtual int ActionTime2Var(int a, int t, int part)=0;
	
	// Typical usage of these 3 functions, eg. for "NOT(A) OR B":
	//      ResetClause();
	//      AddToClause(A, false);
	//      AddToClause(B, true);
	//      AddClause(ccMutexStart);
	// assuming that it is a start-start mutex

	/// accepts a proposition, a layer, and the indication that P or Pre(P) is going to be encoded
	/// and returns the associated variable; the variable will be created if none is associated with


	SatLayer(
		int t, SATResults *results,
		string planfilename, string detailsfilenametemplate,
		float totaltime, bool gen_cnf, bool gen_readable_cnf, bool dont_solve, int conflictsMethod, PrecoSat::Solver *precosatSolver);
	virtual ~SatLayer()
	{}
	void DoEncoding(bool **mutextable, bool *compress); // calls all required functions to encode the SAT formula, without graph
	void DoEncoding(vector< vector<Mutex> > *fact_mutex, vector<int> *prop_layer); // same as DoEncoding(), but uses the graph
	virtual void Solve() = 0; // calls the appropriate SAT solver to solve the formula and returns the status of SAT solver
	virtual void GeneratePlan() = 0; // if the formula is satisfied, it uses the satisfing assignments returned by SAT solver
	                                 // and generates the event plan (leveled plan) and stores it in "vector<PlanEvent> plan"
	virtual void setT(int T) = 0;								 

	void SplitLayers(int factor); // the "Splitting Idea", there was serious problems in the approach when it was implemented
};

// instantiable class, implements the abstract parts of class SatLayer, which are Sat Solver Specific
// this class is specific for Minisat2
class SatLayerMinisat : public SatLayer
{
	Minisat::SimpSolver *minisatSolver;
    Minisat::vec<Minisat::Lit> lits;
	
	inline void ResetClause()
	{
		lits.clear();
	}
	inline void AddToClause(int n, bool polarity)
	{
		if(polarity)
			lits.push(Minisat::mkLit(n-1));
		else
			lits.push(~Minisat::mkLit(n-1));
	}
	inline void AddClause(long long &counter)
	{
		counter++;
		minisatSolver->addClause_(lits);
	}
	inline int SATNewVar(int isAction)
	{
		return minisatSolver->newVar();
	}
public:
	SatLayerMinisat(
		int t, SATResults *results,
		string planfilename, string detailsfilenametemplate,
		float totaltime, bool gen_cnf, bool gen_readable_cnf, bool dont_solve, int conflictsMethod)
		:
	SatLayer(t, results, planfilename, detailsfilenametemplate,
		totaltime, gen_cnf, gen_readable_cnf, dont_solve, conflictsMethod, 0)
	{
		minisatSolver = new Minisat::SimpSolver();
	}
	~SatLayerMinisat()
	{
		if(minisatSolver)
			delete minisatSolver;
	}
	void Solve();
	void GeneratePlan();
};

// instantiable class, implements the abstract parts of class SatLayer, which are Sat Solver Specific
// this class is specific for Precosat
class SatLayerPrecosat : public SatLayer
{
	
	
	inline void ResetClause()
	{
		precosatSolver->ResetClause();
		// not needed with Precosat
	}
	inline void AddToClause(int n, bool polarity)
	{
		precosatSolver->AddToClause(n, polarity);
		/*if(polarity)
			precosatSolver->add(2 * n);
		else
			precosatSolver->add(2 * n + 1);*/
	}
	inline void AddClause(long long &counter)
	{
		precosatSolver->AddClause(counter);
		++counter;
		//precosatSolver->add(0);
	}
	inline int SATNewVar(int isAction)
	{
		return (precosatSolver->SATNewVar(isAction))-1;
		/*int res;
		res = precosatSolver->next()-1;
		if (isAction==1)
			precosatSolver->setaction(res+1);
		return res;*/
	}

	inline int PropTime2Var(int p, int t)
	{
		int propIndex = 2*p  + 2*t*nProp;
		if(index2var[propIndex]==-1)
		{
			index2var[propIndex] = precosatSolver->PropTime2Var(p, t)-1;
			var2index.push_back(propIndex);
		}
		return 1+index2var[propIndex];
	}
	/// accepts an action, a layer, and the type of event
	/// and returns the associated variable; the variable will be created if none is associated with
	inline int ActionTime2Var(int a, int t, int part) // part: 0==start, 1==end, 2==overall
	{
		int actionIndex = 2*nvProp + part + (a+t*nAction)*3; // 2*nvProp shifts the actionIndex so the propIndex will remain unchanged
		//cout<<"ITSAT AT2V "<<index2var[actionIndex]<<'\n';
		if(index2var[actionIndex]==-1)
		{
			index2var[actionIndex] = precosatSolver->ActionTime2Var(a, t, part)-1;
			var2index.push_back(actionIndex);
		}
		return 1+index2var[actionIndex];
	}	
	
	
	
public:
	void restart(){precosatSolver->reset2();}	
	SatLayerPrecosat(
		int t, SATResults *results,
		string planfilename, string detailsfilenametemplate,
		float totaltime, bool gen_cnf, bool gen_readable_cnf, bool dont_solve, int conflictsMethod, PrecoSat::Solver *precosatSolver)
		:
	SatLayer(t, results, planfilename, detailsfilenametemplate,
		totaltime, gen_cnf, gen_readable_cnf, dont_solve, conflictsMethod, precosatSolver)
	{

	}
	~SatLayerPrecosat()
	{
		if(precosatSolver)
		{
			precosatSolver->reset2();
			//delete precosatSolver;
		}
	}
	void setT(int tt)
	{
		
		cout<<"SLSett1\n";
		T = tt;
		nvProp = nProp * (T+1);
		nvEvent = 3 * nAction* (T+1);
		precosatSolver->setT(tt,nAction,nProp);

		delete index2var;
		index2var = new int[2*nvProp + nvEvent];//[2*nvProp + nvEvent];
		for(int i=0; i<2*nvProp + nvEvent; i++)
			index2var[i] = -1;
		var2index.clear();
			
		cout<<"SLSett2\n";
	}
	void Solve();
	void GeneratePlan();
};

}

#endif
