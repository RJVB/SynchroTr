/*
 *  stub.cpp
 *  SynchroTr
 *
 *  Created by René J.V. Bertin on 20120708.
 *  Copyright 2012 IFSTTAR — LEPSIS. All rights reserved.
 *
 */

#include "CritSectEx/msemul.h"

// Xcode v3.x doesn't seem to allow building a project consisting of a single C file to be linked with
// a library containing C++ files: gcc will be used for linking, and thus the C++ libraries will not be
// linked in. Adding a single, stub C++ file to the project resolves the issue.
//static int OnlyHereToForceXCodeToBuildCSETESTdotCasCPP = 1;