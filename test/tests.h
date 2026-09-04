#pragma once

#define X_TESTS(X)\
	X(Test_Testing_Checks)\
	X(Test_Testing_TestWithParameter, 123, "Hello Tests!")\
	X(Test_Testing_Skipping)\
	\
	X(Test_TextController_Movements)\
	X(Test_TextController_Commands)
	
