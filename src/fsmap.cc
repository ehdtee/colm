/*
 * Copyright 2006-2018 Adrian Thurston <thurston@colm.net>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <iostream>

#include <assert.h>

#include "fsmgraph.h"

using std::cerr;
using std::endl;

KeyOps *keyOps = 0;

/* Insert an action into an action table. */
void ActionTable::setAction( int ordering, Action *action )
{
	/* Multi-insert in case specific instances of an action appear in a
	 * transition more than once. */
	insertMulti( ordering, action );
}

/* Set all the action from another action table in this table. */
void ActionTable::setActions( const ActionTable &other )
{
	for ( ActionTable::Iter action = other; action.lte(); action++ )
		insertMulti( action->key, action->value );
}

void ActionTable::setActions( int *orderings, Action **actions, int nActs )
{
	for ( int a = 0; a < nActs; a++ )
		insertMulti( orderings[a], actions[a] );
}

bool ActionTable::hasAction( Action *action )
{
	for ( int a = 0; a < length(); a++ ) {
		if ( data[a].value == action )
			return true;
	}
	return false;
}

/* Insert an action into an action table. */
void LmActionTable::setAction( int ordering, TokenInstance *action )
{
	/* Multi-insert in case specific instances of an action appear in a
	 * transition more than once. */
	insertMulti( ordering, action );
}

/* Set all the action from another action table in this table. */
void LmActionTable::setActions( const LmActionTable &other )
{
	for ( LmActionTable::Iter action = other; action.lte(); action++ )
		insertMulti( action->key, action->value );
}

void ErrActionTable::setAction( int ordering, Action *action, int transferPoint )
{
	insertMulti( ErrActionTableEl( action, ordering, transferPoint ) );
}

void ErrActionTable::setActions( const ErrActionTable &other )
{
	for ( ErrActionTable::Iter act = other; act.lte(); act++ )
		insertMulti( ErrActionTableEl( act->action, act->ordering, act->transferPoint ) );
}

/* Insert a priority into this priority table. Looks out for priorities on
 * duplicate keys. */
void PriorTable::setPrior( int ordering, PriorDesc *desc )
{
	PriorEl *lastHit = 0;
	PriorEl *insed = insert( PriorEl(ordering, desc), &lastHit );
	if ( insed == 0 ) {
		/* This already has a priority on the same key as desc. Overwrite the
		 * priority if the ordering is larger (later in time). */
		if ( ordering >= lastHit->ordering )
			*lastHit = PriorEl( ordering, desc );
	}
}

/* Set all the priorities from a priorTable in this table. */
void PriorTable::setPriors( const PriorTable &other )
{
	/* Loop src priorities once to overwrite duplicates. */
	PriorTable::Iter priorIt = other;
	for ( ; priorIt.lte(); priorIt++ )
		setPrior( priorIt->ordering, priorIt->desc );
}

/* Set the priority of starting transitions. Isolates the start state so it has
 * no other entry points, then sets the priorities of all the transitions out
 * of the start state. If the start state is final, then the outPrior of the
 * start state is also set. The idea is that a machine that accepts the null
 * string can still specify the starting trans prior for when it accepts the
 * null word. */
void FsmGraph::startFsmPrior( int ordering, PriorDesc *prior )
{
	/* Make sure the start state has no other entry points. */
	isolateStartState();

	/* Walk all transitions out of the start state. */
	for ( TransList::Iter trans = startState->outList; trans.lte(); trans++ ) {
		if ( trans->toState != 0 )
			trans->priorTable.setPrior( ordering, prior );
	}
}

/* Set the priority of all transitions in a graph. Walks all transition lists
 * and all def transitions. */
void FsmGraph::allTransPrior( int ordering, PriorDesc *prior )
{
	/* Walk the list of all states. */
	for ( StateList::Iter state = stateList; state.lte(); state++ ) {
		/* Walk the out list of the state. */
		for ( TransList::Iter trans = state->outList; trans.lte(); trans++ ) {
			if ( trans->toState != 0 )
				trans->priorTable.setPrior( ordering, prior );
		}
	}
}

/* Set the priority of all transitions that go into a final state. Note that if
 * any entry states are final, we will not be setting the priority of any
 * transitions that may go into those states in the future. The graph does not
 * support pending in transitions in the same way pending out transitions are
 * supported. */
void FsmGraph::finishFsmPrior( int ordering, PriorDesc *prior )
{
	/* Walk all final states. */
	for ( StateSet::Iter state = finStateSet; state.lte(); state++ ) {
		/* Walk all in transitions of the final state. */
		for ( TransInList::Iter trans = (*state)->inList; trans.lte(); trans++ )
			trans->priorTable.setPrior( ordering, prior );
	}
}

/* Set the priority of any future out transitions that may be made going out of
 * this state machine. */
void FsmGraph::leaveFsmPrior( int ordering, PriorDesc *prior )
{
	/* Set priority in all final states. */
	for ( StateSet::Iter state = finStateSet; state.lte(); state++ )
		(*state)->outPriorTable.setPrior( ordering, prior );
}


/* Set actions to execute on starting transitions. Isolates the start state
 * so it has no other entry points, then adds to the transition functions
 * of all the transitions out of the start state. If the start state is final,
 * then the func is also added to the start state's out func list. The idea is
 * that a machine that accepts the null string can execute a start func when it
 * matches the null word, which can only be done when leaving the start/final
 * state. */
void FsmGraph::startFsmAction( int ordering, Action *action )
{
	/* Make sure the start state has no other entry points. */
	isolateStartState();

	/* Walk the start state's transitions, setting functions. */
	for ( TransList::Iter trans = startState->outList; trans.lte(); trans++ ) {
		if ( trans->toState != 0 )
			trans->actionTable.setAction( ordering, action );
	}
}

/* Set functions to execute on all transitions. Walks the out lists of all
 * states. */
void FsmGraph::allTransAction( int ordering, Action *action )
{
	/* Walk all states. */
	for ( StateList::Iter state = stateList; state.lte(); state++ ) {
		/* Walk the out list of the state. */
		for ( TransList::Iter trans = state->outList; trans.lte(); trans++ ) {
			if ( trans->toState != 0 )
				trans->actionTable.setAction( ordering, action );
		}
	}
}

/* Specify functions to execute upon entering final states. If the start state
 * is final we can't really specify a function to execute upon entering that
 * final state the first time. So function really means whenever entering a
 * final state from within the same fsm. */
void FsmGraph::finishFsmAction( int ordering, Action *action )
{
	/* Walk all final states. */
	for ( StateSet::Iter state = finStateSet; state.lte(); state++ ) {
		/* Walk the final state's in list. */
		for ( TransInList::Iter trans = (*state)->inList; trans.lte(); trans++ )
			trans->actionTable.setAction( ordering, action );
	}
}

/* Add functions to any future out transitions that may be made going out of
 * this state machine. */
void FsmGraph::leaveFsmAction( int ordering, Action *action )
{
	/* Insert the action in the outActionTable of all final states. */
	for ( StateSet::Iter state = finStateSet; state.lte(); state++ )
		(*state)->outActionTable.setAction( ordering, action );
}

/* Add functions to the longest match action table for constructing scanners. */
void FsmGraph::longMatchAction( int ordering, TokenInstance *lmPart )
{
	/* Walk all final states. */
	for ( StateSet::Iter state = finStateSet; state.lte(); state++ ) {
		/* Walk the final state's in list. */
		for ( TransInList::Iter trans = (*state)->inList; trans.lte(); trans++ )
			trans->lmActionTable.setAction( ordering, lmPart );
	}
}

void FsmGraph::fillGaps( FsmState *state )
{
	if ( state->outList.length() == 0 ) {
		/* Add the range on the lower and upper bound. */
		attachNewTrans( state, 0, keyOps->minKey, keyOps->maxKey );
	}
	else {
		TransList srcList;
		srcList.transfer( state->outList );

		/* Check for a gap at the beginning. */
		TransList::Iter trans = srcList, next;
		if ( keyOps->minKey < trans->lowKey ) {
			/* Make the high key and append. */
			Key highKey = trans->lowKey;
			highKey.decrement();

			attachNewTrans( state, 0, keyOps->minKey, highKey );
		}

		/* Write the transition. */
		next = trans.next();
		state->outList.append( trans );

		/* Keep the last high end. */
		Key lastHigh = trans->highKey;

		/* Loop each source range. */
		for ( trans = next; trans.lte(); trans = next ) {
			/* Make the next key following the last range. */
			Key nextKey = lastHigh;
			nextKey.increment();

			/* Check for a gap from last up to here. */
			if ( nextKey < trans->lowKey ) {
				/* Make the high end of the range that fills the gap. */
				Key highKey = trans->lowKey;
				highKey.decrement();

				attachNewTrans( state, 0, nextKey, highKey );
			}

			/* Reduce the transition. If it reduced to anything then add it. */
			next = trans.next();
			state->outList.append( trans );

			/* Keep the last high end. */
			lastHigh = trans->highKey;
		}

		/* Now check for a gap on the end to fill. */
		if ( lastHigh < keyOps->maxKey ) {
			/* Get a copy of the default. */
			lastHigh.increment();

			attachNewTrans( state, 0, lastHigh, keyOps->maxKey );
		}
	}
}

void FsmGraph::setErrorAction( FsmState *state, int ordering, Action *action )
{
	/* Fill any gaps in the out list with an error transition. */
	fillGaps( state );

	/* Set error transitions in the transitions that go to error. */
	for ( TransList::Iter trans = state->outList; trans.lte(); trans++ ) {
		if ( trans->toState == 0 )
			trans->actionTable.setAction( ordering, action );
	}
}

void FsmGraph::setErrorActions( FsmState *state, const ActionTable &other )
{
	/* Fill any gaps in the out list with an error transition. */
	fillGaps( state );

	/* Set error transitions in the transitions that go to error. */
	for ( TransList::Iter trans = state->outList; trans.lte(); trans++ ) {
		if ( trans->toState == 0 )
			trans->actionTable.setActions( other );
	}
}


/* Give a target state for error transitions. */
void FsmGraph::setErrorTarget( FsmState *state, FsmState *target, int *orderings, 
			Action **actions, int nActs )
{
	/* Fill any gaps in the out list with an error transition. */
	fillGaps( state );

	/* Set error target in the transitions that go to error. */
	for ( TransList::Iter trans = state->outList; trans.lte(); trans++ ) {
		if ( trans->toState == 0 ) {
			/* The trans goes to error, redirect it. */
			redirectErrorTrans( trans->fromState, target, trans );
			trans->actionTable.setActions( orderings, actions, nActs );
		}
	}
}

void FsmGraph::transferErrorActions( FsmState *state, int transferPoint )
{
	for ( int i = 0; i < state->errActionTable.length(); ) {
		ErrActionTableEl *act = state->errActionTable.data + i;
		if ( act->transferPoint == transferPoint ) {
			/* Transfer the error action and remove it. */
			setErrorAction( state, act->ordering, act->action );
			state->errActionTable.vremove( i );
		}
		else {
			/* Not transfering and deleting, skip over the item. */
			i += 1;
		}
	}
}

/* Set error actions in the start state. */
void FsmGraph::startErrorAction( int ordering, Action *action, int transferPoint )
{
	/* Make sure the start state has no other entry points. */
	isolateStartState();

	/* Add the actions. */
	startState->errActionTable.setAction( ordering, action, transferPoint );
}

/* Set error actions in all states where there is a transition out. */
void FsmGraph::allErrorAction( int ordering, Action *action, int transferPoint )
{
	/* Insert actions in the error action table of all states. */
	for ( StateList::Iter state = stateList; state.lte(); state++ )
		state->errActionTable.setAction( ordering, action, transferPoint );
}

/* Set error actions in final states. */
void FsmGraph::finalErrorAction( int ordering, Action *action, int transferPoint )
{
	/* Add the action to the error table of final states. */
	for ( StateSet::Iter state = finStateSet; state.lte(); state++ )
		(*state)->errActionTable.setAction( ordering, action, transferPoint );
}

void FsmGraph::notStartErrorAction( int ordering, Action *action, int transferPoint )
{
	for ( StateList::Iter state = stateList; state.lte(); state++ ) {
		if ( state != startState )
			state->errActionTable.setAction( ordering, action, transferPoint );
	}
}

void FsmGraph::notFinalErrorAction( int ordering, Action *action, int transferPoint )
{
	for ( StateList::Iter state = stateList; state.lte(); state++ ) {
		if ( ! state->isFinState() )
			state->errActionTable.setAction( ordering, action, transferPoint );
	}
}

/* Set error actions in the states that have transitions into a final state. */
void FsmGraph::middleErrorAction( int ordering, Action *action, int transferPoint )
{
	/* Isolate the start state in case it is reachable from in inside the
	 * machine, in which case we don't want it set. */
	for ( StateList::Iter state = stateList; state.lte(); state++ ) {
		if ( state != startState && ! state->isFinState() )
			state->errActionTable.setAction( ordering, action, transferPoint );
	}
}

/* Set EOF actions in the start state. */
void FsmGraph::startEOFAction( int ordering, Action *action )
{
	/* Make sure the start state has no other entry points. */
	isolateStartState();

	/* Add the actions. */
	startState->eofActionTable.setAction( ordering, action );
}

/* Set EOF actions in all states where there is a transition out. */
void FsmGraph::allEOFAction( int ordering, Action *action )
{
	/* Insert actions in the EOF action table of all states. */
	for ( StateList::Iter state = stateList; state.lte(); state++ )
		state->eofActionTable.setAction( ordering, action );
}

/* Set EOF actions in final states. */
void FsmGraph::finalEOFAction( int ordering, Action *action )
{
	/* Add the action to the error table of final states. */
	for ( StateSet::Iter state = finStateSet; state.lte(); state++ )
		(*state)->eofActionTable.setAction( ordering, action );
}

void FsmGraph::notStartEOFAction( int ordering, Action *action )
{
	for ( StateList::Iter state = stateList; state.lte(); state++ ) {
		if ( state != startState )
			state->eofActionTable.setAction( ordering, action );
	}
}

void FsmGraph::notFinalEOFAction( int ordering, Action *action )
{
	for ( StateList::Iter state = stateList; state.lte(); state++ ) {
		if ( ! state->isFinState() )
			state->eofActionTable.setAction( ordering, action );
	}
}

/* Set EOF actions in the states that have transitions into a final state. */
void FsmGraph::middleEOFAction( int ordering, Action *action )
{
	/* Set the actions in all states that are not the start state and not final. */
	for ( StateList::Iter state = stateList; state.lte(); state++ ) {
		if ( state != startState && ! state->isFinState() )
			state->eofActionTable.setAction( ordering, action );
	}
}

/*
 * Set To State Actions.
 */

/* Set to state actions in the start state. */
void FsmGraph::startToStateAction( int ordering, Action *action )
{
	/* Make sure the start state has no other entry points. */
	isolateStartState();
	startState->toStateActionTable.setAction( ordering, action );
}

/* Set to state actions in all states. */
void FsmGraph::allToStateAction( int ordering, Action *action )
{
	/* Insert the action on all states. */
	for ( StateList::Iter state = stateList; state.lte(); state++ )
		state->toStateActionTable.setAction( ordering, action );
}

/* Set to state actions in final states. */
void FsmGraph::finalToStateAction( int ordering, Action *action )
{
	/* Add the action to the error table of final states. */
	for ( StateSet::Iter state = finStateSet; state.lte(); state++ )
		(*state)->toStateActionTable.setAction( ordering, action );
}

void FsmGraph::notStartToStateAction( int ordering, Action *action )
{
	for ( StateList::Iter state = stateList; state.lte(); state++ ) {
		if ( state != startState )
			state->toStateActionTable.setAction( ordering, action );
	}
}

void FsmGraph::notFinalToStateAction( int ordering, Action *action )
{
	for ( StateList::Iter state = stateList; state.lte(); state++ ) {
		if ( ! state->isFinState() )
			state->toStateActionTable.setAction( ordering, action );
	}
}

/* Set to state actions in states that are not final and not the start state. */
void FsmGraph::middleToStateAction( int ordering, Action *action )
{
	/* Set the action in all states that are not the start state and not final. */
	for ( StateList::Iter state = stateList; state.lte(); state++ ) {
		if ( state != startState && ! state->isFinState() )
			state->toStateActionTable.setAction( ordering, action );
	}
}

/* 
 * Set From State Actions.
 */

void FsmGraph::startFromStateAction( int ordering, Action *action )
{
	/* Make sure the start state has no other entry points. */
	isolateStartState();
	startState->fromStateActionTable.setAction( ordering, action );
}

void FsmGraph::allFromStateAction( int ordering, Action *action )
{
	/* Insert the action on all states. */
	for ( StateList::Iter state = stateList; state.lte(); state++ )
		state->fromStateActionTable.setAction( ordering, action );
}

void FsmGraph::finalFromStateAction( int ordering, Action *action )
{
	/* Add the action to the error table of final states. */
	for ( StateSet::Iter state = finStateSet; state.lte(); state++ )
		(*state)->fromStateActionTable.setAction( ordering, action );
}

void FsmGraph::notStartFromStateAction( int ordering, Action *action )
{
	for ( StateList::Iter state = stateList; state.lte(); state++ ) {
		if ( state != startState )
			state->fromStateActionTable.setAction( ordering, action );
	}
}

void FsmGraph::notFinalFromStateAction( int ordering, Action *action )
{
	for ( StateList::Iter state = stateList; state.lte(); state++ ) {
		if ( ! state->isFinState() )
			state->fromStateActionTable.setAction( ordering, action );
	}
}

void FsmGraph::middleFromStateAction( int ordering, Action *action )
{
	/* Set the action in all states that are not the start state and not final. */
	for ( StateList::Iter state = stateList; state.lte(); state++ ) {
		if ( state != startState && ! state->isFinState() )
			state->fromStateActionTable.setAction( ordering, action );
	}
}

/* Shift the function ordering of the start transitions to start
 * at fromOrder and increase in units of 1. Useful before staring.
 * Returns the maximum number of order numbers used. */
int FsmGraph::shiftStartActionOrder( int fromOrder )
{
	int maxUsed = 0;

	/* Walk the start state's transitions, shifting function ordering. */
	for ( TransList::Iter trans = startState->outList; trans.lte(); trans++ ) {
		/* Walk the function data for the transition and set the keys to
		 * increasing values starting at fromOrder. */
		int curFromOrder = fromOrder;
		ActionTable::Iter action = trans->actionTable;
		for ( ; action.lte(); action++ ) 
			action->key = curFromOrder++;
	
		/* Keep track of the max number of orders used. */
		if ( curFromOrder - fromOrder > maxUsed )
			maxUsed = curFromOrder - fromOrder;
	}
	
	return maxUsed;
}

/* Remove all priorities. */
void FsmGraph::clearAllPriorities()
{
	for ( StateList::Iter state = stateList; state.lte(); state++ ) {
		/* Clear out priority data. */
		state->outPriorTable.empty();

		/* Clear transition data from the out transitions. */
		for ( TransList::Iter trans = state->outList; trans.lte(); trans++ )
			trans->priorTable.empty();
	}
}

/* Zeros out the function ordering keys. This may be called before minimization
 * when it is known that no more fsm operations are going to be done.  This
 * will achieve greater reduction as states will not be separated on the basis
 * of function ordering. */
void FsmGraph::nullActionKeys( )
{
	/* For each state... */
	for ( StateList::Iter state = stateList; state.lte(); state++ ) {
		/* Walk the transitions for the state. */
		for ( TransList::Iter trans = state->outList; trans.lte(); trans++ ) {
			/* Walk the action table for the transition. */
			for ( ActionTable::Iter action = trans->actionTable;
					action.lte(); action++ )
				action->key = 0;

			/* Walk the action table for the transition. */
			for ( LmActionTable::Iter action = trans->lmActionTable;
					action.lte(); action++ )
				action->key = 0;
		}

		/* Null the action keys of the to state action table. */
		for ( ActionTable::Iter action = state->toStateActionTable;
				action.lte(); action++ )
			action->key = 0;

		/* Null the action keys of the from state action table. */
		for ( ActionTable::Iter action = state->fromStateActionTable;
				action.lte(); action++ )
			action->key = 0;

		/* Null the action keys of the out transtions. */
		for ( ActionTable::Iter action = state->outActionTable;
				action.lte(); action++ )
			action->key = 0;

		/* Null the action keys of the error action table. */
		for ( ErrActionTable::Iter action = state->errActionTable;
				action.lte(); action++ )
			action->ordering = 0;

		/* Null the action keys eof action table. */
		for ( ActionTable::Iter action = state->eofActionTable;
				action.lte(); action++ )
			action->key = 0;
	}
}

/* Walk the list of states and verify that non final states do not have out
 * data, that all stateBits are cleared, and that there are no states with
 * zero foreign in transitions. */
void FsmGraph::verifyStates()
{
	for ( StateList::Iter state = stateList; state.lte(); state++ ) {
		/* Non final states should not have leaving data. */
		if ( ! (state->stateBits & SB_ISFINAL) ) {
			assert( state->outActionTable.length() == 0 );
			assert( state->outCondSet.length() == 0 );
			assert( state->outPriorTable.length() == 0 );
		}

		/* Data used in algorithms should be cleared. */
		assert( (state->stateBits & SB_BOTH) == 0 );
		assert( state->foreignInTrans > 0 );
	}
}

/* Compare two transitions according to their relative priority. Since the
 * base transition has no priority associated with it, the default is to
 * return equal. */
int FsmGraph::comparePrior( const PriorTable &priorTable1, const PriorTable &priorTable2 )
{
	/* Looking for differing priorities on same keys. Need to concurrently
	 * scan the priority lists. */
	PriorTable::Iter pd1 = priorTable1;
	PriorTable::Iter pd2 = priorTable2;
	while ( pd1.lte() && pd2.lte() ) {
		/* Check keys. */
		if ( pd1->desc->key < pd2->desc->key )
			pd1.increment();
		else if ( pd1->desc->key > pd2->desc->key )
			pd2.increment();
		/* Keys are the same, check priorities. */
		else if ( pd1->desc->priority < pd2->desc->priority )
			return -1;
		else if ( pd1->desc->priority > pd2->desc->priority )
			return 1;
		else {
			/* Keys and priorities are equal, advance both. */
			pd1.increment();
			pd2.increment();
		}
	}

	/* No differing priorities on the same key. */
	return 0;
}

/* Compares two transitions according to priority and functions. Pointers
 * should not be null. Does not consider to state or from state.  Compare two
 * transitions according to the data contained in the transitions.  Data means
 * any properties added to user transitions that may differentiate them. Since
 * the base transition has no data, the default is to return equal. */
int FsmGraph::compareTransData( FsmTrans *trans1, FsmTrans *trans2 )
{
	/* Compare the prior table. */
	int cmpRes = CmpPriorTable::compare( trans1->priorTable, 
			trans2->priorTable );
	if ( cmpRes != 0 )
		return cmpRes;

	/* Compare longest match action tables. */
	cmpRes = CmpLmActionTable::compare(trans1->lmActionTable, 
			trans2->lmActionTable);
	if ( cmpRes != 0 )
		return cmpRes;
	
	/* Compare action tables. */
	return CmpActionTable::compare(trans1->actionTable, 
			trans2->actionTable);
}

/* Callback invoked when another trans (or possibly this) is added into this
 * transition during the merging process.  Draw in any properties of srcTrans
 * into this transition. AddInTrans is called when a new transitions is made
 * that will be a duplicate of another transition or a combination of several
 * other transitions. AddInTrans will be called for each transition that the
 * new transition is to represent. */
void FsmGraph::addInTrans( FsmTrans *destTrans, FsmTrans *srcTrans )
{
	/* Protect against adding in from ourselves. */
	if ( srcTrans == destTrans ) {
		/* Adding in ourselves, need to make a copy of the source transitions.
		 * The priorities are not copied in as that would have no effect. */
		destTrans->lmActionTable.setActions( LmActionTable(srcTrans->lmActionTable) );
		destTrans->actionTable.setActions( ActionTable(srcTrans->actionTable) );
	}
	else {
		/* Not a copy of ourself, get the functions and priorities. */
		destTrans->lmActionTable.setActions( srcTrans->lmActionTable );
		destTrans->actionTable.setActions( srcTrans->actionTable );
		destTrans->priorTable.setPriors( srcTrans->priorTable );
	}
}

/* Compare the properties of states that are embedded by users. Compares out
 * priorities, out transitions, to, from, out, error and eof action tables. */
int FsmGraph::compareStateData( const FsmState *state1, const FsmState *state2 )
{
	/* Compare the out priority table. */
	int cmpRes = CmpPriorTable::
			compare( state1->outPriorTable, state2->outPriorTable );
	if ( cmpRes != 0 )
		return cmpRes;
	
	/* Test to state action tables. */
	cmpRes = CmpActionTable::compare( state1->toStateActionTable, 
			state2->toStateActionTable );
	if ( cmpRes != 0 )
		return cmpRes;

	/* Test from state action tables. */
	cmpRes = CmpActionTable::compare( state1->fromStateActionTable, 
			state2->fromStateActionTable );
	if ( cmpRes != 0 )
		return cmpRes;

	/* Test out action tables. */
	cmpRes = CmpActionTable::compare( state1->outActionTable, 
			state2->outActionTable );
	if ( cmpRes != 0 )
		return cmpRes;

	/* Test out condition sets. */
	cmpRes = CmpActionSet::compare( state1->outCondSet, 
			state2->outCondSet );
	if ( cmpRes != 0 )
		return cmpRes;

	/* Test out error action tables. */
	cmpRes = CmpErrActionTable::compare( state1->errActionTable, 
			state2->errActionTable );
	if ( cmpRes != 0 )
		return cmpRes;

	/* Test eof action tables. */
	return CmpActionTable::compare( state1->eofActionTable, 
			state2->eofActionTable );
}

/* Invoked when a state looses its final state status and the leaving
 * transition embedding data should be deleted. */
void FsmGraph::clearOutData( FsmState *state )
{
	/* Kill the out actions and priorities. */
	state->outActionTable.empty();
	state->outCondSet.empty();
	state->outPriorTable.empty();
}

bool FsmGraph::hasOutData( FsmState *state )
{
	return ( state->outActionTable.length() > 0 ||
			state->outCondSet.length() > 0 ||
			state->outPriorTable.length() > 0 );
}
