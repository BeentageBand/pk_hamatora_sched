/*=====================================================================================*/
/**
 * app.c
 * author : puch
 * date : Oct 22 2015
 *
 * description : Any comments
 *
 */
/*=====================================================================================*/

#define Dbg_FID DBG_FID_DEF(APP_FID, 0)
#define COBJECT_IMPLEMENTATION
#include "app.h"
#include "app_fsm.h"
#include "dbg_log.h"
#include "ipc.h"

/*==============================================================================
 * Local Types
 * ============================================================================*/
struct APP_FSM
{
      union Worker * worker;
      union FSM fsm;
};

/*==============================================================================
 * Local Prototypes
 * ============================================================================*/
static void application_delete(struct Object * const obj);
static void application_on_mail(union Worker * const super, union Mail * const mail);
static void application_on_start(union Worker * const super);
static void application_on_loop(union Worker * const super);
static void application_on_stop(union Worker * const super);
static int application_startup(union Application * const this);

/*==============================================================================
 * Local Objects 
 * ============================================================================*/
Application_Class_T Application_Class =
{{
      {
            {application_delete, NULL},
            application_on_mail,
            application_on_start, 
            application_on_loop,  
            application_on_stop
      },
      application_startup
}};

static union Application Application = {{NULL}};
static union Mail Application_Mail_Buff[64] ={0};
static struct APP_FSM Application_Pool[IPC_MAX_TID] = {0};
static union St_Machine_State Application_St_Machine[APP_MAX_STID];

/*==============================================================================
 * Local Functions
 * ============================================================================*/
void application_delete(struct Object * const obj)
{
   union Application * const this = _cast(Application, obj);
   Isnt_Nullptr(this, );
}

void application_on_mail(union Worker * const super, union Mail * const mail)
{
   Application_T * const this = _dynamic_cast(Application, super);
   Isnt_Nullptr(this, );
   Mail_T const mail = {{NULL}};
   if (IPC_retreive_mail(&mail, IPC_RETRIEVE_TOUT_MS))
   {
         if(mail.mid >= APP_START_THREADS_INT_MID &&
            mail.mid <= APP_SHUTDOWN_INT_MID)
         {
               Isnt_Nullptr(mail.payload, );
               IPC_TID_T tid = *(IPC_TID_T *) mail.payload;
               Application_Pool[tid].fsm->dispatch(Application_Pool + tid, &mail);
         }
   }
}

void application_on_start(union Worker * const super)
{
   union Application * const this = _cast(Application, super);
   Isnt_Nullptr(this, );
   IPC_TID_T i;
   for(i = 0; i < IPC_MAX_TID; ++i)
   {
         union Worker * worker = Application_Pool[i].worker;
         if(worker)
         {
               IPC_Self_Send(APP_START_THREADS_INT_MID, &i, sizeof(i));
         }
   }
}

void application_on_loop(union Worker * const super)
{
      bool is_alive = true;
      IPC_TID_T i;
      for(i = 0; i < IPC_MAX_TID; ++i)
      {
            if(Application_Pool[i].worker &&
            APP_TERM_STID != Application_Pool[i].fsm.State_Machine.current_st)
            {
                  is_alive = false;
                  break;
            }
      }

      if(is_alive)
      {
            IPC_Self_Send(WORKER_SHUTDOWN_INT_MID, NULL, 0);
      }
}

void application_on_stop(union Worker * const super)
{
   union Application * const this = _cast(Application, super);
   Isnt_Nullptr(this, );
}

int application_startup(union Application * const this)
{
      IPC_Run(APP_WORKER_TID);
      while(1){}
      return -1; //TERMINATED
}

/*==============================================================================
 * External Functions
 * ============================================================================*/
void Populate_Application(union Application * const this, union Worker * (* factory_method)(IPC_TID_T const tid))
{
      if(NULL == Application.vtbl)
      {
            Populate_Worker(&Application.Worker, APP_WORKER_TID,
            Application_Mail_Buff, Num_Elems(Application_Mail_Buff));
            Object_Init(&Application.Object, &Application_Class.Class,
            sizeof(Application_Class.Worker));
            Application.vtbl = &Application_Class;
      }
      memcpy(this, &Application, sizeof(Application));

      IPC_TID_T i;
      for(i = 0; i < IPC_MAX_TID; ++i)
      {
            Application_Pool[i].worker = factory_method(i);
            if(NULL != Application_Pool[i].worker)
            {
                  Populate_FSM(&Application_Pool[i].fsm, 
                  Application_St_Chart, Num_Elems(Application_St_Chart),
                  Application_St_Machine, Num_Elems(Application_St_Machine));
            }
      }
}

void Application_initialized(void)
{
   IPC_TID_T self = IPC_Self();
   IPC_Send(APP_WORKER_TID,  APP_THREAD_INIT_INT_MID, &self, sizeof(self));
}

void Application_terminated(void)
{
   IPC_TID_T self = IPC_Self();
   IPC_Send(APP_WORKER_TID,  APP_THREAD_TERM_INT_MID, &self, sizeof(self));
}

void Application_shutdown(void)
{
   IPC_TID_T self = IPC_Self();
   IPC_Send(APP_WORKER_TID,  APP_SHUTDOWN_INT_MID, &self, sizeof(self));
}