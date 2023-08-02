#include <libiec61850/hal_thread.h>
#include "XSWI.h"
#include "iec61850_model_extensions.h"
#include "timestep_config.h"

// process simulator
void XSWI_simulate_switch(Input *input);

// open the circuit breaker(i.e. make it isolating)
void XSWI_open(XSWI *inst)
{
  inst->conducting = false;
}

// close the circuit breaker switch(i.e. make it conducting)
void XSWI_close(XSWI *inst)
{
  inst->conducting = true;
}

// callback for open/close signal from GOOSE-> will trigger process simulator threat
void XSWI_callback(InputEntry *extRef)
{
  // only one type of extref is expected: ctlVal
  bool state = MmsValue_getBoolean(extRef->value);
  if (state == true)
    XSWI_open(extRef->callBackParam);
  else
    XSWI_close(extRef->callBackParam);
}

// initialise XSWI instance for process simulation, and publish/subscription of GOOSE
void *XSWI_init(IedServer server, LogicalNode *ln, Input *input, LinkedList allInputValues)
{
  XSWI *inst = (XSWI *)malloc(sizeof(XSWI)); // create new instance with MALLOC
  inst->server = server;
  inst->Pos_stVal = (DataAttribute *)ModelNode_getChild((ModelNode *)ln, "Pos.stVal");
  inst->Pos_t = (DataAttribute *)ModelNode_getChild((ModelNode *)ln, "Pos.t");       // the node to operate on when a operate is triggered
  inst->Pos_stVal_callback = _findAttributeValueEx(inst->Pos_stVal, allInputValues); // find node that this element was subscribed to, so that it will be called during an update

  // inst->conducting = true;

  Dbpos stval = Dbpos_fromMmsValue(IedServer_getAttributeValue(server, inst->Pos_stVal));
  if (stval == DBPOS_ON)
    inst->conducting = true;
  else
    inst->conducting = false;

  if (input != NULL)
  {
    InputEntry *extref = input->extRefs;
    while (extref != NULL)
    {
      if (strcmp(extref->intAddr, "Tr") == 0) // TODO: should be Op, but then also modify in SCL
      {
        // register callbacks for GOOSE-subscription
        extref->callBack = (callBackFunction)XSWI_callback;
        extref->callBackParam = inst; // pass instance in param
      }
      extref = extref->sibling;
    }
  }
  else
  {
    printf("ERROR: no input element defined");
    return 0;
  }

  // start simulation threat
  Thread thread = Thread_create((ThreadExecutionFunction)XSWI_simulate_switch, input, true);
  Thread_start(thread);

  return inst;
}

void XSWI_change_switch(XSWI *inst, Dbpos value)
{
  uint64_t timestamp = Hal_getTimeInMs();
  IedServer_updateDbposValue(inst->server, inst->Pos_stVal, value);
  IedServer_updateUTCTimeAttributeValue(inst->server, inst->Pos_t, timestamp);
  InputValueHandleExtensionCallbacks(inst->Pos_stVal_callback); // update the associated callbacks with this Data Element
}

// threath for process-simulation: open/close switch
void XSWI_simulate_switch(Input *input)
{
  int state;
  int step = 0;
  XSWI *inst = input->extRefs->callBackParam; // take the initial callback, as they all contain the same object instance

  // inst->conducting = true;//initial state
  // XSWI_change_switch(inst,DBPOS_ON);//initial state
  if (inst->conducting)
  {
    XSWI_change_switch(inst, DBPOS_ON); // initial state
    state = 3;
  }
  else
  {
    XSWI_change_switch(inst, DBPOS_OFF); // initial state
    state = 1;
  }
  printf("XSWI: initialised in state: %i (0=opening, 1=opened, 2=closing, 3=closed)\n", state);

  while (1)
  {
    switch (state)
    {
    case 0: // opening
    {
      printf("XSWI: opening, ZZZZT\n");
      XSWI_change_switch(inst, DBPOS_INTERMEDIATE_STATE);

      Thread_sleep(2000);
      XSWI_change_switch(inst, DBPOS_OFF);
      printf("XSWI: opened\n");
      state = 1;
      break;
    }

    case 1: // opened
    {
      if (inst->conducting == true) // switch is closed
        state = 2;
      break;
    }

    case 2: // closing
    {
      printf("XSWI: closing\n");
      XSWI_change_switch(inst, DBPOS_INTERMEDIATE_STATE);

      Thread_sleep(2000);
      printf("XSWI: closed\n");
      XSWI_change_switch(inst, DBPOS_ON);
      state = 3;
      break;
    }

    case 3: // closed
    {
      if (inst->conducting == false)
        state = 0;
      break;
    }
    }
    if (IEC61850_server_timestep_type() == TIMESTEP_TYPE_REMOTE)
    {
      IEC61850_server_timestep_sync(step++);
    }
    else
    {
      Thread_sleep(100);
    }
  }
}
