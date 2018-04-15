/*
 * Copyright 2014-2018 Adrian Thurston <thurston@colm.net>
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

#include "ragel.h"
#include "flatvarloop.h"
#include "redfsm.h"
#include "gendata.h"
#include "parsedata.h"
#include "inputdata.h"

FlatVarLoop::FlatVarLoop( const CodeGenArgs &args )
:
	FlatVar( args )
{}


void FlatVarLoop::tableDataPass()
{
	if ( redFsm->anyActions() )
		taActions();

	taKeys();
	taCharClass();
	taFlatIndexOffset();

	taIndicies();
	taIndexDefaults();
	taTransCondSpaces();

	if ( red->condSpaceList.length() > 0 )
		taTransOffsets();

	taCondTargs();
	taCondActions();

	taToStateActions();
	taFromStateActions();
	taEofActions();
	taEofTrans();
	taNfaTargs();
	taNfaOffsets();
	taNfaPushActions();
	taNfaPopTrans();
}

void FlatVarLoop::genAnalysis()
{
	redFsm->sortByStateId();

	/* Choose default transitions and the single transition. */
	redFsm->chooseDefaultSpan();
		
	/* Do flat expand. */
	redFsm->makeFlatClass();

	/* If any errors have occured in the input file then don't write anything. */
	if ( red->id->errorCount > 0 )
		return;

	/* Anlayze Machine will find the final action reference counts, among other
	 * things. We will use these in reporting the usage of fsm directives in
	 * action code. */
	red->analyzeMachine();

	setKeyType();

	/* Run the analysis pass over the table data. */
	setTableState( TableArray::AnalyzePass );
	tableDataPass();

	/* Switch the tables over to the code gen mode. */
	setTableState( TableArray::GeneratePass );
}


void FlatVarLoop::COND_ACTION( RedCondPair *cond )
{
	int act = 0;
	if ( cond->action != 0 )
		act = cond->action->location+1;
	condActions.value( act );
}

void FlatVarLoop::TO_STATE_ACTION( RedStateAp *state )
{
	int act = 0;
	if ( state->toStateAction != 0 )
		act = state->toStateAction->location+1;
	toStateActions.value( act );
}

void FlatVarLoop::FROM_STATE_ACTION( RedStateAp *state )
{
	int act = 0;
	if ( state->fromStateAction != 0 )
		act = state->fromStateAction->location+1;
	fromStateActions.value( act );
}

void FlatVarLoop::EOF_ACTION( RedStateAp *state )
{
	int act = 0;
	if ( state->eofAction != 0 )
		act = state->eofAction->location+1;
	eofActions.value( act );
}

std::ostream &FlatVarLoop::TO_STATE_ACTION_SWITCH()
{
	/* Walk the list of functions, printing the cases. */
	for ( GenActionList::Iter act = red->actionList; act.lte(); act++ ) {
		/* Write out referenced actions. */
		if ( act->numToStateRefs > 0 ) {
			/* Write the case label, the action and the case break. */
			out << "\t" << CASE( STR( act->actionId ) ) << " {\n";
			ACTION( out, act, IlOpts( 0, false, false ) );
			out << "\n\t" << CEND() << "}\n";
		}
	}

	return out;
}

std::ostream &FlatVarLoop::FROM_STATE_ACTION_SWITCH()
{
	/* Walk the list of functions, printing the cases. */
	for ( GenActionList::Iter act = red->actionList; act.lte(); act++ ) {
		/* Write out referenced actions. */
		if ( act->numFromStateRefs > 0 ) {
			/* Write the case label, the action and the case break. */
			out << "\t" << CASE( STR( act->actionId ) ) << " {\n";
			ACTION( out, act, IlOpts( 0, false, false ) );
			out << "\n\t" << CEND() << "}\n";
		}
	}

	return out;
}

std::ostream &FlatVarLoop::EOF_ACTION_SWITCH()
{
	/* Walk the list of functions, printing the cases. */
	for ( GenActionList::Iter act = red->actionList; act.lte(); act++ ) {
		/* Write out referenced actions. */
		if ( act->numEofRefs > 0 ) {
			/* Write the case label, the action and the case break. */
			out << "\t" << CASE( STR( act->actionId ) ) << " {\n";
			ACTION( out, act, IlOpts( 0, true, false ) );
			out << "\n\t" << CEND() << "}\n";
		}
	}

	return out;
}


std::ostream &FlatVarLoop::ACTION_SWITCH()
{
	/* Walk the list of functions, printing the cases. */
	for ( GenActionList::Iter act = red->actionList; act.lte(); act++ ) {
		/* Write out referenced actions. */
		if ( act->numTransRefs > 0 ) {
			/* Write the case label, the action and the case break. */
			out << "\t" << CASE( STR( act->actionId ) ) << " {\n";
			ACTION( out, act, IlOpts( 0, false, false ) );
			out << "\n\t" << CEND() << "}\n";
		}
	}

	return out;
}


void FlatVarLoop::writeData()
{
	/* If there are any transtion functions then output the array. If there
	 * are none, don't bother emitting an empty array that won't be used. */
	if ( redFsm->anyActions() )
		taActions();

	taKeys();
	taCharClass();
	taFlatIndexOffset();

	taIndicies();
	taIndexDefaults();
	taTransCondSpaces();
	if ( red->condSpaceList.length() > 0 )
		taTransOffsets();
	taCondTargs();
	taCondActions();

	if ( redFsm->anyToStateActions() )
		taToStateActions();

	if ( redFsm->anyFromStateActions() )
		taFromStateActions();

	if ( redFsm->anyEofActions() )
		taEofActions();

	if ( redFsm->anyEofTrans() )
		taEofTrans();

	taNfaTargs();
	taNfaOffsets();
	taNfaPushActions();
	taNfaPopTrans();

	STATE_IDS();
}

void FlatVarLoop::NFA_PUSH_ACTION( RedNfaTarg *targ )
{
	int act = 0;
	if ( targ->push != 0 )
		act = targ->push->actListId+1;
	nfaPushActions.value( act );
}

void FlatVarLoop::NFA_POP_TEST( RedNfaTarg *targ )
{
	int act = 0;
	if ( targ->popTest != 0 )
		act = targ->popTest->actListId+1;
	nfaPopTrans.value( act );
}

void FlatVarLoop::NFA_FROM_STATE_ACTION_EXEC()
{
	if ( redFsm->anyFromStateActions() ) {
		out <<
			"	_acts = " << OFFSET( ARR_REF( actions ), ARR_REF( fromStateActions ) + "[nfa_bp[nfa_len].state]" ) << ";\n"
			"	_nacts = " << CAST( UINT() ) << DEREF( ARR_REF( actions ), "_acts" ) << ";\n"
			"	_acts += 1;\n"
			"	while ( _nacts > 0 ) {\n"
			"		switch ( " << DEREF( ARR_REF( actions ), "_acts" ) << " ) {\n";
			FROM_STATE_ACTION_SWITCH() <<
			"		}\n"
			"		_nacts -= 1;\n"
			"		_acts += 1;\n"
			"	}\n"
			"\n";
	}
}

void FlatVarLoop::writeExec()
{
	testEofUsed = false;
	outLabelUsed = false;
	matchCondLabelUsed = false;

	if ( redFsm->anyNfaStates() ) {
		out <<
			"{\n"
			"	" << UINT() << " _nfa_cont = 1;\n"
			"	" << UINT() << " _nfa_repeat = 1;\n"
			"	while ( _nfa_cont != 0 )\n";
	}

	out <<
		"	{\n"
		;

	if ( redFsm->anyRegCurStateRef() )
		out << "	int _ps;\n";

	out <<
		"	" << UINT() << " _trans = 0;\n" <<
		"	" << UINT() << " _have = 0;\n"
		"	" << UINT() << " _cont = 1;\n"
	;

	if ( red->condSpaceList.length() > 0 ) {
		out <<
			"	" << UINT() << " _cond = 0;\n";
	}

	if ( redFsm->anyToStateActions() || redFsm->anyRegActions() 
			|| redFsm->anyFromStateActions() )
	{
		out << 
			"	" << INDEX( ARR_TYPE( actions ), "_acts" ) << ";\n"
			"	" << UINT() << " _nacts;\n";
	}

	if ( redFsm->classMap != 0 ) {
		out <<
			"	" << INDEX( ALPH_TYPE(), "_keys" ) << ";\n"
			"	" << INDEX( ARR_TYPE( indicies ), "_inds" ) << ";\n";
	}

#if 0
	out <<
		"	index " << ALPH_TYPE() << " _keys;\n"
		"	index " << ARR_TYPE( condKeys ) << " _ckeys;\n"
#endif 

	if ( red->condSpaceList.length() > 0 )
		out << "	int _cpc;\n";

	out <<
		"	while ( _cont == 1 ) {\n"
		"\n";

	if ( redFsm->errState != 0 ) {
		outLabelUsed = true;
		out << 
			"	if ( " << vCS() << " == " << redFsm->errState->id << " )\n"
			"		_cont = 0;\n";
	}

	out << 
//		"label _resume {\n"
		"_have = 0;\n";

	if ( !noEnd ) {
		out << 
			"	if ( " << P() << " == " << PE() << " ) {\n";

		if ( redFsm->anyEofTrans() || redFsm->anyEofActions() ) {
			out << 
				"	if ( " << P() << " == " << vEOF() << " )\n"
				"	{\n";

			if ( redFsm->anyEofActions() ) {
				out <<
					"	" << INDEX( ARR_TYPE( actions ), "__acts" ) << ";\n"
					"	" << UINT() << " __nacts;\n"
					"	__acts = " << OFFSET( ARR_REF( actions ), 
							ARR_REF( eofActions ) + "[" + vCS() + "]" ) << ";\n"
					"	__nacts = " << CAST( UINT() ) << DEREF( ARR_REF( actions ), "__acts" ) << ";\n"
					"	__acts += 1;\n"
					"	while ( __nacts > 0 ) {\n"
					"		switch ( " << DEREF( ARR_REF( actions ), "__acts" ) << " ) {\n";
					EOF_ACTION_SWITCH() <<
					"		}\n"
					"		__nacts -= 1;\n"
					"		__acts += 1;\n"
					"	}\n";
			}

			out << "if ( _have == 0 ) {\n";

			if ( redFsm->anyEofTrans() ) {
				out <<
					"	if ( " << ARR_REF( eofTrans ) << "[" << vCS() << "] > 0 ) {\n"
					"		_trans = " << CAST(UINT()) << ARR_REF( eofTrans ) << "[" << vCS() << "] - 1;\n";

				if ( red->condSpaceList.length() > 0 ) {
					out <<
						"		_cond = " << CAST( UINT() ) << ARR_REF( transOffsets ) << "[_trans];\n";
				}

				out <<
					"		_have = 1;\n"
					"	}\n";
					matchCondLabelUsed = true;
			}

			out << "}\n";
			
			out << 
				"	}\n"
				"\n";
		}

		out << 
			"	if ( _have == 0 )\n"
			"		_cont = 0;\n"
			"	}\n";

	}

	out << 
		"	if ( _cont == 1 ) {\n"
		"	if ( _have == 0 ) {\n";

	if ( redFsm->anyFromStateActions() ) {
		out <<
			"	_acts = " << OFFSET( ARR_REF( actions ),
					ARR_REF( fromStateActions ) + "[" + vCS() + "]" ) << ";\n"
			"	_nacts = " << CAST( UINT() ) << DEREF( ARR_REF( actions ), "_acts" ) << ";\n"
			"	_acts += 1;\n"
			"	while ( _nacts > 0 ) {\n"
			"		switch ( " << DEREF( ARR_REF( actions ), "_acts" ) << " ) {\n";
			FROM_STATE_ACTION_SWITCH() <<
			"		}\n"
			"		_nacts -= 1;\n"
			"		_acts += 1;\n"
			"	}\n"
			"\n";
	}

	NFA_PUSH();

	LOCATE_TRANS();

	string cond = "_cond";
	if ( red->condSpaceList.length() == 0 )
		cond = "_trans";

	out << "}\n";
	
	out << "if ( _cont == 1 ) {\n";

	if ( redFsm->anyRegCurStateRef() )
		out << "	_ps = " << vCS() << ";\n";

	out <<
		"	" << vCS() << " = " << CAST( "int" ) << ARR_REF( condTargs ) << "[" << cond << "];\n"
		"\n";

	if ( redFsm->anyRegActions() ) {
		out <<
			"	if ( " << ARR_REF( condActions ) << "[" << cond << "] != 0 ) {\n"
			"		_acts = " << OFFSET( ARR_REF( actions ), ARR_REF( condActions ) + "[" + cond + "]" ) << ";\n"
			"		_nacts = " << CAST( UINT() ) << DEREF( ARR_REF( actions ), "_acts" ) << ";\n"
			"		_acts += 1;\n"
			"		while ( _nacts > 0 )\n	{\n"
			"			switch ( " << DEREF( ARR_REF( actions ), "_acts" ) << " )\n"
			"			{\n";
			ACTION_SWITCH() <<
			"			}\n"
			"			_nacts -= 1;\n"
			"			_acts += 1;\n"
			"		}\n"
			"	}\n"
			"\n";
	}

//	if ( /*redFsm->anyRegActions() || */ redFsm->anyActionGotos() || 
//			redFsm->anyActionCalls() || redFsm->anyActionRets() )
//	{
//		out << "}\n";
//		out << "label _again {\n";
//	}

	if ( redFsm->anyToStateActions() ) {
		out <<
			"	_acts = " << OFFSET( ARR_REF( actions ),
					ARR_REF( toStateActions ) + "[" + vCS() + "]" ) << ";\n"
			"	_nacts = " << CAST( UINT() ) << DEREF( ARR_REF( actions ), "_acts" ) << ";\n"
			"	_acts += 1;\n"
			"	while ( _nacts > 0 ) {\n"
			"		switch ( " << DEREF( ARR_REF( actions ), "_acts" ) << " ) {\n";
			TO_STATE_ACTION_SWITCH() <<
			"		}\n"
			"		_nacts -= 1;\n"
			"		_acts += 1;\n"
			"	}\n"
			"\n";
	}

	if ( redFsm->errState != 0 ) {
		outLabelUsed = true;
		out << 
			"	if ( " << vCS() << " == " << redFsm->errState->id << " )\n"
			"		_cont = 0;\n";
	}

	out << 
		"	if ( _cont == 1 )\n"
		"		" << P() << " += 1;\n"
		"\n"
		/* cont if. */
		"}}\n";



	/* The loop. */
	out << "}\n";

	NFA_POP();

	out <<
		"	}\n";

	if ( redFsm->anyNfaStates() )
		out << "}\n";
}
