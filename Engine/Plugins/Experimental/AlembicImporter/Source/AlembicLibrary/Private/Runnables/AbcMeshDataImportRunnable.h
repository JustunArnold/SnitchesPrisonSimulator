// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"

struct FAbcImportData;

class FAbcMeshDataImportRunnable : public FRunnable
{
public:
	FAbcMeshDataImportRunnable(FAbcImportData* InImportData, const int32 InStartFrameIndex, const int32 InStopFrameIndex, const float InTimeStep);
	~FAbcMeshDataImportRunnable();
	
	// Begin FRunnable overrides
	virtual bool Init() override;
	virtual uint32 Run() override;
	// End FRunnable overrides

	/** Allows to force waiting for the runnable to finish */
	void Wait();

	const bool WasSuccesful() const;
private:
	/** Data structure used to store data generated by this class */
	FAbcImportData* ImportData;

	/** Start and stop frame indices to process within this runnable*/
	int32 StartFrameIndex;
	int32 StopFrameIndex;
	float TimeStep;

	/** Succes flag*/
	bool bImportSuccesful;

	/** Internal thread this instance is running on */
	FRunnableThread* WorkerThread;
};