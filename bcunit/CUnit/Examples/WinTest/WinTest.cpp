// WinTest.cpp : Defines the entry point for the application.
//

#include "stdafx.h"

extern "C" {
	#include "Win.h"
}

int APIENTRY WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPSTR     lpCmdLine,
                     int       nCmdShow)
{
 	// TODO: Place code here.
	win_run_tests();
	return 0;
}



