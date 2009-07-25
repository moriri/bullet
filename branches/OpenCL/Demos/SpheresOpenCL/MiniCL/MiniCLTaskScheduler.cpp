/*
Bullet Continuous Collision Detection and Physics Library
Copyright (c) 2003-2007 Erwin Coumans  http://bulletphysics.com

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the use of this software.
Permission is granted to anyone to use this software for any purpose, 
including commercial applications, and to alter it and redistribute it freely, 
subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/

//#define __CELLOS_LV2__ 1

#define USE_SAMPLE_PROCESS 1
#ifdef USE_SAMPLE_PROCESS


#include "MiniCLTaskScheduler.h"
#include <stdio.h>

#ifdef __SPU__



void	SampleThreadFunc(void* userPtr,void* lsMemory)
{
	//do nothing
	printf("hello world\n");
}


void*	SamplelsMemoryFunc()
{
	//don't create local store memory, just return 0
	return 0;
}


#else


#include "BulletMultiThreaded/btThreadSupportInterface.h"

//#	include "SPUAssert.h"
#include <string.h>



extern "C" {
	extern char SPU_SAMPLE_ELF_SYMBOL[];
}

const char* cMiniCLKernels[CMD_MINICL_TOTAL_COMMANDS] = 
{
	"EMPTY_SLOT_0",
	"EMPTY_SLOT_1",
	"kPredictUnconstrainedMotion",
	"kSetSpheres",
	"kIntegrateTransforms",
	"kBitonicSortHash",
	"kBroadphaseCD",
	"kInitObjUsageTab",
	"kSetupBatches",
	"kCheckBatches",
	"kSetupContacts",
	"kSolveConstraints"
};


int	MiniCLTaskScheduler::findProgramCommandIdByName(const char* programName) const
{
	for(int i = 0; i < CMD_MINICL_TOTAL_COMMANDS; i++)
	{
		if(!strcmp(programName, cMiniCLKernels[i]))
		{
			return i;
		}
	}
	return -1;
}



MiniCLTaskScheduler::MiniCLTaskScheduler(btThreadSupportInterface*	threadInterface,  int maxNumOutstandingTasks)
:m_threadInterface(threadInterface),
m_maxNumOutstandingTasks(maxNumOutstandingTasks)
{

	m_taskBusy.resize(m_maxNumOutstandingTasks);
	m_spuSampleTaskDesc.resize(m_maxNumOutstandingTasks);

	for (int i = 0; i < m_maxNumOutstandingTasks; i++)
	{
		m_taskBusy[i] = false;
	}
	m_numBusyTasks = 0;
	m_currentTask = 0;

	m_initialized = false;

	m_threadInterface->startSPU();


}

MiniCLTaskScheduler::~MiniCLTaskScheduler()
{
	m_threadInterface->stopSPU();
	
}



void	MiniCLTaskScheduler::initialize()
{
#ifdef DEBUG_SPU_TASK_SCHEDULING
	printf("MiniCLTaskScheduler::initialize()\n");
#endif //DEBUG_SPU_TASK_SCHEDULING
	
	for (int i = 0; i < m_maxNumOutstandingTasks; i++)
	{
		m_taskBusy[i] = false;
	}
	m_numBusyTasks = 0;
	m_currentTask = 0;
	m_initialized = true;

}


void MiniCLTaskScheduler::issueTask(int firstWorkUnit, int lastWorkUnit,int kernelProgramId,char* argData,int* argSizes)
{

#ifdef DEBUG_SPU_TASK_SCHEDULING
	printf("MiniCLTaskScheduler::issueTask (m_currentTask= %d\)n", m_currentTask);
#endif //DEBUG_SPU_TASK_SCHEDULING

	m_taskBusy[m_currentTask] = true;
	m_numBusyTasks++;

	MiniCLTaskDesc& taskDesc = m_spuSampleTaskDesc[m_currentTask];
	{
		// send task description in event message
		taskDesc.m_firstWorkUnit = firstWorkUnit;
		taskDesc.m_lastWorkUnit = lastWorkUnit;
		taskDesc.m_kernelProgramId = kernelProgramId;
		//some bookkeeping to recognize finished tasks
		taskDesc.m_taskId = m_currentTask;
		
		for (int i=0;i<MINI_CL_MAX_ARG;i++)
		{
			taskDesc.m_argSizes[i] = argSizes[i];
			if (taskDesc.m_argSizes[i])
			{
				memcpy(&taskDesc.m_argData[i],&argData[MINICL_MAX_ARGLENGTH*i],taskDesc.m_argSizes[i]);
			}
		}
	}


	m_threadInterface->sendRequest(1, (ppu_address_t) &taskDesc, m_currentTask);

	// if all tasks busy, wait for spu event to clear the task.
	
	if (m_numBusyTasks >= m_maxNumOutstandingTasks)
	{
		unsigned int taskId;
		unsigned int outputSize;

		for (int i=0;i<m_maxNumOutstandingTasks;i++)
	  {
		  if (m_taskBusy[i])
		  {
			  taskId = i;
			  break;
		  }
	  }
		m_threadInterface->waitForResponse(&taskId, &outputSize);

		//printf("PPU: after issue, received event: %u %d\n", taskId, outputSize);

		postProcess(taskId, outputSize);

		m_taskBusy[taskId] = false;

		m_numBusyTasks--;
	}

	// find new task buffer
	for (int i = 0; i < m_maxNumOutstandingTasks; i++)
	{
		if (!m_taskBusy[i])
		{
			m_currentTask = i;
			break;
		}
	}
}


///Optional PPU-size post processing for each task
void MiniCLTaskScheduler::postProcess(int taskId, int outputSize)
{

}


void MiniCLTaskScheduler::flush()
{
#ifdef DEBUG_SPU_TASK_SCHEDULING
	printf("\nSpuCollisionTaskProcess::flush()\n");
#endif //DEBUG_SPU_TASK_SCHEDULING
	

	// all tasks are issued, wait for all tasks to be complete
	while(m_numBusyTasks > 0)
	{
// Consolidating SPU code
	  unsigned int taskId;
	  unsigned int outputSize;
	  
	  for (int i=0;i<m_maxNumOutstandingTasks;i++)
	  {
		  if (m_taskBusy[i])
		  {
			  taskId = i;
			  break;
		  }
	  }
	  {
			
		  m_threadInterface->waitForResponse(&taskId, &outputSize);
	  }

		//printf("PPU: flushing, received event: %u %d\n", taskId, outputSize);

		postProcess(taskId, outputSize);

		m_taskBusy[taskId] = false;

		m_numBusyTasks--;
	}


}

#endif


#endif //USE_SAMPLE_PROCESS
