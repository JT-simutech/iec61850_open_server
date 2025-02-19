#include "iec61850_model_extensions.h"
#include "inputs_api.h"
#include "RBDR.h"

// callback when GOOSE is received
void RBDR_callback(InputEntry *extRef)
{
  RBDR *inst = extRef->callBackParam;
  inst->buffer[inst->bufferIndex] = MmsValue_toInt32(extRef->value);
    inst->bufferIndex++;
    if(inst->bufferIndex >= RBDR_MAX_SAMPLES)
    {
      inst->bufferIndex = 0;
    }
}

void * RBDR_init(IedServer server, LogicalNode *ln, IedModel * model , IedModel_extensions * model_ex,Input *input, LinkedList allInputValues)
{
  RBDR *inst = (RBDR *)malloc(sizeof(RBDR)); // create new instance with MALLOC
  inst->server = server;
  inst->input = input;
  inst->bufferIndex = 0;
  for(int i = 0; i < RBDR_MAX_SAMPLES; i++)
  {
    inst->buffer[i] = 0;
  }
  //register callback for digital input
  // this should record the data in a buffer
  if (input != NULL)
  {
    InputEntry *extRef = input->extRefs;

    while (extRef != NULL)
    {
      if (strcmp(extRef->intAddr, "digital") == 0) // find extref for the last SMV, using the intaddr, so that all values are updated
      {
        extRef->callBack = (callBackFunction)RBDR_callback;
        extRef->callBackParam = inst;
      }
      extRef = extRef->sibling;
    }
  }
  return inst;
}