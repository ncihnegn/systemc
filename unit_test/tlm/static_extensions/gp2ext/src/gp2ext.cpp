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

#include "tlm.h"

#include "SimpleLTMaster1_DMI.h"
#include "SimpleLTSlave_ext.h"
#include "extension_adaptors.h"


int sc_main(int argc, char* argv[])
{
  SimpleLTMaster1_dmi master("master1", 10, 0x0);
  adapt_gp2ext<32> bridge("bridge");
  SimpleLTSlave_ext  slave("slave1");

  master.socket(bridge.target_socket);
  bridge.initiator_socket(slave.socket);

  sc_core::sc_start();

  return 0;
}
