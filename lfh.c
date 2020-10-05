/*-------------------------------------------------------------------------
 *
 * LFH.c
 *	  learn from history.
 *
 *
 * IDENTIFICATION
 *	  src\backend\optimizer\plan\LFH.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"
#include "optimizer/lfh.h"
#include "nodes/bitmapset.h"
#include <stdlib.h>
#include "utils/hashutils.h"

#define COMPARE_SCALAR_FIELD(fldname) do { if (a->fldname != b->fldname) return false;} while (0)
#define foreach_myList(cell, l)	for ((cell) = mylist_head(l); (cell) != NULL; (cell) = lnext(cell))

myList* CheckList = ((myList*)NULL);
static myRelInfo** myRelInfoArray;

static inline myListCell* mylist_head(const myList * l)
{
	return l ? l->head : NULL;
}
bool _equalmyList(myList *a, myList *b) {
	if (a == b)
		return true;
	if (a == NULL || b == NULL)
		return false;
	COMPARE_SCALAR_FIELD(length);
	int cnt = 0;
	myListCell *lc1, *lc2;
	foreach_myList(lc1, a) {
		foreach_myList(lc2, b) {
			if (my_equal(lc1->data, lc2->data)) {
				cnt++;
				break;
			}
		}
	}
	if (cnt == a->length)
		return true;
	return false;
}
bool _equalmyRelInfo(myRelInfo* a, myRelInfo* b) {
	COMPARE_SCALAR_FIELD(relid);
	if(_equalmyList(a->RestrictClauseList, b->RestrictClauseList) 
		&& _equalmyList(a->childRelList, b->childRelList) 
		&& _equalmyList(a->joininfo, b->joininfo))
		return true;
	return false;
}
bool _equalRestrictClause(RestrictClause* a, RestrictClause* b) {
	COMPARE_SCALAR_FIELD(rc_type);
	if(my_equal(a->left, b->left) && my_equal(a->right, b->right))
		return true;
	return false;
}
bool _equalHistory(History* a, History* b) {
	COMPARE_SCALAR_FIELD(selec);
	if(_equalmyRelInfo(a->content, b->content))
		return true;
	return false;
}
bool _equalmyVar(myVar* a, myVar* b) {
	COMPARE_SCALAR_FIELD(relid);
	COMPARE_SCALAR_FIELD(varattno);
	COMPARE_SCALAR_FIELD(vartype);
	COMPARE_SCALAR_FIELD(vartypmod);
	COMPARE_SCALAR_FIELD(varcollid);
	COMPARE_SCALAR_FIELD(varlevelsup);
	COMPARE_SCALAR_FIELD(varnoold);
	COMPARE_SCALAR_FIELD(varoattno);
	return true;
}
bool _equalmyConst(myConst* a, myConst* b) {
	COMPARE_SCALAR_FIELD(consttype);
	COMPARE_SCALAR_FIELD(consttypmod);
	COMPARE_SCALAR_FIELD(constcollid);
	COMPARE_SCALAR_FIELD(constlen);
	COMPARE_SCALAR_FIELD(constvalue);
	COMPARE_SCALAR_FIELD(constisnull);
	COMPARE_SCALAR_FIELD(constbyval);
	return true;
}
bool my_equal(void* a, void* b) {
	bool retval = false;
	if (a == b)
		return true;
	if (a == NULL || b == NULL)
		return false;
	if (nodeTag(a) != nodeTag(b))
		return false;
	switch (nodeTag(a))
	{
		case T_myList:
			retval = _equalmyList(a, b);
			break;
		case T_myVar:
			retval = _equalmyVar(a, b);
			break;
		case T_myConst:
			retval = _equalmyConst(a, b);
			break;
		case T_RestrictClause:
			retval = _equalRestrictClause(a, b);
			break;
		case T_myRelInfo:
			retval = _equalmyRelInfo(a, b);
			break;
		case T_History:
			retval = _equalHistory(a, b);
			break;		
	}
	return retval;
}
void* my_copyList(PlannerInfo* root, const Var* from) {
	if (from == NULL)
		return NULL;
	Node* temp = (Node*)malloc(sizeof(myVar));
	temp->type = T_myVar;
	myVar* newnode = (myVar*)temp;
	newnode->relid = root->simple_rte_array[((Var*)from)->varno]->relid;
	newnode->varattno = ((Var*)from)->varattno;
	newnode->vartype = ((Var*)from)->vartype;
	newnode->vartypmod = ((Var*)from)->vartypmod;
	newnode->varcollid = ((Var*)from)->varcollid;
	newnode->varlevelsup = ((Var*)from)->varlevelsup;
	newnode->varnoold = ((Var*)from)->varnoold;
	newnode->varoattno = ((Var*)from)->varoattno;
	newnode->location = ((Var*)from)->location;
	return newnode;
}
void* my_copyVar(PlannerInfo* root, const Var *from) {
	if (from == NULL)
		return NULL;
	Node* temp = (Node*)malloc(sizeof(myVar));
	temp->type = T_myVar;
	myVar *newnode = (myVar*)temp;
	newnode->relid = root->simple_rte_array[((Var*)from)->varno]->relid;
	newnode->varattno = ((Var*)from)->varattno;
	newnode->vartype = ((Var*)from)->vartype;
	newnode->vartypmod = ((Var*)from)->vartypmod;
	newnode->varcollid = ((Var*)from)->varcollid;
	newnode->varlevelsup = ((Var*)from)->varlevelsup;
	newnode->varnoold = ((Var*)from)->varnoold;
	newnode->varoattno = ((Var*)from)->varoattno;
	newnode->location = ((Var*)from)->location;
	return newnode;
}
void* my_copyConst(PlannerInfo* root, const Const* from) {
	if (from == NULL)
		return NULL;
	Node* temp = (Node*)malloc(sizeof(myConst));
	temp->type = T_myConst;
	myConst* newnode = (myConst*)temp;
	newnode->consttype = ((Const*)from)->consttype;
	newnode->consttypmod = ((Const*)from)->consttypmod;
	newnode->constcollid = ((Const*)from)->constcollid;
	newnode->constlen = ((Const*)from)->constlen;
	if (newnode->constlen != -1) {
		newnode->constvalue = ((Const*)from)->constvalue;
	}
	else {
		Pointer ptr = ((Pointer)((Const*)from)->constvalue);
		Pointer ptr1 = VARDATA_ANY(ptr);
		int n = VARSIZE_ANY_EXHDR(ptr);
		newnode->constvalue = hash_any(ptr1, n);
	}
	newnode->constisnull = ((Const*)from)->constisnull;
	newnode->constbyval = ((Const*)from)->constbyval;
	newnode->location = ((Const*)from)->location;
	return newnode;
}
void* my_copyRelInfo(PlannerInfo* root, const myRelInfo* from) {
	if (from == NULL)
		return NULL;
	Node* temp = (Node*)malloc(sizeof(myRelInfo));
	temp->type = T_myRelInfo;
	myRelInfo* newnode = (myRelInfo*)temp;
	newnode->relid = ((myRelInfo*)from)->relid;
	newnode->RestrictClauseList = ((myRelInfo*)from)->RestrictClauseList;
	newnode->childRelList = ((myRelInfo*)from)->childRelList;
	newnode->joininfo = ((myRelInfo*)from)->joininfo;
	return newnode;
}
myList* ListRenew(myList* l, void* p) {
	if (l == (myList*)NULL) {
		myListCell* new_head;
		new_head = (myListCell*)malloc(sizeof(ListCell));
		new_head->next = NULL;
		l = (myList*)malloc(sizeof(myList));
		l->type = T_myList;
		l->length = 1;
		l->head = new_head;
		l->tail = new_head;
	}
	else {
		myListCell* new_tail;
		new_tail = (myListCell*)malloc(sizeof(ListCell));
		new_tail->next = NULL;
		l->tail->next = new_tail;
		l->tail = new_tail;
		l->length++;
	}
	l->tail->data = p;
	return l;
}
void* getExpr(PlannerInfo* root, Node* expr) {
	void* ans = NULL;
	switch (expr->type) {
		case T_Var: {
			ans = my_copyVar(root, (const Var*)expr);
			break;
		}
		case T_Const: {
			ans = my_copyConst(root, (const Const*)expr);
			break;
		}
		case T_RelabelType: {
			ans = getExpr(root, (Node*)((RelabelType*)expr)->arg);
			break;
		}
	}
	return (Expr*)ans;
}
void* getLeftandRight(PlannerInfo* root, RestrictClause* rc, Expr* clause) {
	switch (nodeTag(clause)) {
		case T_OpExpr: {
			OpExpr* temp = (OpExpr*)clause;
			rc->left = getExpr(root, (Node*)(temp->args->head->data.ptr_value));
			rc->right = getExpr(root, (Node*)(temp->args->head->next->data.ptr_value));
		}
	}
	return;
}
/*
*  Unpack the input list info to get the restrict or join information.
*/
void* getRestrictClause(PlannerInfo* root, List* info) {
	myList* ans = (myList*)NULL;
	ListCell* lc;
	foreach(lc, info) {
			Expr* clause = (((RestrictInfo*)lfirst(lc))->clause);
			RestrictClause* rc = (RestrictClause*)malloc(sizeof(RestrictClause));
			rc->type = T_RestrictClause;
			rc->rc_type = clause->type;
			getLeftandRight(root, rc, clause);
			ans = ListRenew(ans, rc);
	}
	return ans;
}
void* getChildList(PlannerInfo* root, RelOptInfo* rel) {
	myList* ans = (myList*)NULL;
	if (rel->relid == 0) {
		Bitmapset* temp_bms;
		temp_bms = bms_copy(rel->relids);
		int x = 0;
		while ((x = bms_first_member(temp_bms)) > 0) {
			myRelInfo* temp = myRelInfoArray[x];
			ans = ListRenew(ans, temp);
		}
	}
	return ans;
}
void* CreateNewRel(PlannerInfo* root, RelOptInfo* rel, List* joininfo) {
	myRelInfo* ans = (myRelInfo*)malloc(sizeof(myRelInfo));
	ans->type = T_myRelInfo;
	/* 
	*  If rel is a base relation, joininfo is useless.
	*/
	if (rel->relid != 0) {
		ans->relid = root->simple_rte_array[rel->relid]->relid;
		ans->childRelList = (myList*)NULL;
		ans->RestrictClauseList = getRestrictClause(root, rel->baserestrictinfo);
		ans->joininfo = (myList*)NULL;
	}
	/*
	*  If rel is a temporary relation, joininfo is useful.
	*/
	else {
		ans->relid = 0;
		ans->childRelList = getChildList(root, rel);
		ans->RestrictClauseList = (myList*)NULL;
		ans->joininfo = getRestrictClause(root, joininfo);
	}
	return ans;
}

void* BecomeHistory(myRelInfo* rel) {
	History* temp = (History*)malloc(sizeof(History));
	temp->type = T_History;
	temp->content = rel;
	temp->selec = -1.0;
	return temp;
}

bool LookupHistory(myRelInfo* rel, Selectivity* selec) {
	myListCell* lc;
	foreach_myList(lc, CheckList) {
		History* one_page = ((History*)lc->data);
		if (my_equal(one_page->content, rel)) {
			*selec = one_page->selec;
			return true;
		}
	}
	return false;
}
void initial_myRelInfoArray(PlannerInfo* root) {
	int n;
	n = root->simple_rel_array_size + 1;
	myRelInfoArray = (myRelInfo**)palloc0(n * sizeof(myRelInfo*));
}
bool learn_from_history(PlannerInfo* root, RelOptInfo* joinrel, RelOptInfo* outer_rel, RelOptInfo* inner_rel, List* joininfo, Selectivity* selec) {
	/* If a RelOptInfo's relid unequal to 0, meaning it's a base relation. */
	if ((outer_rel->relid != 0) && (myRelInfoArray[outer_rel->relid] == NULL)) {
		myRelInfo* rel = CreateNewRel(root, outer_rel, joininfo);
		myRelInfoArray[outer_rel->relid] = rel;
	}
	if (inner_rel->relid != 0) {
		myRelInfo* rel = CreateNewRel(root, inner_rel, joininfo);
		myRelInfoArray[inner_rel->relid] = rel;
	}
	myRelInfo* rel = CreateNewRel(root, joinrel, joininfo);
	if (!LookupHistory(rel, selec)) {
		History* one_page = (History*)BecomeHistory(rel);
		CheckList = ListRenew(CheckList, one_page);
		return false;
	}
	return true;
}
bool isCorrRel(QueryDesc* queryDesc, myRelInfo* rel, int* t_array) {
	int cnt = 0;
	int i = 0;
	while (t_array[i] != 0) {
		myListCell* lc;
		foreach_myList(lc, rel->childRelList) {
			if (((myRelInfo*)lc->data)->relid == t_array[i]) {
				cnt++;
				break;
			}
		}
		i++;
	}
	if (cnt == i) {
		return true;
	}
	return false;
}
void FindComponent(QueryDesc* queryDesc, Plan* plan, int* t_array) {
	switch (plan->type) {
		case T_Scan: {
			Scan* scanplan = (Scan*)plan;
			int i = 0;
			while (t_array[i] != 0) {
				i++;
			}
			t_array[i] = queryDesc->estate ->es_range_table_array[scanplan->scanrelid - 1]->relid;
			break;
		}
		case T_SeqScan: {
			SeqScan* scanplan = (SeqScan*)plan;
			int i = 0;
			while (t_array[i] != 0) {
				i++;
			}
			t_array[i] = queryDesc->estate->es_range_table_array[scanplan->scanrelid - 1]->relid;
			break;
		}
		case T_SampleScan: {
			SampleScan* scanplan = (SampleScan*)plan;
			int i = 0;
			while (t_array[i] != 0) {
				i++;
			}
			break;
		}
		case T_IndexScan: {
			IndexScan* scanplan = (IndexScan*)plan;
			int i = 0;
			while (t_array[i] != 0) {
				i++;
			}
			break;
		}
		case T_IndexOnlyScan: {
			IndexOnlyScan* scanplan = (IndexOnlyScan*)plan;
			int i = 0;
			while (t_array[i] != 0) {
				i++;
			}
			break;
		}
		case T_BitmapIndexScan: {
			BitmapIndexScan* scanplan = (BitmapIndexScan*)plan;
			int i = 0;
			while (t_array[i] != 0) {
				i++;
			}
			break;
		}
		case T_BitmapHeapScan: {
			BitmapHeapScan* scanplan = (BitmapHeapScan*)plan;
			int i = 0;
			while (t_array[i] != 0) {
				i++;
			}
			break;
		}
		case T_TidScan: {
			TidScan* scanplan = (TidScan*)plan;
			int i = 0;
			while (t_array[i] != 0) {
				i++;
			}
			break;
		}
		case T_SubqueryScan: {
			SubqueryScan* scanplan = (SubqueryScan*)plan;
			int i = 0;
			while (t_array[i] != 0) {
				i++;
			}
			break;
		}
		case T_FunctionScan: {
			FunctionScan* scanplan = (FunctionScan*)plan;
			int i = 0;
			while (t_array[i] != 0) {
				i++;
			}
			break;
		}
		case T_ValuesScan: {
			ValuesScan* scanplan = (ValuesScan*)plan;
			int i = 0;
			while (t_array[i] != 0) {
				i++;
			}
			break;
		}
		case T_TableFuncScan: {
			TableFuncScan* scanplan = (TableFuncScan*)plan;
			int i = 0;
			while (t_array[i] != 0) {
				i++;
			}
			break;
		}
		case T_CteScan: {
			CteScan* scanplan = (CteScan*)plan;
			int i = 0;
			while (t_array[i] != 0);
			break;
		}
		case T_NamedTuplestoreScan: {
			NamedTuplestoreScan* scanplan = (NamedTuplestoreScan*)plan;
			int i = 0;
			while (t_array[i] != 0) {
				i++;
			}
			break;
		}
		case T_WorkTableScan: {
			WorkTableScan* scanplan = (WorkTableScan*)plan;
			int i = 0;
			while (t_array[i] != 0) {
				i++;
			}
			break;
		}
		case T_ForeignScan: {
			ForeignScan* scanplan = (ForeignScan*)plan;
			int i = 0;
			while (t_array[i] != 0) {
				i++;
			}
			break;
		}
		case T_CustomScan: {
			CustomScan* scanplan = (CustomScan*)plan;
			int i = 0;
			while (t_array[i] != 0) {
				i++;
			}
			break;
		}
		case T_Join: {
			if (plan->lefttree)
				FindComponent(queryDesc, plan->lefttree, t_array);
			if (plan->righttree)
				FindComponent(queryDesc, plan->righttree, t_array);
			break;
		}
		case T_NestLoop: {
			if (plan->lefttree)
				FindComponent(queryDesc, plan->lefttree, t_array);
			if (plan->righttree)
				FindComponent(queryDesc, plan->righttree, t_array);
			break;
		}
		case T_MergeJoin: {
			if (plan->lefttree)
				FindComponent(queryDesc, plan->lefttree, t_array);
			if (plan->righttree)
				FindComponent(queryDesc, plan->righttree, t_array);
			break;
		}
		case T_HashJoin: {
			if (plan->lefttree)
				FindComponent(queryDesc, plan->lefttree, t_array);
			if (plan->righttree)
				FindComponent(queryDesc, plan->righttree, t_array);
			break;
		}
		case T_Material: {
			if(plan->lefttree)
				FindComponent(queryDesc, plan->lefttree, t_array);
			if(plan->righttree)
				FindComponent(queryDesc, plan->righttree, t_array);
			break;
		}
		case T_Sort: {
			if (plan->lefttree)
				FindComponent(queryDesc, plan->lefttree, t_array);
			if (plan->righttree)
				FindComponent(queryDesc, plan->righttree, t_array);
			break;
		}
		case T_Group: {
			if (plan->lefttree)
				FindComponent(queryDesc, plan->lefttree, t_array);
			if (plan->righttree)
				FindComponent(queryDesc, plan->righttree, t_array);
			break;
		}
		case T_Agg: {
			if (plan->lefttree)
				FindComponent(queryDesc, plan->lefttree, t_array);
			if (plan->righttree)
				FindComponent(queryDesc, plan->righttree, t_array);
			break;
		}
		case T_WindowAgg: {
			if (plan->lefttree)
				FindComponent(queryDesc, plan->lefttree, t_array);
			if (plan->righttree)
				FindComponent(queryDesc, plan->righttree, t_array);
			break;
		}
		case T_Unique: {
			if (plan->lefttree)
				FindComponent(queryDesc, plan->lefttree, t_array);
			if (plan->righttree)
				FindComponent(queryDesc, plan->righttree, t_array);
			break;
		}
		case T_Gather: {
			if (plan->lefttree)
				FindComponent(queryDesc, plan->lefttree, t_array);
			if (plan->righttree)
				FindComponent(queryDesc, plan->righttree, t_array);
			break;
		}
		case T_GatherMerge: {
			if (plan->lefttree)
				FindComponent(queryDesc, plan->lefttree, t_array);
			if (plan->righttree)
				FindComponent(queryDesc, plan->righttree, t_array);
			break;
		}
		case T_Hash: {
			if (plan->lefttree)
				FindComponent(queryDesc, plan->lefttree, t_array);
			if (plan->righttree)
				FindComponent(queryDesc, plan->righttree, t_array);
			break;
		}
	}
}
History* FindCorHistory(const QueryDesc* queryDesc, const Plan* plan) {
	int n = queryDesc->estate->es_range_table_size + 1;
	unsigned int* t_array = malloc(n * sizeof(unsigned int));
	memset(t_array, 0, n * sizeof(unsigned int));
	FindComponent(queryDesc, plan, t_array);
	myListCell* lc;
	foreach_myList(lc, CheckList) {
		if (isCorrRel(queryDesc, ((History*)lc->data)->content, t_array)) {
			return ((History*)lc->data);
		}
	}
	return (History*)NULL;
}
int learnSelectivity(const QueryDesc* queryDesc, const PlanState* planstate) {
	Plan* plan = planstate->plan;
	if (plan->type == T_SeqScan)
		return plan->plan_rows;
	int left_tuple = -1, right_tuple = -1;
	History* his = (History*)NULL;
	if(planstate->lefttree)
		left_tuple = learnSelectivity(queryDesc, planstate->lefttree);
	if(planstate->righttree)
		right_tuple = learnSelectivity(queryDesc, planstate->righttree);
	if((left_tuple != -1) && (right_tuple != -1))
		his = FindCorHistory(queryDesc, plan);
	if(his)
		his->selec = planstate->instrument->tuplecount / left_tuple / right_tuple;
	return planstate->instrument->tuplecount;
}