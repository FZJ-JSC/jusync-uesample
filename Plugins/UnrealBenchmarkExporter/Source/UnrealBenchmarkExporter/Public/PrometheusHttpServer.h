// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PrometheusHttpServer.generated.h"

/**
 * Simple metrics exporter that writes Prometheus metrics to file
 * The file can then be served by an external HTTP server
 */
UCLASS(BlueprintType)
class UNREALBENCHMARKEXPORTER_API UPrometheusHttpServer : public UObject
{
	GENERATED_BODY()

public:
	/** Start the metrics exporter */
	UFUNCTION(BlueprintCallable, Category = "Prometheus")
	static bool StartServer(int32 Port = 9090);

	/** Stop the metrics exporter */
	UFUNCTION(BlueprintCallable, Category = "Prometheus")
	static void StopServer();

	/** Check if exporter is running */
	UFUNCTION(BlueprintCallable, Category = "Prometheus")
	static bool IsServerRunning();

	/** Get the current server port */
	UFUNCTION(BlueprintCallable, Category = "Prometheus")
	static int32 GetServerPort() { return ServerPort; }

	/** Get metrics file path */
	UFUNCTION(BlueprintCallable, Category = "Prometheus")
	static FString GetMetricsFilePath();

private:
	/** Server port */
	static int32 ServerPort;

	/** Is exporter running */
	static bool bIsRunning;
};