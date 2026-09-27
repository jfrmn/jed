#pragma once

#define X_TESTS(X)\
	X(Test_Testing_Checks)\
	X(Test_Testing_TestWithParameter, 123, "Hello Tests!")\
	X(Test_Testing_Skipping)\
	\
	X(Test_TextController_Movements)\
	X(Test_TextController_Commands)\
	\
	X(Test_ToolOutput_RunTestTool)\
	\
	X(Test_Hashtable_Insert_BasicInsertion)\
	X(Test_Hashtable_Insert_LookupReturnsCorrectValues)\
	X(Test_Hashtable_Upsert_UpdateExistingValue)\
	X(Test_Hashtable_Upsert_InsertsWhenNotExists)\
	X(Test_Hashtable_Reserve_CreatesInitialCapacity)\
	X(Test_Hashtable_Reserve_NoOpWhenAlreadyLargeEnough)\
	X(Test_Hashtable_GrowthDuringInsertion)\
	X(Test_Hashtable_CollisionTest_SameKeys_DifferentValues)\
	X(Test_Hashtable_CollisionTest_DifferentKeys_SameHash)
	
