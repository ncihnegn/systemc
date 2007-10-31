/*****************************************************************************

  The following code is derived, directly or indirectly, from the SystemC
  source code Copyright (c) 1996-2007 by all Contributors.
  All Rights reserved.

  The contents of this file are subject to the restrictions and limitations
  set forth in the SystemC Open Source License Version 2.4 (the "License");
  You may not use this file except in compliance with such restrictions and
  limitations. You may obtain instructions on how to receive a copy of the
  License at http://www.systemc.org/. Software distributed by Contributors
  under the License is distributed on an "AS IS" basis, WITHOUT WARRANTY OF
  ANY KIND, either express or implied. See the License for the specific
  language governing rights and limitations under the License.

 *****************************************************************************/

//#include <iostream>

//#include <systemc>

#include "tlm.h"

#include "SimpleLTMaster1.h"
#include "SimpleLTSlave1.h"
#include "TransactorLT2UT.h"
#include "TransactorUT2LT.h"

int sc_main(int argc, char* argv[])
{
  SimpleLTMaster1 master1("master1");
  SimpleLTSlave1 slave1("slave1");

  TransactorLT2UT transactorLT2UT("transactorLT2UT");
  TransactorUT2LT transactorUT2LT("transactorUT2LT");

  master1.socket(transactorLT2UT.targetSocket);
  transactorLT2UT.initiatorSocket(transactorUT2LT.targetSocket);
  transactorUT2LT.initiatorSocket(slave1.socket);

  sc_core::sc_start();

  return 0;
}
