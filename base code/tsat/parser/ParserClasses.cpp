#include <string>
#include <algorithm>
#include <map>
#include <vector>
#include <set>
#include <sstream>
#include <iterator>
#include <stdexcept> // for runtime_error
#include "tsat/val/ptree.h"
#include "tsat/parser/ParserClasses.h"
#include "tsat/utils.h"

using namespace std;
using namespace VAL;
#define NOTYPE "notype"
//#define TA_PROJECT

namespace MyParser
{

	PDomain pDomain;
	PProblem pProblem;

	int InnerJoin(const vector<int>::const_iterator &key1Begin, const vector<int>::const_iterator &key1End,
		const vector<int>::const_iterator &key2Begin, const vector<int>::const_iterator &key2End,
		const vector<vector<int> >::const_iterator obj1Begin, const vector<vector<int> >::const_iterator obj1End,
		const vector<vector<int> >::const_iterator obj2Begin, const vector<vector<int> >::const_iterator obj2End,
		const vector<int>::const_iterator &resultKeyBegin, const vector<int>::const_iterator &resultKeyEnd,
		std::back_insert_iterator< vector<vector<int> > > itOut)
	{
		int count = 0;
		vector<vector<int> >::const_iterator it1, it2, it2Backup;
		vector<int>::const_iterator itKey1, itKey2;
		vector<int>::const_iterator itResultKey;

		if(obj1Begin == obj1End)
		{
			for(it2 = obj2Begin; it2 != obj2End; ++it2, ++count)
			{
				++*itOut = *it2;
			}
			return count;
		}


		it2 = obj2Begin;
		for(it1 = obj1Begin; it1 != obj1End && it2 != obj2End; )
		{
			bool inequal = false;
			for(itKey1 = key1Begin, itKey2 = key2Begin; itKey1 != key1End && itKey2 != key2End; ++itKey1, ++itKey2)
			{
				if((*it1)[*itKey1] < (*it2)[*itKey2])
				{
					++it1;
					inequal = true;
					break;
				}
				else if((*it1)[*itKey1] > (*it2)[*itKey2])
				{
					++it2;
					inequal = true;
					break;
				}
			}
			if(inequal)
				continue;
			it2Backup = it2;
			while(it2 != obj2End)
			{
				vector<int> result;
				std::copy(it1->begin(), it1->end(), std::inserter(result, result.begin()));
				int i = 0;
				for(itResultKey = resultKeyBegin; itResultKey != resultKeyEnd; ++itResultKey)
				{
					if(*itResultKey == -1)
						result.push_back((*it2)[i]);
					++i;
				}
				++*itOut = result;
				++count;
				if(++it2 == obj2End)
					break;
				inequal = false;
				for(itKey1 = key1Begin, itKey2 = key2Begin; itKey1 != key1End && itKey2 != key2End; ++itKey1, ++itKey2)
				{
					if((*it1)[*itKey1] != (*it2)[*itKey2])
					{
						inequal = true;
						break;
					}
				}
				if(inequal)
					break;
			}
			if(it1+1 != obj1End)
			{
				inequal = false;
				for(itKey1 = key1Begin; itKey1 != key1End; ++itKey1)
				{
					if((*it1)[*itKey1] != (*(it1+1))[*itKey1])
					{
						inequal = true;
						break;
					}
				}
				if(!inequal)
					it2 = it2Backup;
			}
			++it1;
		}
		return count;
	}

	struct MagicVectorCompare
	{
		vector<int> indices;
		inline bool operator()(const vector<int> &a, const vector<int> &b)
		{
			vector<int>::const_iterator it;
			for(it = indices.begin(); it != indices.end(); ++it)
			{
				if(a[*it] < b[*it])
					return true; // less than
				else if(a[*it] > b[*it])
					return false; // greather than
			}
			return false; // equal
		}
	};

	void GroundOperators2(const PListOperator &_operators, PListProposition &_propositions, PListAction &_actions)
	{
		vector<POperator>::const_iterator itOp;
		vector<PProposition>::const_iterator itProp;
		set<PPredicate>::const_iterator itPred;
		for(itOp = _operators.all.begin(); itOp != _operators.all.end(); ++itOp)
		{
			cout << "Grounding operator: " << itOp->name << "  " << itOp->duration << "  " << itOp->params.size() << endl;

			set<PPredicate> PList;
			std::copy(itOp->addEffectAtStart.all.begin(), itOp->addEffectAtStart.all.end(), std::insert_iterator<set<PPredicate> >(PList, PList.end()));
			std::copy(itOp->delEffectAtStart.all.begin(), itOp->delEffectAtStart.all.end(), std::insert_iterator<set<PPredicate> >(PList, PList.end()));
			std::copy(itOp->conditionAtStart.all.begin(), itOp->conditionAtStart.all.end(), std::insert_iterator<set<PPredicate> >(PList, PList.end()));
			std::copy(itOp->addEffectAtEnd.all.begin(), itOp->addEffectAtEnd.all.end(), std::insert_iterator<set<PPredicate> >(PList, PList.end()));
			std::copy(itOp->delEffectAtEnd.all.begin(), itOp->delEffectAtEnd.all.end(), std::insert_iterator<set<PPredicate> >(PList, PList.end()));
			std::copy(itOp->conditionAtEnd.all.begin(), itOp->conditionAtEnd.all.end(), std::insert_iterator<set<PPredicate> >(PList, PList.end()));
			std::copy(itOp->conditionOverAll.all.begin(), itOp->conditionOverAll.all.end(), std::insert_iterator<set<PPredicate> >(PList, PList.end()));
			
			vector<int> allIndices;
			vector< vector<int> > allObjects;

			for(itPred = PList.begin(); itPred != PList.end(); ++itPred)
			{
				vector<int> indices, backIndices, resultKey;
				for(int i=0; i < itOp->params.paramNames.size(); ++i)
				{
					for(int j=0; j < itPred->params.paramNames.size(); ++j)
					{
						if(itOp->params.paramNames[i] == itPred->params.paramNames[j])
						{
							indices.push_back(i);
							backIndices.push_back(j);
							break;
						}
					}
					if(indices.size() == itPred->params.size())
						break;
				}
				MagicVectorCompare magic, magic2;
				for(int j=0; j < indices.size(); ++j)
				{
					bool found = false;
					for(int i=0; i < allIndices.size(); ++i)
					{
						if(allIndices[i] == indices[j])
						{
							magic.indices.push_back(i);
							resultKey.push_back(i);
							magic2.indices.push_back(j);
							found = true;
							break;
						}
					}
					if(!found)
						resultKey.push_back(-1);
				}
				std::sort(allObjects.begin(), allObjects.end(), magic);

				vector< vector<int> > objects;
				for(itProp = _propositions.all.begin(); itProp != _propositions.all.end(); ++itProp)
				{
					if(itProp->prd->head != itPred->head)
						continue;
					bool imcompatible = false;
					for(int i=0; i<itProp->objects.size(); ++i)
					{
						const string &t = itOp->params.paramTypes[indices[i]];
						const vector<string> &vt = pProblem.pObjects.types[itProp->objects[backIndices[i]]];
						bool typeCompatible = false;
						for(vector<string>::const_iterator itVt = vt.begin(); itVt != vt.end(); ++itVt)
						{
							if(*itVt == t)
							{
								typeCompatible = true;
								break;
							}
						}
						if(!typeCompatible)
						{
							imcompatible = true;
							break;
						}
					}
					if(!imcompatible)
						objects.push_back(itProp->objects);
				}
				std::sort(objects.begin(), objects.end(), magic2);

				vector< vector<int> > tmpObjects;
				InnerJoin(magic.indices.begin(), magic.indices.end(), magic2.indices.begin(), magic2.indices.end(),
					allObjects.begin(), allObjects.end(), objects.begin(), objects.end(),
					resultKey.begin(), resultKey.end(),
					std::back_inserter< vector<vector<int> > >(tmpObjects));
				allObjects.swap(tmpObjects);
				for(int i=0; i<resultKey.size(); ++i)
				{
					if(resultKey[i] == -1)
						allIndices.push_back(indices[i]);
				}
				cout << "\t" << itPred->head;
				for(int i=0; i<itPred->params.paramNames.size(); ++i)
					cout << " " << itPred->params.paramNames[i];
				cout << "\t" << allObjects.size();
			}

			PAction a;
			a.op = &*itOp;
			a.duration = itOp->duration; // if duration == -1 then it will be managed in GroundAction()
			a.n_objects = itOp->params.size();
			a.objects.resize(a.n_objects, -1);
			for(vector< vector<int> >::const_iterator it = allObjects.begin(); it != allObjects.end(); ++it)
			{
				vector<int>::const_iterator it3 = it->begin();
				for(vector<int>::const_iterator it2 = allIndices.begin(); it2 != allIndices.end(); ++it2)
				{
					a.objects[*it2] = *it3++;
				}
				if(a.GroundAction()) // it must hold always, theoretically
					_actions.AddAction(a);
			}
			cout << "\t" << allObjects.size() << endl;
		}
	}

	void GroundOperators(const PObjects &_objects, const PListOperator &_operators, PListAction &_actions)
	{
		int nfp=0;
		// counts number of objects of each type
		map<string,int> objectOfTypeCount;
		for(vector<vector<string> >::const_iterator it = _objects.types.begin(); it != _objects.types.end() ; ++it)
		{
			const vector<string> &types = *it;
			for(vector<string>::const_iterator it = types.begin(); it != types.end() ; ++it)
			{
				objectOfTypeCount[*it]++;
			}
		}
		// for each type stores the index of all corresponding objects
		// it will be used for converting local index to global index
		// used by "perm"
		/* ex:
		0 match1 - match
		1 fuse1 - fuse
		2 fuse2 - fuse
		3 match2 - match
		typeIndex2realIndex["match"] = [0 3]
		typeIndex2realIndex["fuse"] = [1 2]
		now, perm = [1 1], means second match and second fuse
		so result = [3 2]
		*/
		map<string,vector<int> > typeIndex2realIndex;
		int i = 0;
		for(vector<vector<string> >::const_iterator it = _objects.types.begin(); it != _objects.types.end() ; ++it)
		{
			const vector<string> &types = *it;
			for(vector<string>::const_iterator it = types.begin(); it != types.end() ; ++it)
			{
				typeIndex2realIndex[*it].push_back(i);
			}
			i++;
		}

		// this loop generates all ground version of each operator:
		for(vector<POperator>::const_iterator it = _operators.all.begin(); it != _operators.all.end() ; ++it)
		{
			const POperator *op = &*it;
			cout << "Grounding operator: " << op->name << "  " << op->duration << "  " << op->params.size() << endl;
			PAction a;
			a.op = op; //op->addEffectAtEnd.all.begin()->params.
			a.n_objects = op->params.size();
			int *perm = new int[a.n_objects];
			int *permBound = new int[a.n_objects];
			int emptyType = false;
			//int ZZZ = 1;
			for( int j=0; j < a.n_objects; j++)
			{
				perm[j] = 0;
				permBound[j] = objectOfTypeCount[op->params.paramTypes[j]];
				emptyType |= permBound[j] == 0;
				//ZZZ *= objectOfTypeCount[op->params.paramTypes[j]];
			}
			if(emptyType)
				continue;
			//int XXX = 0;
			//int mmm = 0;
			while(true)
			{
				//XXX++; mmm++;
				//if(mmm == 100000)
				//{
				//	cout << XXX << "   " << ZZZ << endl;
				//	mmm = 0;
				//}

				// check validity of "perm"
				// if operator has two parameter with equal type
				//    the objects that "perm" chooses for those two, can not be the same
				bool valid = true;
				//if(pDomain.name != "elevators-time" && pDomain.name != "rover")
				/*for( int j=0; j < a.n_objects && valid; j++ )
				{
					for( int k=0; k < a.n_objects && valid; k++ )
					{
						if( j != k && perm[j] == perm[k] && op->params.paramTypes[j] == op->params.paramTypes[k] )
							valid = false;
					}
				}*/
				// if "perm" is valid then create action
				if( valid )
				{
					a.objects.clear();
					for( int j=0; j < a.n_objects; j++)
					{
						a.objects.push_back( typeIndex2realIndex[op->params.paramTypes[j]] [perm[j]] );
					}
					a.duration = op->duration; // if duration == -1 then it will be managed in GroundAction()
					nfp=a.GroundAction();
					if(nfp==-2) // create conditions and effects and perhaps compute duration
					{
						
						_actions.AddAction(a);
					}
				}
				if (nfp==-2 || nfp==-1)
				{
				// now generate next "perm"
					for( int j=0; j < a.n_objects; j++ )
					{
						perm[j] ++;
						if( perm[j] == permBound[j] ) // if index reached the bound (limit)
						{
							if( j+1 < a.n_objects ) // if there is more indexes
								perm[j] = 0; // do not break, and continue with next index
							else // if it is last index
							{
								perm[j] = -1; // finished grounding operator
								break;
							}
						}
						else
							break;
					}
				}

				else
				{
					for( int j=0; j < nfp; j++ )
						perm[j]=0;
					for( int j=nfp; j < a.n_objects; j++ )
					{
						perm[j] ++;
						if( perm[j] == permBound[j] ) // if index reached the bound (limit)
						{
							if( j+1 < a.n_objects ) // if there is more indexes
								perm[j] = 0; // do not break, and continue with next index
							else // if it is last index
							{
								perm[j] = -1; // finished grounding operator
								break;
							}
						}
						else
							break;
					}
				}
				// if finished then break
				if( a.n_objects==0 || perm[a.n_objects-1] == -1 )
					break;
			}
			delete[] perm;
			delete[] permBound;
		}
	}

	void GroundPredicates(const PListPredicate &_predicates, const PObjects &_objects, PListProposition &_propositions)
	{
		int sz;
		// same as in GroundOperators(), look there for more info
		map<string,int> objectOfTypeCount;
		for(vector<vector<string> >::const_iterator it = _objects.types.begin(); it != _objects.types.end() ; ++it)
		{
			const vector<string> &types = *it;
			for(vector<string>::const_iterator it = types.begin(); it != types.end() ; ++it)
			{
				objectOfTypeCount[*it]++;
			}
		}
		map<string,vector<int> > typeIndex2realIndex;
		int i = 0;
		for(vector<vector<string> >::const_iterator it = _objects.types.begin(); it != _objects.types.end() ; ++it)
		{
			const vector<string> &types = *it;
			for(vector<string>::const_iterator it = types.begin(); it != types.end() ; ++it)
			{
				typeIndex2realIndex[*it].push_back(i);
			}
			i++;
		}

		// now ground the predicate
		sz = _predicates.all.size();
		PProposition prp;
		const PPredicate *prd;
		for( int i=0; i<sz; i++ )
		{
			prd = &_predicates.all[i];
			if(prd->readonly)
				continue;
			prp.prd = prd;
			int n_objects = prd->params.size();
			if(n_objects == 0)
			{
				prp.objects.clear();
				_propositions.AddProposition(prp);
				continue;
			}
			int *perm = new int[n_objects];
			int *permBound = new int[n_objects];
			int emptyType = false;
			for( int j=0; j < n_objects; j++)
			{
				perm[j] = 0;
				permBound[j] = objectOfTypeCount[prd->params.paramTypes[j]];
				emptyType |= permBound[j] == 0;
			}
			if(emptyType)
				continue;
			while(true)
			{
				// check validity of "perm"
				// if predicate has two parameter with equal type
				//    the objects that "perm" chooses for those two, can not be the same
				bool valid = true;
				/*for( int j=0; j < n_objects && valid; j++ )
				{
					for( int k=0; k < n_objects && valid; k++ )
					{
						if( j != k && perm[j] == perm[k] && prd->params.paramTypes[j] == prd->params.paramTypes[k] )
							valid = false;
					}
				}*/

				// if "perm" is valid then create proposition
				if( valid )
				{
					prp.objects.clear();
					for( int j=0; j < n_objects; j++)
					{
						prp.objects.push_back( typeIndex2realIndex[prd->params.paramTypes[j]] [perm[j]] );
					}
					cout << endl << prp.ToFullString() << endl; 
					_propositions.AddProposition(prp);
				}
				// now generate next "perm"
				for( int j=0; j < n_objects; j++ )
				{
					perm[j] ++;
					if( perm[j] == permBound[j] ) // if index reached the bound (limit)
					{
						if( j+1 < n_objects ) // if there is more indexes
							perm[j] = 0; // do not break, and continue with next index
						else // if it is last index
						{
							perm[j] = -1; // finished grounding operator
							break;
						}
					}
					else
						break;
				}
				// if finished then break
				if( perm[n_objects-1] == -1 )
					break;
			}
			delete[] perm;
			delete[] permBound;
		}
		for( int i=0; i<sz; i++ )
		{
			prd = &_predicates.all[i];
			if(prd->readonly)
				continue;
			prp.prd = prd;
			int n_objects = prd->params.size();

			if (!(prd->params.either))
				continue;
			cout << endl << "resid" << endl;
			if(n_objects == 0)
			{
				prp.objects.clear();
				_propositions.AddProposition(prp);
				continue;
			}
			int *perm = new int[n_objects];
			int *permBound = new int[n_objects];
			//int emptyType = false;
			for( int j=0; j < n_objects; j++)
			{

				perm[j] = 0;
				if (j==prd->params.ntype2)
					permBound[j] = objectOfTypeCount[prd->params.type2];
				else
					permBound[j] = objectOfTypeCount[prd->params.paramTypes[j]];
				//emptyType |= permBound[j] == 0;
			}
			//if(emptyType)
				//continue;
			while(true)
			{
				// check validity of "perm"
				// if predicate has two parameter with equal type
				//    the objects that "perm" chooses for those two, can not be the same
				bool valid = true;
				/*for( int j=0; j < n_objects && valid; j++ )
				{
					for( int k=0; k < n_objects && valid; k++ )
					{
						if( j != k && perm[j] == perm[k] && prd->params.paramTypes[j] == prd->params.paramTypes[k] )
							valid = false;
					}
				}*/

				// if "perm" is valid then create proposition
				if( valid )
				{
					prp.objects.clear();
					for( int j=0; j < n_objects; j++)
					{
						if (j==prd->params.ntype2)
							prp.objects.push_back( typeIndex2realIndex[prd->params.type2] [perm[j]] );
						else
							prp.objects.push_back( typeIndex2realIndex[prd->params.paramTypes[j]] [perm[j]] );
					}
					cout << endl << prp.ToFullString() << endl; 
					_propositions.AddProposition(prp);
				}
				// now generate next "perm"
				for( int j=0; j < n_objects; j++ )
				{
					perm[j] ++;
					if( perm[j] == permBound[j] ) // if index reached the bound (limit)
					{
						if( j+1 < n_objects ) // if there is more indexes
							perm[j] = 0; // do not break, and continue with next index
						else // if it is last index
						{
							perm[j] = -1; // finished grounding operator
							break;
						}
					}
					else
						break;
				}
				// if finished then break
				if( perm[n_objects-1] == -1 )
					break;
			}
			delete[] perm;
			delete[] permBound;
		}
	}

	string GroundOperatorDuration(const PPredicate &prd, const PParams &params, const vector<int> &objects)
	{
		// similar to GroundOperatorPredicate(), but it will return a string, eg. "clear A" or "on A B"
		stringstream durationFunction;
		durationFunction << prd.head;
		for(vector<string>::const_iterator it = prd.params.paramNames.begin(); it != prd.params.paramNames.end() ; ++it)
		{
			int i=0;
			// search for current parameter in operator parameters
			int sz = params.size();
			for( i=0; i<sz; i++)
				if(*it == params.paramNames[i]) // if their names are equal
				{
					// put corresponding object in action objects into proposition objects
					durationFunction << " " << objects[i];
					break;
				}
		}
		return durationFunction.str();
	}

	void GroundOperatorPredicate(const PPredicate &prd, PProposition &prp, const PParams &params, const vector<int> &objects)
	{
		prp.prd = &prd;
		prp.objects.clear();
		// for each parameter of predicate, search it in operator parameters
		// and add corresponding object in action objects into proposition objects
		/* ex: Operator Move(x,y,z) : preconditions (predicates) = clear(x) clear(z) on(x,y)
		Action Move01(A,B,C) : precondition (propositions) = clear(A) clear(C) on(A,B)
		"params" : (x,y,z) => operator parameters
		"objects" : (A,B,C) => action objects
		"prd->params.paramNames" : (x) (z) (x,y) => predicate parameters
		"prp->objects" : (A) (C) (A,B) => proposition objects
		*/
		int objectCounter = 0;
		int sz = prd.params.size();
		for( int j=0; j<sz; j++ )
		{
			string p = prd.params.paramNames[j];
			if(!prd.params.paramIsConst[j])
			{
				// search for current parameter in operator parameters
				int sz2 = params.size();
				for( int i=0; i<sz2; i++)
				{
					if(p == params.paramNames[i]) // if their names are equal
					{
						// put corresponding object in action objects into proposition objects
						prp.objects.push_back( objects[i] );
						break;
					}
				}
			}
			else
			{
				prp.objects.push_back( pProblem.pObjects.FindObject(p) );
				++ objectCounter;
			}
		}
		prp.id = pProblem.pAllProposition.FindProposition(prp);
	}

	int ConvertValProposition(const proposition *p, PProposition &prp)
	{
		const PPredicate *prd = pDomain.predicates.FindPredicate(p->head->getName());
		prp.prd = prd;
		for(parameter_symbol_list::const_iterator it
			= p->args->begin(); it != p->args->end(); ++it)
		{
			prp.objects.push_back(pProblem.pObjects.FindObject((*it)->getName()));
		}
		if(prd->readonly)
		{
			// GroundPredicates() does not grounds readonly predicates
			// so add the proposition to pAllProposition set
			pProblem.pAllProposition.AddProposition(prp); // it will set prp.id
		}
		else
		{
			prp.id = pProblem.pAllProposition.FindProposition(prp);
		}
		return prp.id;
	}

	void AddValObject(const const_symbol *obj)
	{
		vector<const pddl_type *> vt;
		PTypes pt;
		if(obj->type)
			vt.push_back(obj->type);
		else if(obj->either_types)
		{
			for(pddl_type_list::const_iterator it2 = obj->either_types->begin();
				it2 != obj->either_types->end(); ++it2)
			{
				vt.push_back(*it2);
			}
		}
		else
			pt.AddType(NOTYPE);
		while(!vt.empty())
		{
			const pddl_type *t = vt.back();
			vt.pop_back();
			pt.AddType(t->getName());
			if(t->type)
				vt.push_back(t->type);
			if(t->either_types)
			{
				for(pddl_type_list::const_iterator it2 = t->either_types->begin();
					it2 != t->either_types->end(); ++it2)
				{
					vt.push_back(*it2);
				}
			}
		}
		pProblem.pObjects.AddObject(obj->getName(), pt.types);
	}

	bool AddProblemClasses(parse_category *top_thing)
	{
		problem *prb = (problem *)top_thing;
		// add objects with maximum 3 level of typing:
		for(const_symbol_list::const_iterator it
			= prb->objects->begin(); it != prb->objects->end(); ++it)
		{
			const const_symbol *obj = *it;
			AddValObject(obj);
		}
		// ground each predicate and create all possible propositions, 3rd parameter is the output
		GroundPredicates(pDomain.predicates, pProblem.pObjects, pProblem.pAllProposition);
		// create initial state
		for(pc_list<simple_effect *>::const_iterator it
			= prb->initial_state->add_effects.begin();
			it != prb->initial_state->add_effects.end(); ++it)
		{
			simple_effect *se = *it;
			const proposition *p = se->prop;
			PProposition prp;
			ConvertValProposition(p, prp);
			pProblem.initialState.push_back(prp.id);
		}
		// assign function values
		for(pc_list<assignment *>::const_iterator it = prb->initial_state->assign_effects.begin();
			it != prb->initial_state->assign_effects.end(); ++it)
		{
			assignment *asgn = *it;
			if(asgn->getOp() == E_ASSIGN) // it is always true, I think
			{
				const int_expression *intx = dynamic_cast<const int_expression *>(asgn->getExpr()); // int
				const float_expression *floatx = dynamic_cast<const float_expression *>(asgn->getExpr()); // float
				if(intx == NULL && floatx == NULL)
					continue;
				const func_term *fterm = asgn->getFTerm();
				stringstream fullName;
				fullName << fterm->getFunction()->getName();
				for(parameter_symbol_list::const_iterator it = fterm->getArgs()->begin(); it != fterm->getArgs()->end(); ++it)
				{
					fullName << " " << pProblem.pObjects.FindObject((*it)->getName());
				}
				if(intx)
					pProblem.functionValues[fullName.str()] = intx->double_value();
				else
					pProblem.functionValues[fullName.str()] = floatx->double_value();
			}
		}
		// create goals
		const simple_goal *g = dynamic_cast<simple_goal *>(prb->the_goal);
		if(g != NULL) // if just one condition
		{
			const proposition *p = ((simple_goal *)g)->getProp();
			PProposition prp;
			ConvertValProposition(p, prp);
			pProblem.goals.push_back(prp.id);
		}
		else
		{
			const goal_list *conditions = ((conj_goal *)prb->the_goal)->getGoals();
			for(goal_list::const_iterator it
				= conditions->begin(); it != conditions->end(); ++it)
			{
				const goal *g = *it;
				const proposition *p = ((simple_goal *)g)->getProp();
				PProposition prp;
				ConvertValProposition(p, prp);
				pProblem.goals.push_back(prp.id);
			}
		}
		// ground operators and create actions (no interaction with VAL)
		#ifndef TA_PROJECT
		//if(pDomain.name == "sokoban-temporal")
			//GroundOperators2(pDomain.operators, pProblem.pAllProposition, pProblem.pAllAction);
		//else
			GroundOperators(pProblem.pObjects, pDomain.operators, pProblem.pAllAction);
		for(vector<PAction>::const_iterator it = pProblem.pAllAction.all.begin(); it != pProblem.pAllAction.all.end(); ++it)
		{
			set<int>::const_iterator it2;
			for(it2 = it->addEffectAtStart.begin(); it2 != it->addEffectAtStart.end(); ++it2)
				pProblem.pAllProposition.GetPropositionById( *it2 )->addEffectAtStart.insert(it->id);
			for(it2 = it->addEffectAtEnd.begin(); it2 != it->addEffectAtEnd.end(); ++it2)
				pProblem.pAllProposition.GetPropositionById( *it2 )->addEffectAtEnd.insert(it->id);
			for(it2 = it->delEffectAtStart.begin(); it2 != it->delEffectAtStart.end(); ++it2)
				pProblem.pAllProposition.GetPropositionById( *it2 )->delEffectAtStart.insert(it->id);
			for(it2 = it->delEffectAtEnd.begin(); it2 != it->delEffectAtEnd.end(); ++it2)
				pProblem.pAllProposition.GetPropositionById( *it2 )->delEffectAtEnd.insert(it->id);
			for(it2 = it->conditionAtStart.begin(); it2 != it->conditionAtStart.end(); ++it2)
				pProblem.pAllProposition.GetPropositionById( *it2 )->conditionAtStart.insert(it->id);
			for(it2 = it->conditionAtEnd.begin(); it2 != it->conditionAtEnd.end(); ++it2)
				pProblem.pAllProposition.GetPropositionById( *it2 )->conditionAtEnd.insert(it->id);
			for(it2 = it->conditionOverAll.begin(); it2 != it->conditionOverAll.end(); ++it2)
				pProblem.pAllProposition.GetPropositionById( *it2 )->conditionOverAll.insert(it->id);
		}
		#endif

		return true;
	}

	bool AddParserClasses(parse_category *top_thing)
	{
		domain *dm = (domain *)top_thing;
		pDomain.name = dm->name;
		// store predicates in pDomain.predicates
		if(dm->isTyped())
		{
			vector<const pddl_type *> vt;
			for(pddl_type_list::const_iterator it
				= dm->types->begin(); it != dm->types->end(); ++it)
			{
				vt.clear();
				vt.push_back(*it);
				while(!vt.empty())
				{
					const pddl_type *t = vt.back();
					pDomain.pTypes.AddType(t->getName());
					vt.pop_back();
					if(t->type)
						vt.push_back(t->type);
					if(t->either_types)
					{
						for(pddl_type_list::const_iterator it2 = t->either_types->begin();
							it2 != t->either_types->end(); ++it2)
						{
							vt.push_back(*it2);
						}
					}
				}
			}
			for(pred_decl_list::const_iterator it
				= dm->predicates->begin(); it != dm->predicates->end(); ++it)
			{
				const pred_decl *p = *it;
				PPredicate P;
				P.head = p->getPred()->getName();
				// even if a predicate has no arguments or just a single argument
				//      ->getArgs() will return {var_symbol_list *}
				int t2=0;
				P.params.either=false;
				for(var_symbol_list::const_iterator it
					= p->getArgs()->begin(); it != p->getArgs()->end(); ++it)
				{
					const var_symbol *arg = *it;

					if(arg->type)
					{
						
						P.params.AddParam(arg->getName(), arg->type->getName());
					}
					else
					{
						P.params.either=true;
						P.params.ntype2=t2;
						
						pddl_type_list::const_iterator it2 = arg->either_types->begin();
						const pddl_type *thistype= *it2;
						P.params.AddParam(arg->getName(), thistype->getName());
						it2++;
						thistype=*it2;
						P.params.type2=thistype->getName();

						cout << "Error: The domain is typed; But the argument \"" << arg->getName() << "\" of predicate \""
							<< P.head << "\" is not typed." << endl;
						//cout << "Error: The domain is typed; But the argument \"" << arg->getName() << "\" of predicate \""
							//<< P.head << "\" is not typed." << endl; 
						//return false;
					}
					t2++;
				}
				pDomain.predicates.AddPredicate(P);
			}
		}
		else
		{
			pDomain.pTypes.AddType(NOTYPE);
			for(pred_decl_list::const_iterator it
				= dm->predicates->begin(); it != dm->predicates->end(); ++it)
			{
				const pred_decl *p = *it;
				PPredicate P;
				P.head = p->getPred()->getName();
				// even if a predicate has no arguments or just a single argument
				//      ->getArgs() will return {var_symbol_list *}
				for(var_symbol_list::const_iterator it
					= p->getArgs()->begin(); it != p->getArgs()->end(); ++it)
				{
					const var_symbol *arg = *it;
					P.params.AddParam(arg->getName(), NOTYPE);
				}
				pDomain.predicates.AddPredicate(P);
			}
		}
		// add objects with maximum 3 level of typing:
		if( dm->constants )
			for(const_symbol_list::const_iterator it
				= dm->constants->begin(); it != dm->constants->end(); ++it)
			{
				const const_symbol *obj = *it;
				AddValObject(obj);
			}
		// store operators in pDomain.operators
		for(operator_list::const_iterator it
			= dm->ops->begin(); it != dm->ops->end(); ++it)
		{
			operator_ *o = *it;
			POperator O;
			if(dynamic_cast<durative_action *>(o) != NULL)
			{
				// req :durative_actions
				durative_action *da = (durative_action *)o;
				O.name = da->name->getName();
				// get duration:
				const timed_goal *x1;
				const comparison *x2;
				const int_expression *x3;
				const func_term *x4;
				const mul_expression *x5;
				const div_expression *x6;
				x1 = (const timed_goal *)da->dur_constraint;
				x2 = (const comparison *)x1->getGoal();
				if(x3 = dynamic_cast<const int_expression *>(x2->getRHS()))
					O.duration = (float)x3->double_value();
				else
				{
					O.duration = -1;
					O.durOperator = 0;
					if(x5 = dynamic_cast<const mul_expression *>(x2->getRHS()))
					{
						if(const int_expression *f = dynamic_cast<const int_expression *>(x5->getLHS()))
						{
							O.durOperand = f->double_value();
							if(const int_expression *f2 = dynamic_cast<const int_expression *>(x5->getRHS()))
							{
								O.type=0; //n*n
								O.durOperand2 = f2->double_value();
							}
							else if(const float_expression *f2 = dynamic_cast<const float_expression *>(x5->getRHS()))
							{
								O.type=0;
								O.durOperand2 = f2->double_value();
							}
							else if(const func_term *f2 = dynamic_cast<const func_term *>(x5->getRHS()))
							{
								O.type=1; //n*f()
								O.durationFunction2.head = f2->getFunction()->getName();
								for(parameter_symbol_list::const_iterator it
									= f2->getArgs()->begin(); it != f2->getArgs()->end(); ++it)
								{
									const parameter_symbol *arg = *it;
									if( arg->type )
										O.durationFunction2.params.AddParam(arg->getName(), arg->type->getName());
									else
										O.durationFunction2.params.AddParam(arg->getName(), NOTYPE);
								}								

							}
						}
						else if(const float_expression *f = dynamic_cast<const float_expression *>(x5->getLHS()))
						{
							O.durOperand = f->double_value();
							if(const int_expression *f2 = dynamic_cast<const int_expression *>(x5->getRHS()))
							{
								O.type=0; //n*n
								O.durOperand2 = f2->double_value();
							}
							else if(const float_expression *f2 = dynamic_cast<const float_expression *>(x5->getRHS()))
							{
								O.type=0;
								O.durOperand2 = f2->double_value();
							}
							else if(const func_term *f2 = dynamic_cast<const func_term *>(x5->getRHS()))
							{
								O.type=1; //n*f()
								O.durationFunction2.head = f2->getFunction()->getName();
								for(parameter_symbol_list::const_iterator it
									= f2->getArgs()->begin(); it != f2->getArgs()->end(); ++it)
								{
									const parameter_symbol *arg = *it;
									if( arg->type )
										O.durationFunction2.params.AddParam(arg->getName(), arg->type->getName());
									else
										O.durationFunction2.params.AddParam(arg->getName(), NOTYPE);
								}								

							}
						}
						else if(const func_term *f = dynamic_cast<const func_term *>(x5->getLHS()))
						{
							O.durationFunction.head = f->getFunction()->getName();
							for(parameter_symbol_list::const_iterator it
								= f->getArgs()->begin(); it != f->getArgs()->end(); ++it)
							{
								const parameter_symbol *arg = *it;
								if( arg->type )
									O.durationFunction.params.AddParam(arg->getName(), arg->type->getName());
								else
									O.durationFunction.params.AddParam(arg->getName(), NOTYPE);
							}

							if(const int_expression *f2 = dynamic_cast<const int_expression *>(x5->getRHS()))
							{
								O.type=2; //f()*n
								O.durOperand2 = f2->double_value();
							}
							else if(const float_expression *f2 = dynamic_cast<const float_expression *>(x5->getRHS()))
							{
								O.type=2;
								O.durOperand2 = f2->double_value();
							}

							else if(const func_term *f2 = dynamic_cast<const func_term *>(x5->getRHS()))
							{
								O.type=3; //f()*f()
								O.durationFunction2.head = f2->getFunction()->getName();
								for(parameter_symbol_list::const_iterator it
									= f2->getArgs()->begin(); it != f2->getArgs()->end(); ++it)
								{
									const parameter_symbol *arg = *it;
									if( arg->type )
										O.durationFunction2.params.AddParam(arg->getName(), arg->type->getName());
									else
										O.durationFunction2.params.AddParam(arg->getName(), NOTYPE);
								}								

							}
						}
						O.durOperator = '*';

					}
					else if(x6 = dynamic_cast<const div_expression *>(x2->getRHS()))
					{
						if(const int_expression *f = dynamic_cast<const int_expression *>(x6->getLHS()))
						{
							O.durOperand = f->double_value();
							if(const int_expression *f2 = dynamic_cast<const int_expression *>(x6->getRHS()))
							{
								O.type=0; //n*n
								O.durOperand2 = f2->double_value();
							}
							else if(const float_expression *f2 = dynamic_cast<const float_expression *>(x6->getRHS()))
							{
								O.type=0;
								O.durOperand2 = f2->double_value();
							}
							else if(const func_term *f2 = dynamic_cast<const func_term *>(x6->getRHS()))
							{
								O.type=1; //n*f()
								O.durationFunction2.head = f2->getFunction()->getName();
								for(parameter_symbol_list::const_iterator it
									= f2->getArgs()->begin(); it != f2->getArgs()->end(); ++it)
								{
									const parameter_symbol *arg = *it;
									if( arg->type )
										O.durationFunction2.params.AddParam(arg->getName(), arg->type->getName());
									else
										O.durationFunction2.params.AddParam(arg->getName(), NOTYPE);
								}								

							}
						}
						else if(const float_expression *f = dynamic_cast<const float_expression *>(x6->getLHS()))
						{
							O.durOperand = f->double_value();
							if(const int_expression *f2 = dynamic_cast<const int_expression *>(x6->getRHS()))
							{
								O.type=0; //n*n
								O.durOperand2 = f2->double_value();
							}
							else if(const float_expression *f2 = dynamic_cast<const float_expression *>(x6->getRHS()))
							{
								O.type=0;
								O.durOperand2 = f2->double_value();
							}
							else if(const func_term *f2 = dynamic_cast<const func_term *>(x6->getRHS()))
							{
								O.type=1; //n*f()
								O.durationFunction2.head = f2->getFunction()->getName();
								for(parameter_symbol_list::const_iterator it
									= f2->getArgs()->begin(); it != f2->getArgs()->end(); ++it)
								{
									const parameter_symbol *arg = *it;
									if( arg->type )
										O.durationFunction2.params.AddParam(arg->getName(), arg->type->getName());
									else
										O.durationFunction2.params.AddParam(arg->getName(), NOTYPE);
								}								

							}
						}
						else if(const func_term *f = dynamic_cast<const func_term *>(x6->getLHS()))
						{
							O.durationFunction.head = f->getFunction()->getName();
							for(parameter_symbol_list::const_iterator it
								= f->getArgs()->begin(); it != f->getArgs()->end(); ++it)
							{
								const parameter_symbol *arg = *it;
								if( arg->type )
									O.durationFunction.params.AddParam(arg->getName(), arg->type->getName());
								else
									O.durationFunction.params.AddParam(arg->getName(), NOTYPE);
							}

							if(const int_expression *f2 = dynamic_cast<const int_expression *>(x6->getRHS()))
							{
								O.type=2; //f()*n
								O.durOperand2 = f2->double_value();
							}
							else if(const float_expression *f2 = dynamic_cast<const float_expression *>(x6->getRHS()))
							{
								O.type=2;
								O.durOperand2 = f2->double_value();
							}

							else if(const func_term *f2 = dynamic_cast<const func_term *>(x6->getRHS()))
							{
								O.type=3; //f()*f()
								O.durationFunction2.head = f2->getFunction()->getName();
								for(parameter_symbol_list::const_iterator it
									= f2->getArgs()->begin(); it != f2->getArgs()->end(); ++it)
								{
									const parameter_symbol *arg = *it;
									if( arg->type )
										O.durationFunction2.params.AddParam(arg->getName(), arg->type->getName());
									else
										O.durationFunction2.params.AddParam(arg->getName(), NOTYPE);
								}								

							}
						}
						O.durOperator = '/';

					}
					else
					{
						O.type=4;
						x4 = (func_term *)x2->getRHS();
						O.durationFunction.head = x4->getFunction()->getName();
						for(parameter_symbol_list::const_iterator it
							= x4->getArgs()->begin(); it != x4->getArgs()->end(); ++it)
						{
							const parameter_symbol *arg = *it;
							if( arg->type )
								O.durationFunction.params.AddParam(arg->getName(), arg->type->getName());
							else
								O.durationFunction.params.AddParam(arg->getName(), NOTYPE);
						}
					}
				}
				// get parameters:
				for(var_symbol_list::const_iterator it
					= da->parameters->begin(); it != da->parameters->end(); ++it)
				{
					const var_symbol *param = *it;
					if( param->type )
						O.params.AddParam(param->getName(), param->type->getName());
					else
						O.params.AddParam(param->getName(), NOTYPE);
				}
				// get conditions:
				// if just one condition
				if(dynamic_cast<timed_goal *>(da->precondition) != NULL)
				{
					//cout << endl <<"NEGETIVE PRECONDITIONS OMITED" << endl;
					const timed_goal *tg = (timed_goal *)da->precondition;
					simple_goal *sg = (simple_goal *)tg->getGoal();
					if (sg->getProp()==NULL)
					{
						cout << endl <<"NEGETIVE PRECONDITIONS OMITED" << endl;
						continue;
					}
					proposition *p = (proposition *)sg->getProp();

					//cout << endl <<"NEGETIVE PRECONDITIONS OMITED   1   " << p->head->getName() << endl;
					PPredicate P;
					P.head = p->head->getName();
					for(parameter_symbol_list::const_iterator it
						= p->args->begin(); it != p->args->end(); ++it)
					{
						const parameter_symbol *arg = *it;
						bool isConst = NULL == ( dynamic_cast<const var_symbol *>(arg) );
						if( arg->type )
							P.params.AddParam(arg->getName(), arg->type->getName(), isConst);
						else
							P.params.AddParam(arg->getName(), NOTYPE, isConst);
					}
					switch(tg->getTime())
					{
					case E_AT_START:
						O.conditionAtStart.AddPredicate(P);
						break;
					case E_AT_END:
						O.conditionAtEnd.AddPredicate(P);
						break;
					case E_OVER_ALL:
						O.conditionOverAll.AddPredicate(P);
						break;
					}
				}
				// if list of conditions ("conj_goal")
				else if(dynamic_cast<conj_goal *>(da->precondition) != NULL)
				{

					const goal_list *conditions = ((conj_goal *)da->precondition)->getGoals();
					for(goal_list::const_iterator it
						= conditions->begin(); it != conditions->end(); ++it)
					{
						//cout << endl <<"NEGETIVE PRECONDITIONS OMITED 0" << endl;
						const goal *g = *it;
						//cout << endl <<"NEGETIVE PRECONDITIONS OMITED 1" << endl;
						// do the same before else (if just one condition) here
						timed_goal *tg = (timed_goal *)g;
						//cout << endl <<"NEGETIVE PRECONDITIONS OMITED 2" << endl;
						simple_goal *sg = (simple_goal *)tg->getGoal();
						//cout << endl <<"NEGETIVE PRECONDITIONS OMITED 3     " << (sg->getProp()==NULL)  << endl;
						if (sg->getProp()==NULL)
						{
							cout << endl <<"NEGETIVE PRECONDITIONS OMITED" << endl;
							continue;
						}
						proposition *p = (proposition *)sg->getProp(); // ??use?? sg->getPolarity()
						//cout << endl <<"NEGETIVE PRECONDITIONS OMITED 4     " << (sg->getPolarity()==E_NEG) << p->head->getName() << endl;

						PPredicate P;
						
						//cout << endl <<"NEGETIVE PRECONDITIONS OMITED   2   " << p->head->getName() << endl;
						P.head = p->head->getName();
						for(parameter_symbol_list::const_iterator it
							= p->args->begin(); it != p->args->end(); ++it)
						{
							const parameter_symbol *arg = *it;
							bool isConst = NULL == ( dynamic_cast<const var_symbol *>(arg) );
							if( arg->type )
								P.params.AddParam(arg->getName(), arg->type->getName(), isConst);
							else
								P.params.AddParam(arg->getName(), NOTYPE, isConst);
						}
						switch(tg->getTime())
						{
						case E_AT_START:
							O.conditionAtStart.AddPredicate(P);
							break;
						case E_AT_END:
							O.conditionAtEnd.AddPredicate(P);
							break;
						case E_OVER_ALL:
							O.conditionOverAll.AddPredicate(P);
							break;
						}
					}
				}
				// get effects:
				for(pc_list<timed_effect *>::const_iterator it
					= da->effects->timed_effects.begin(); it != da->effects->timed_effects.end(); ++it)
				{
					const timed_effect *te = *it;
					for(pc_list<simple_effect *>::const_iterator it
						= te->effs->add_effects.begin(); it != te->effs->add_effects.end(); ++it)
					{
						const simple_effect *se = *it;
						proposition *p = se->prop;
						PPredicate P;
						P.head = p->head->getName();
						for(parameter_symbol_list::const_iterator it
							= p->args->begin(); it != p->args->end(); ++it)
						{
							const parameter_symbol *arg = *it;
							bool isConst = NULL == ( dynamic_cast<const var_symbol *>(arg) );
							if( arg->type )
								P.params.AddParam(arg->getName(), arg->type->getName(), isConst);
							else
								P.params.AddParam(arg->getName(), NOTYPE, isConst);
						}
						switch(te->ts)
						{
						case E_AT_START:
							O.addEffectAtStart.AddPredicate(P);
							break;
						case E_AT_END:
							O.addEffectAtEnd.AddPredicate(P);
							break;
						}
					}
					for(pc_list<simple_effect *>::const_iterator it
						= te->effs->del_effects.begin(); it != te->effs->del_effects.end(); ++it)
					{
						const simple_effect *se = *it;
						proposition *p = se->prop;
						PPredicate P;
						P.head = p->head->getName();
						for(parameter_symbol_list::const_iterator it
							= p->args->begin(); it != p->args->end(); ++it)
						{
							const parameter_symbol *arg = *it;
							bool isConst = NULL == ( dynamic_cast<const var_symbol *>(arg) );
							if( arg->type )
								P.params.AddParam(arg->getName(), arg->type->getName(), isConst);
							else
								P.params.AddParam(arg->getName(), NOTYPE, isConst);
						}
						switch(te->ts)
						{
						case E_AT_START:
							O.delEffectAtStart.AddPredicate(P);
							break;
						case E_AT_END:
							O.delEffectAtEnd.AddPredicate(P);
							break;
						}
					}
				}
			}
			else
			{
				// req :strips
				action *sa = (action *)o;
				O.name = sa->name->getName();
				// get duration:
				O.duration = 0;
				// get parameters:
				for(var_symbol_list::const_iterator it
					= sa->parameters->begin(); it != sa->parameters->end(); ++it)
				{
					const var_symbol *param = *it;
					if( param->type )
						O.params.AddParam(param->getName(), param->type->getName());
					else
						O.params.AddParam(param->getName(), NOTYPE);
				}
				// get conditions:
				if(dynamic_cast<conj_goal *>(sa->precondition) == NULL) // if just one condition
				{
					simple_goal *sg = (simple_goal *)sa->precondition;
					proposition *p = (proposition *)sg->getProp();
					PPredicate P;
					P.head = p->head->getName();
					for(parameter_symbol_list::const_iterator it
						= p->args->begin(); it != p->args->end(); ++it)
					{
						const parameter_symbol *arg = *it;
						bool isConst = NULL == ( dynamic_cast<const var_symbol *>(arg) );
						if( arg->type )
							P.params.AddParam(arg->getName(), arg->type->getName(), isConst);
						else
							P.params.AddParam(arg->getName(), NOTYPE, isConst);
					}
					O.conditionAtStart.AddPredicate(P);
				}
				else // if list of conditions ("conj_goal")
				{
					const goal_list *conditions = ((conj_goal *)sa->precondition)->getGoals();
					for(goal_list::const_iterator it
						= conditions->begin(); it != conditions->end(); ++it)
					{
						const goal *g = *it;
						// do the same before else (if just one condition) here
						const proposition *p = ((simple_goal *)g)->getProp(); // ??use?? sg->getPolarity()
						PPredicate P;
						P.head = p->head->getName();
						for(parameter_symbol_list::const_iterator it
							= p->args->begin(); it != p->args->end(); ++it)
						{
							const parameter_symbol *arg = *it;
							bool isConst = NULL == ( dynamic_cast<const var_symbol *>(arg) );
							if( arg->type )
								P.params.AddParam(arg->getName(), arg->type->getName(), isConst);
							else
								P.params.AddParam(arg->getName(), NOTYPE, isConst);
						}
						O.conditionAtStart.AddPredicate(P);
					}
				}
				// get effects:
				for(pc_list<simple_effect *>::const_iterator it
					= sa->effects->add_effects.begin(); it != sa->effects->add_effects.end(); ++it)
				{
					const simple_effect *se = *it;
					proposition *p = se->prop;
					PPredicate P;
					P.head = p->head->getName();
					for(parameter_symbol_list::const_iterator it
						= p->args->begin(); it != p->args->end(); ++it)
					{
						const parameter_symbol *arg = *it;
						bool isConst = NULL == ( dynamic_cast<const var_symbol *>(arg) );
						if( arg->type )
							P.params.AddParam(arg->getName(), arg->type->getName(), isConst);
						else
							P.params.AddParam(arg->getName(), NOTYPE, isConst);
					}
					O.addEffectAtEnd.AddPredicate(P);
				}
				for(pc_list<simple_effect *>::const_iterator it
					= sa->effects->del_effects.begin(); it != sa->effects->del_effects.end(); ++it)
				{
					const simple_effect *se = *it;
					proposition *p = se->prop;
					PPredicate P;
					P.head = p->head->getName();
					for(parameter_symbol_list::const_iterator it
						= p->args->begin(); it != p->args->end(); ++it)
					{
						const parameter_symbol *arg = *it;
						bool isConst = NULL == ( dynamic_cast<const var_symbol *>(arg) );
						if( arg->type )
							P.params.AddParam(arg->getName(), arg->type->getName(), isConst);
						else
							P.params.AddParam(arg->getName(), NOTYPE, isConst);
					}
					O.delEffectAtEnd.AddPredicate(P);
				}
			}
			pDomain.operators.AddOperator(O);
		}
		// Set readonly attribute of predicates whom do not appear
		//     in any operator's effects.
		// Then move the readonly predicates from OVERALL sections to ATSTART
		for(int i=0; i<pDomain.predicates.size(); i++)
		{
			bool readonly = true;
			string head = pDomain.predicates.all[i].head;
			for(vector<POperator>::const_iterator it = pDomain.operators.all.begin();
				it != pDomain.operators.all.end(); ++it)
			{
				const POperator& op = *it;
				if(op.DoesAddAtStart(head) || op.DoesDeleteAtStart(head)
					|| op.DoesAddAtEnd(head) || op.DoesDeleteAtEnd(head))
				{
					readonly = false;
					break;
				}
			}
			pDomain.predicates.all[i].readonly = readonly;
			if(!readonly)
				continue;
			// now move the readonly predicates from OVERALL section to ATSTART
			int sz = pDomain.operators.all.size();
			for(int j=0; j<sz; j++) // for each operator
			{
				POperator *op = &pDomain.operators.all[j];
				int sz2 = op->conditionOverAll.size();
				for(int k=0; k<sz2; k++) // for each overall condition
				{
					if(head == op->conditionOverAll.all[k].head)
					{
						op->conditionAtStart.AddPredicate(op->conditionOverAll.all[k]);
						op->conditionOverAll.all.erase( k + op->conditionOverAll.all.begin());
						sz2 --; k--; // k will be incremented on the next loop, so k will remain unchanged
					}
				}
			}
		}
		return true;
	}

#pragma region Domain Printers TA

void PListPredicate::PrintTA(void) const
{
	for(vector<PPredicate>::const_iterator it = all.begin();
		it != all.end(); ++it)
	{
		const PPredicate& p = *it;
		string nameUC = p.head;
		std::transform(nameUC.begin(), nameUC.end(), nameUC.begin(), ::toupper);
		cout << nameUC << endl;
		for( int i=0; i<p.params.size(); i++ )
		{
			cout << p.params.paramNames[i] << endl;
		}
	}
}

void PListOperator::PrintTA(void) const
{
	for(vector<POperator>::const_iterator it = all.begin();
		it != all.end(); ++it)
	{
		const POperator &o = *it;
		string nameUC = o.name;
		std::transform(nameUC.begin(), nameUC.end(), nameUC.begin(), ::toupper);
		cout << nameUC << endl;
		cout << "Parameters:" << o.params.size() << endl;
		for( int i=0; i<o.params.size(); i++ )
		{
			cout << o.params.paramNames[i] << endl;
		}
		cout << "Preconditions:" << o.conditionAtStart.size() << endl;
		o.conditionAtStart.PrintTA();
		cout << "Add-Effects:" << o.addEffectAtEnd.size() << endl;
		o.addEffectAtEnd.PrintTA();
		cout << "Delete-Effects:" << o.delEffectAtEnd.size() << endl;
		o.delEffectAtEnd.PrintTA();
		cout << endl;
	}
}

void PDomain::PrintTA(void) const
{
	cout << "PREDICATES:" << predicates.size() << endl;
	//cout << "Predicate name : Number of parameters" << endl;
	//cout << "Follows by name and type of each parameter" << endl;
	//cout << "------------------------------------------" << endl;
	for(vector<PPredicate>::const_iterator it = predicates.all.begin();
		it != predicates.all.end(); ++it)
	{
		const PPredicate& p = *it;
		cout << p.head << ":" << p.params.size() << endl;
	}

	cout << endl << endl << "OPERATORS:" << operators.size() << endl;
	//cout << "Syntax for each Operator:" << endl;
	//cout << "Operator name" << endl;
	//cout << "Parameters:count" << endl;
	//cout << "  Follows by name and type of each parameter" << endl;
	//cout << "Preconditions:count" << endl;
	//cout << "  Follows by list of Preconditions" << endl;
	//cout << "Add-Effects:count" << endl;
	//cout << "  Follows by list of Add-Effects" << endl;
	//cout << "Delete-Effects:count" << endl;
	//cout << "  Follows by list of Delete-Effects" << endl;
	//cout << "Ends with a blank line" << endl;
	//cout << "------------------------------------------------" << endl;
	operators.PrintTA();
}

#pragma endregion

#pragma region Problem Printers TA

void PProblem::PrintTA(void)
{
	int sz, sz2;
	sz = pObjects.size();
	cout << "OBJECTS:" << sz << endl;
	for( int i=0; i < sz; i++ )
		cout << pObjects.objects[i] << endl;
	
	sz = initialState.size();
	cout << endl << endl << "INITIAL-STATE:" << sz << endl;
	for( int i=0; i < sz; i++ )
	{
		const PProposition *p = pAllProposition.GetPropositionById(initialState[i]);
		sz2 = p->objects.size();
		string nameUC = p->prd->head;
		std::transform(nameUC.begin(), nameUC.end(), nameUC.begin(), ::toupper);
		cout << nameUC << endl;
		for ( int j=0; j < sz2; j++ )
			cout << pObjects.objects[p->objects[j]] << endl;
	}

	sz = goals.size();
	cout << endl << endl << "GOALS:" << sz << endl;
	for( int i=0; i < sz; i++ )
	{
		const PProposition *p = pAllProposition.GetPropositionById(goals[i]);
		sz2 = p->objects.size();
		string nameUC = p->prd->head;
		std::transform(nameUC.begin(), nameUC.end(), nameUC.begin(), ::toupper);
		cout << nameUC << endl;
		for ( int j=0; j < sz2; j++ )
			cout << pObjects.objects[p->objects[j]] << endl;
	}
}

#pragma endregion

#pragma region Problem Printers

void PObjects::Print(void) const
{
	int sObjects = objects.size();
	cout << "id\tname\ttype(s)" << endl;
	cout << "--\t----\t-----------------" << endl;
	for( int i=0; i < sObjects; i++ )
	{
		cout << i << "\t" << objects[i];
		const vector<string> &t = types[i];
		for(vector<string>::const_iterator it = t.begin(); it != t.end() ; ++it)
		{
			cout << "\t" << *it << endl;
		}
	}
}

void PListProposition::Print(void) const
{
	int i;
	vector<PProposition>::const_iterator it;
	for(it = all.begin(), i=0; it != all.end(); ++it, ++i)
	{
		cout << i << "\t" << it->ToFullString() << endl;
	}
}

void PListAction::Print(void) const
{
	cout << "Id\tDur\tFull Name" << endl;
	cout << "--\t---\t---------" << endl;
	set<int>::const_iterator it2;
	for(vector<PAction>::const_iterator it = all.begin();
		it != all.end(); ++it)
	{
		const PAction &a = *it;
		cout << a.id << "\t" << a.duration << "\t" << a.ToFullString() << endl;

		if(!a.addEffectAtStart.empty())
		{
			cout << "\t\t  Add Effect AtStart:";
			for(it2 = a.addEffectAtStart.begin(); it2 != a.addEffectAtStart.end(); ++it2)
			{
				cout << "\t" << pProblem.pAllProposition.GetPropositionById(*it2)->ToFullString();
			}
			cout << endl;
		}
		if(!a.delEffectAtStart.empty())
		{
			cout << "\t\t  Del Effect AtStart:";
			for(it2 = a.delEffectAtStart.begin(); it2 != a.delEffectAtStart.end(); ++it2)
			{
				cout << "\t" << pProblem.pAllProposition.GetPropositionById(*it2)->ToFullString();
			}
			cout << endl;
		}
		if(!a.addEffectAtEnd.empty())
		{
			cout << "\t\t  Add Effect AtEnd:";
			for(it2 = a.addEffectAtEnd.begin(); it2 != a.addEffectAtEnd.end(); ++it2)
			{
				cout << "\t" << pProblem.pAllProposition.GetPropositionById(*it2)->ToFullString();
			}
			cout << endl;
		}
		if(!a.delEffectAtEnd.empty())
		{
			cout << "\t\t  Del Effect AtEnd:";
			for(it2 = a.delEffectAtEnd.begin(); it2 != a.delEffectAtEnd.end(); ++it2)
			{
				cout << "\t" << pProblem.pAllProposition.GetPropositionById(*it2)->ToFullString();
			}
			cout << endl;
		}
		if(!a.conditionAtStart.empty())
		{
			cout << "\t\t  Condition AtStart:";
			for(it2 = a.conditionAtStart.begin(); it2 != a.conditionAtStart.end(); ++it2)
			{
				cout << "\t" << pProblem.pAllProposition.GetPropositionById(*it2)->ToFullString();
			}
			cout << endl;
		}
		if(!a.conditionAtEnd.empty())
		{
			cout << "\t\t  Condition AtEnd:";
			for(it2 = a.conditionAtEnd.begin(); it2 != a.conditionAtEnd.end(); ++it2)
			{
				cout << "\t" << pProblem.pAllProposition.GetPropositionById(*it2)->ToFullString();
			}
			cout << endl;
		}
		if(!a.conditionOverAll.empty())
		{
			cout << "\t\t  Condition OverAll:";
			for(it2 = a.conditionOverAll.begin(); it2 != a.conditionOverAll.end(); ++it2)
			{
				cout << "\t" << pProblem.pAllProposition.GetPropositionById(*it2)->ToFullString();
			}
			cout << endl;
		}
	}
}

void PProblem::Print(void) const
{
	int i;
	vector<int>::const_iterator it;
	cout << "INITIAL STATE:" << endl;
	for(it = initialState.begin(), i=0; it != initialState.end(); ++it, ++i)
	{
		cout << i << "\t" << pProblem.pAllProposition.GetPropositionById(*it)->ToFullString() << endl;
	}
	cout << endl;

	cout << endl << endl << "GOALS:" << endl;
	for(it = goals.begin(), i=0; it != goals.end(); ++it, ++i)
	{
		cout << i << "\t" << pProblem.pAllProposition.GetPropositionById(*it)->ToFullString() << endl;
	}
	cout << endl;

	cout << endl << endl << "ACTIONS" << endl << "-------" << endl;
	pAllAction.Print();

	cout << endl << endl << "PROPOSITIONS" << endl << "------------" << endl;
	pAllProposition.Print();

	cout << endl << endl << "OBJECTS" << endl << "-------" << endl;
	pObjects.Print();
}

#pragma endregion

#pragma region PROBLEM

#pragma region PProblem-methods

float PProblem::GetFunctionValue(string f) const
{
	map<string, float>::const_iterator it;
	it = functionValues.find(f);
	if(it == functionValues.end())
		return -1;
	else
		return it->second;
}

#pragma endregion

#pragma region PAction-methods	
string PAction::ToFullString(void) const
{
	int j;
	char s[256];
	j = sprintf(s, "%s", op->name.c_str());
	for(vector<int>::const_iterator it = objects.begin();
		it != objects.end(); ++it)
	{
		j += sprintf(s+j, " %s", pProblem.pObjects.objects[*it].c_str());
	}
	return s;
}

string PAction::ToString(void) const
{
	int j;
	char s[256];
	j = sprintf(s, "%s", op->name.c_str());
	for(vector<int>::const_iterator it = objects.begin();
		it != objects.end(); ++it)
	{
		j += sprintf(s+j, " %d", *it);
	}
	return s;
}

bool PAction::DoesNeedAtStart(int prop) const
{
	return conditionAtStart.find(prop) != conditionAtStart.end();
}
bool PAction::DoesNeedAtEnd(int prop) const
{
	return conditionAtEnd.find(prop) != conditionAtEnd.end();
}
bool PAction::DoesNeedOverall(int prop) const
{
	return conditionOverAll.find(prop) != conditionOverAll.end();
}
bool PAction::DoesAddAtStart(int prop) const
{
	return addEffectAtStart.find(prop) != addEffectAtStart.end();
}
bool PAction::DoesDeleteAtStart(int prop) const
{
	return delEffectAtStart.find(prop) != delEffectAtStart.end();
}
bool PAction::DoesAddAtEnd(int prop) const
{
	return addEffectAtEnd.find(prop) != addEffectAtEnd.end();
}
bool PAction::DoesDeleteAtEnd(int prop) const
{
	return delEffectAtEnd.find(prop) != delEffectAtEnd.end();
}

int PAction::GroundAction(void)
{
	if(duration == -1)
	{
		string durationFunction = GroundOperatorDuration(op->durationFunction, op->params, objects);
		float d = pProblem.GetFunctionValue(durationFunction);
		string durationFunction2 = GroundOperatorDuration(op->durationFunction2, op->params, objects);
		float d2 = pProblem.GetFunctionValue(durationFunction2);
		//if(d == -1)
			//return false;
		if (op->type==0)
		{
			d = op->durOperand;
			d2 = op->durOperand2;
		}
		if (op->type==1)
		{
			d = op->durOperand;
		}
		if (op->type==2)
		{
			d2= op->durOperand2;
		}
		if (op->type<4)
		{
		// FIXME: remove domain specific manual fixes:
		if(op->durOperator == '*')
			d = d * d2;
		else if(op->durOperator == '/')
			d = d / d2;
		}
		//if(pDomain.name == "satellite")
			//d *= 100;
		//else if(pDomain.name == "pipesworld_strips")
			//d *= 12;
		duration = d;
	}
	else
	{
		//if(pDomain.name == "satellite")
			//duration *= 100;
		//else if(pDomain.name == "pipesworld_strips")
			//duration *= 12;
	}

	PProposition prp;
	// note: in the following statements, &prd is sent to GroundOperatorPredicate()
	//       in which a mistake is occured: the address of prd is saved in prp,
	//       while the prd is a temporary variable,
	//       But we dont push the prp in action, so who cares?! (we just use its id.)
	// note: although prd has prd->readonly, but it can't be used,
	//       since when an operator is created, the readonly attribute
	//       of its preconditions and effects is not set.
	//       (so we use pDomain.predicates.FindPredicate(prd.head)->readonly instead)
	conditionAtStart.clear();
	for(vector<PPredicate>::const_iterator it = op->conditionAtStart.all.begin();
		it != op->conditionAtStart.all.end(); ++it)
	{
		const PPredicate &prd = *it;
		GroundOperatorPredicate(prd, prp, op->params, objects);
		if(pDomain.predicates.FindPredicate(prd.head)->readonly
			&& !IsInList(pProblem.initialState, prp.id))
		{
			string thelastpar;
			int thelastnumber=1000;
			for(vector<string>::const_iterator it11 = prd.params.paramNames.begin();
				it11 != prd.params.paramNames.end(); ++it11)
			{
				thelastpar=*it11;
			int thisnumber=0;
			for(vector<string>::const_iterator it22 = op->params.paramNames.begin();
				(it22 != op->params.paramNames.end()) && (*it22 != thelastpar); ++it22)
				thisnumber++;
			if (thisnumber<thelastnumber)
				thelastnumber=thisnumber;
			}

			//vector<string>::const_iterator it11 = prd.params.paramNames.begin();
			//vector<string>::const_iterator it22 = op->params.paramNames.begin();
			//if((*it11)==(*it22))
			return thelastnumber;
		}
		if(prp.id == -1)
			throw runtime_error(("Proposition not found: " + prp.ToString()).c_str());
		conditionAtStart.insert( prp.id );
	}
	conditionAtEnd.clear();
	for(vector<PPredicate>::const_iterator it = op->conditionAtEnd.all.begin();
		it != op->conditionAtEnd.all.end(); ++it)
	{
		const PPredicate &prd = *it;
		GroundOperatorPredicate(prd, prp, op->params, objects);
		if(pDomain.predicates.FindPredicate(prd.head)->readonly
			&& !IsInList(pProblem.initialState, prp.id))
		{
			string thelastpar;
			int thelastnumber=1000;
			for(vector<string>::const_iterator it11 = prd.params.paramNames.begin();
				it11 != prd.params.paramNames.end(); ++it11)
			{
				thelastpar=*it11;
			int thisnumber=0;
			for(vector<string>::const_iterator it22 = op->params.paramNames.begin();
				(it22 != op->params.paramNames.end()) && (*it22 != thelastpar); ++it22)
				thisnumber++;
			if (thisnumber<thelastnumber)
				thelastnumber=thisnumber;
			}

			//vector<string>::const_iterator it11 = prd.params.paramNames.begin();
			//vector<string>::const_iterator it22 = op->params.paramNames.begin();
			//if((*it11)==(*it22))
			return thelastnumber;
		}
		if(prp.id == -1)
			throw runtime_error(("Proposition not found: " + prp.ToString()).c_str());
		conditionAtEnd.insert( prp.id );
	}
	conditionOverAll.clear();
	for(vector<PPredicate>::const_iterator it = op->conditionOverAll.all.begin();
		it != op->conditionOverAll.all.end(); ++it)
	{
		const PPredicate &prd = *it;
		GroundOperatorPredicate(prd, prp, op->params, objects);
		if(prp.id == -1)
			throw runtime_error(("Proposition not found: " + prp.ToString()).c_str());
		// no need to check, since we moved readonly predicates
		//    from overall to atstart in each operator:
		//    (there will be no readonly predicate in overall conditions)
		/*if(pDomain.predicates.FindPredicate(prd.head)->readonly
			&& !IsInList(pProblem.initialState, prp.id))
			return false;*/
		conditionOverAll.insert( prp.id );
	}
	addEffectAtStart.clear();
	for(vector<PPredicate>::const_iterator it = op->addEffectAtStart.all.begin();
		it != op->addEffectAtStart.all.end(); ++it)
	{
		const PPredicate &prd = *it;
		GroundOperatorPredicate(prd, prp, op->params, objects);
		if(prp.id == -1)
			throw runtime_error(("Proposition not found: " + prp.ToString()).c_str());
		addEffectAtStart.insert( prp.id );
	}
	delEffectAtStart.clear();
	for(vector<PPredicate>::const_iterator it = op->delEffectAtStart.all.begin();
		it != op->delEffectAtStart.all.end(); ++it)
	{
		const PPredicate &prd = *it;
		GroundOperatorPredicate(prd, prp, op->params, objects);
		if(prp.id == -1)
			throw runtime_error(("Proposition not found: " + prp.ToString()).c_str());
		delEffectAtStart.insert( prp.id );
	}
	addEffectAtEnd.clear();
	for(vector<PPredicate>::const_iterator it = op->addEffectAtEnd.all.begin();
		it != op->addEffectAtEnd.all.end(); ++it)
	{
		const PPredicate &prd = *it;
		GroundOperatorPredicate(prd, prp, op->params, objects);
		if(prp.id == -1)
			throw runtime_error(("Proposition not found: " + prp.ToString()).c_str());
		addEffectAtEnd.insert( prp.id );
	}
	delEffectAtEnd.clear();
	for(vector<PPredicate>::const_iterator it = op->delEffectAtEnd.all.begin();
		it != op->delEffectAtEnd.all.end(); ++it)
	{
		const PPredicate &prd = *it;
		GroundOperatorPredicate(prd, prp, op->params, objects);
		if(prp.id == -1)
			throw runtime_error(("Proposition not found: " + prp.ToString()).c_str());
		delEffectAtEnd.insert( prp.id );
	}
	set<int>::iterator it11, it22;
	for( it11 = delEffectAtEnd.begin(); it11 != delEffectAtEnd.end(); ++it11)
		for( it22 = addEffectAtEnd.begin(); it22 != addEffectAtEnd.end(); ++it22)
			if (*it11==*it22)
				return -1;
	for( it11 = delEffectAtStart.begin(); it11 != delEffectAtStart.end(); ++it11)
		for( it22 = addEffectAtStart.begin(); it22 != addEffectAtStart.end(); ++it22)
			if (*it11==*it22)
				return -1;

	return -2;
}
#pragma endregion

#pragma region PListAction-methods
bool PListAction::AddAction(PAction a)
{
	a.id = all.size(); // id starts from 0
	all.push_back(a);
	return true; // we do not check whether a proposition already exists.
}

// FIXME: speed up using map<string, int>
// although it is not used in the project
int PListAction::FindAction(string name, const vector<int> &objects) const
{
	int sAll = all.size();
	int sObjects = objects.size();
	for( int i=0; i<sAll; i++ )
	{
		const PAction *a2 = (const PAction *)&all[i];
		if(name == a2->op->name)
			if(sObjects == a2->n_objects)
			{
				bool eq = true;
				for( int j=0; j<sObjects; j++)
					if(objects[j] != a2->objects[j])
					{
						eq=false;
						break;
					}
					if(eq)
						return i;
			}
	}
	return -1;
}

const PAction* PListAction::GetActionById(int id) const
{
	return &all[id];
}

#pragma endregion

#pragma region PProposition-methods

string PProposition::ToFullString(void) const
{
	int j;
	char s[256];
	j = sprintf(s, "%s", prd->head.c_str());
	for(vector<int>::const_iterator it = objects.begin();
		it != objects.end(); ++it)
	{
		j += sprintf(s+j, " %s", pProblem.pObjects.objects[*it].c_str());
	}
	return s;
}

string PProposition::ToString(void) const
{
	int j;
	char s[256];
	j = sprintf(s, "%s", prd->head.c_str());
	for(vector<int>::const_iterator it = objects.begin();
		it != objects.end(); ++it)
	{
		j += sprintf(s+j, " %d", *it);
	}
//	string s2 = s;
	return s;
}

#pragma endregion

#pragma region PListProposition
bool PListProposition::AddProposition(PProposition &p)
{
	p.id = all.size(); // id starts from 0
	all.push_back(p);
	tofind[p.ToString()] = p.id;
	return true; // we do not check whether a proposition alread exists.
}

int PListProposition::FindProposition(PProposition &p) const
{
	map<string, int>::const_iterator it;
	it = tofind.find(p.ToString());
	if(it == tofind.end())
		return -1;
	else
		return it->second;
}

PProposition* PListProposition::GetPropositionById(const int id)
{
	return &all[id];
}

#pragma endregion

#pragma region PObjects-methods

bool PObjects::AddObject(string o, vector<string> &t)
{
	for(vector<string>::const_iterator it = t.begin(); it != t.end() ; ++it)
	{
		if(!pDomain.pTypes.IsValidType(*it)) // invalid type
			return false;
	}
	tofind[o] = objects.size();
	objects.push_back(o);
	types.push_back(t);
	return true;
}

bool PObjects::RemoveLastObject(void)
{
	if(objects.empty())
		return false;
	objects.pop_back();
	types.pop_back();
	tofind.erase(--tofind.rbegin().base());
	return true;
}

int PObjects::FindObject(string o) const
{
	map<string, int>::const_iterator it;
	it = tofind.find(o);
	if(it == tofind.end())
		return -1;
	else
		return it->second;
}

#pragma endregion

#pragma endregion

#pragma region DOMAIN

#pragma region POperator-methods
bool POperator::DoesNeedAtStart(string head) const
{
	return NULL != conditionAtStart.FindPredicate(head);
}
bool POperator::DoesAddAtStart(string head) const
{
	return NULL != addEffectAtStart.FindPredicate(head);
}
bool POperator::DoesDeleteAtStart(string head) const
{
	return NULL != delEffectAtStart.FindPredicate(head);
}
bool POperator::DoesNeedAtEnd(string head) const
{
	return NULL != conditionAtEnd.FindPredicate(head);
}
bool POperator::DoesAddAtEnd(string head) const
{
	return NULL != addEffectAtEnd.FindPredicate(head);
}
bool POperator::DoesDeleteAtEnd(string head) const
{
	return NULL != delEffectAtEnd.FindPredicate(head);
}
#pragma endregion

#pragma region PTypes-methods

bool PTypes::AddType(string t)
{
	if(IsValidType(t))
		return false;
	types.push_back(t);
	return true;
}

bool PTypes::IsValidType(string t) const
{
	for(vector<string>::const_iterator it = types.begin(); it != types.end() ; ++it)
	{
		if(t==*it)
			return true;
	}
	return false;
}

#pragma endregion

#pragma region PParams-methods

bool PParams::AddParam(string p, string t, bool isConst)
{
	if(!pDomain.pTypes.IsValidType(t)) // invalid type
		return false;
	paramNames.push_back(p);
	paramTypes.push_back(t);
	//paramTypes2.push_back(t2);
	paramIsConst.push_back(isConst);
	return true;
}

#pragma endregion

#pragma region PListPredicate-methods

bool PListPredicate::AddPredicate(PPredicate p)
{
	tofind[p.head] = size();
	all.push_back(p);
	return true;
}

const PPredicate* PListPredicate::FindPredicate(string head) const
{
	map<string, int>::const_iterator it;
	it = tofind.find(head);
	if(it == tofind.end())
		return NULL;
	else
		return &all[it->second];
}

#pragma endregion

#pragma region PListOperator-methods

bool PListOperator::AddOperator(POperator o)
{
	tofind[o.name] = size();
	all.push_back(o);
	return true;
}

const POperator* PListOperator::FindOperator(string name) const
{
	map<string, int>::const_iterator it;
	it = tofind.find(name);
	if(it == tofind.end())
		return NULL;
	else
		return &all[it->second];
}

#pragma endregion

#pragma endregion

}
