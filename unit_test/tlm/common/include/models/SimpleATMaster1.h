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

#ifndef __SIMPLE_AT_MASTER1_H__
#define __SIMPLE_AT_MASTER1_H__

#include "tlm.h"
#include "simple_initiator_socket.h"
//#include <systemc>
#include <cassert>
#include <queue>
//#include <iostream>

class SimpleATMaster1 : public sc_core::sc_module
{
public:
  typedef tlm::tlm_generic_payload transaction_type;
  typedef tlm::tlm_phase           phase_type;
  typedef tlm::tlm_sync_enum       sync_enum_type;
  typedef SimpleInitiatorSocket<>  initiator_socket_type;

public:
  // extended transaction, holds tlm_generic_payload + data storage
  template <typename DT>
  class MyTransaction : public transaction_type
  {
  public:
    MyTransaction()
    {
      this->set_data_ptr(reinterpret_cast<unsigned char*>(&mData));
    }

    void setData(DT& data) { mData = data; }
    DT getData() const { return mData; }

  private:
    DT mData;
  };
  typedef MyTransaction<unsigned int>  mytransaction_type;

  // Dummy Transaction Pool
  template <typename T>
  class SimplePool
  {
  public:
    SimplePool() {}
    T* claim() { return new T(); }
    void release(T* t) { delete t; }
  };

public:
  initiator_socket_type socket;

public:
  SC_HAS_PROCESS(SimpleATMaster1);
  SimpleATMaster1(sc_core::sc_module_name name,
                  unsigned int nrOfTransactions = 0x5,
                  unsigned int baseAddress = 0x0) :
    sc_core::sc_module(name),
    socket("socket"),
    ACCEPT_DELAY(10, sc_core::SC_NS),
    mNrOfTransactions(nrOfTransactions),
    mBaseAddress(baseAddress),
    mTransactionCount(0),
    mCurrentTransaction(0)
  {
    // register nb_transport method
    REGISTER_NBTRANSPORT(socket, myNBTransport);

    // Master thread
    SC_THREAD(run);

    SC_METHOD(endResponse)
    sensitive << mEndResponseEvent;
    dont_initialize();
  }

  bool initTransaction(mytransaction_type*& trans)
  {
    if (mTransactionCount < mNrOfTransactions) {
      trans = transPool.claim();
      trans->set_address(mBaseAddress + 4*mTransactionCount);
      trans->setData(mTransactionCount);
      trans->set_command(tlm::TLM_WRITE_COMMAND);

    } else if (mTransactionCount < 2 * mNrOfTransactions) {
      trans = transPool.claim();
      trans->set_address(mBaseAddress + 4*(mTransactionCount - mNrOfTransactions));
      trans->set_command(tlm::TLM_READ_COMMAND);

    } else {
      return false;
    }

    ++mTransactionCount;
    return true;
  }

  void logStartTransation(mytransaction_type& trans)
  {
    if (trans.get_command() == tlm::TLM_WRITE_COMMAND) {
      std::cout << name() << ": Send write request: A = 0x"
                << std::hex << (unsigned int)trans.get_address()
                << ", D = 0x" << trans.getData() << std::dec
                << " @ " << sc_core::sc_time_stamp() << std::endl;
      
    } else {
      std::cout << name() << ": Send read request: A = 0x"
                << std::hex << (int)trans.get_address() << std::dec
                << " @ " << sc_core::sc_time_stamp() << std::endl;
    }
  }

  void logEndTransaction(mytransaction_type& trans)
  {
    if (trans.get_response_status() != tlm::TLM_OK_RESPONSE) {
      std::cout << name() << ": Received error response @ "
                << sc_core::sc_time_stamp() << std::endl;

    } else {
      std::cout << name() <<  ": Received ok response";
      if (trans.get_command() == tlm::TLM_READ_COMMAND) {
        std::cout << ": D = 0x" << trans.getData() << std::dec;
      }
      std::cout << " @ " << sc_core::sc_time_stamp() << std::endl;
    }
  }

  //
  // Simple AT Master
  // - Request must be accepted by the slave before the next request cwn be send
  // - Responses can come out of order
  // - Responses will be accepted after fixed delay
  //
  void run()
  {
    phase_type phase;
    sc_core::sc_time t;
    
    mytransaction_type* ptrans;
    while (initTransaction(ptrans)) {
      // Create transaction and initialise phase and t
      mytransaction_type& trans = *ptrans;
      phase = tlm::BEGIN_REQ;
      t = sc_core::SC_ZERO_TIME;

      logStartTransation(trans);

      switch (socket->nb_transport(trans, phase, t)) {
      case tlm::TLM_COMPLETED:
        // Transaction Finished, wait for the returned delay
        wait(t);
        logEndTransaction(trans);
        break;

      case tlm::TLM_SYNC:
      case tlm::TLM_SYNC_CONTINUE:
        switch (phase) {
        case tlm::BEGIN_REQ:
          // Request phase not yet finished
          // Wait until end of request phase before sending new request
          
          // FIXME
          mCurrentTransaction = &trans;
          wait(mEndRequestPhase);
	  mCurrentTransaction = 0;
          break;

        case tlm::END_REQ:
          // Request phase ended
          if (t != sc_core::SC_ZERO_TIME) {
            // Wait until end of request time before sending new request
            wait(t);
          }
          break;

        case tlm::BEGIN_RESP:
          // Request phase ended and response phase already started
          if (t != sc_core::SC_ZERO_TIME) {
            // Wait until end of request time before sending new request
            wait(t);
          }
          if (mEndResponseQueue.empty()) {
            // Notify end of response phase after ACCEPT delay
            mEndResponseEvent.notify(ACCEPT_DELAY);
          }
          mEndResponseQueue.push(&trans);
          break;

        case tlm::END_RESP:   // fall-through
        default:
          // A slave should never return with these phases
          // If phase == END_RESP, nb_transport should have returned true
          assert(0); exit(1);
          break;
        }
        break;

      case tlm::TLM_REJECTED:
        // FIXME: Not supported (wait and retry same transaction)
      default:
        assert(0); exit(1);
      };
    }
    sc_core::sc_stop();
    wait();
  }

  sync_enum_type myNBTransport(transaction_type& trans, phase_type& phase, sc_core::sc_time& t)
  {
    switch (phase) {
    case tlm::END_REQ:
      assert(t == sc_core::SC_ZERO_TIME); // FIXME: can t != 0?
      // Request phase ended
      mEndRequestPhase.notify(sc_core::SC_ZERO_TIME);
      return tlm::TLM_SYNC;

    case tlm::BEGIN_RESP:
    {
      assert(t == sc_core::SC_ZERO_TIME); // FIXME: can t != 0?

      // Notify end of request phase if run thread is waiting for it
      // FIXME
      if (&trans == mCurrentTransaction) {
        mEndRequestPhase.notify(sc_core::SC_ZERO_TIME);
      }

      assert(dynamic_cast<mytransaction_type*>(&trans));
      mytransaction_type* myTrans = static_cast<mytransaction_type*>(&trans);
      assert(myTrans); 

      if (mEndResponseQueue.empty()) {
        // Notify end of response phase after ACCEPT delay
        mEndResponseEvent.notify(ACCEPT_DELAY);
      }
      mEndResponseQueue.push(myTrans);
      return tlm::TLM_SYNC;
    }

    case tlm::BEGIN_REQ: // fall-through
    case tlm::END_RESP:  // fall-through
    default:
      // A slave should never call nb_transport with these phases
      assert(0); exit(1);
      return tlm::TLM_REJECTED;
    };
  }

  void endResponse()
  {
    assert(!mEndResponseQueue.empty());
    // end response phase
    phase_type phase = tlm::END_RESP;
    sc_core::sc_time t = sc_core::SC_ZERO_TIME;
    mytransaction_type* trans = mEndResponseQueue.front();
    assert(trans);
    mEndResponseQueue.pop();
    sync_enum_type r = socket->nb_transport(*trans, phase, t);
    assert(r == tlm::TLM_COMPLETED); // FIXME: slave should return TLM_COMPLETED?
    assert(t == sc_core::SC_ZERO_TIME); // t must be SC_ZERO_TIME

    logEndTransaction(*trans);

    transPool.release(trans);

    if (!mEndResponseQueue.empty()) {
      // Notify end of response phase after ACCEPT delay
      mEndResponseEvent.notify(ACCEPT_DELAY);
    }
  }

private:
  const sc_core::sc_time ACCEPT_DELAY;

private:
  unsigned int mNrOfTransactions;
  unsigned int mBaseAddress;
  SimplePool<mytransaction_type> transPool;
  unsigned int mTransactionCount;
  sc_core::sc_event mEndRequestPhase;
  std::queue<mytransaction_type*> mEndResponseQueue;
  sc_core::sc_event mEndResponseEvent;
  transaction_type* mCurrentTransaction;
};

#endif
