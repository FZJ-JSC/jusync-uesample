// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BenchmarkMetricsCollector.h"
#include "UnrealBenchmarkExporterBPLibrary.generated.h"

/**
 * Blueprint function library for Unreal Benchmark Exporter
 */
UCLASS()
class UNREALBENCHMARKEXPORTER_API UUnrealBenchmarkExporterBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Initialize the benchmark metrics collector */
	UFUNCTION(BlueprintCallable, Category = "Benchmark Exporter")
	static void InitializeBenchmarkCollector();

	/** Get current benchmark metrics */
	UFUNCTION(BlueprintCallable, Category = "Benchmark Exporter")
	static FBenchmarkMetrics GetBenchmarkMetrics();

	/** Get Prometheus metrics as text */
	UFUNCTION(BlueprintCallable, Category = "Benchmark Exporter")
	static FString GetPrometheusMetricsText();

	/** Check if HTTP server is running */
	UFUNCTION(BlueprintCallable, Category = "Benchmark Exporter")
	static bool IsPrometheusServerRunning();

	/** Start Prometheus HTTP server */
	UFUNCTION(BlueprintCallable, Category = "Benchmark Exporter")
	static bool StartPrometheusServer(int32 Port = 9090);

	/** Stop Prometheus HTTP server */
	UFUNCTION(BlueprintCallable, Category = "Benchmark Exporter")
	static void StopPrometheusServer();

	/** Get server URL */
	UFUNCTION(BlueprintCallable, Category = "Benchmark Exporter")
	static FString GetPrometheusServerURL();

	/** Push metrics to Prometheus Pushgateway (returns success) */
	UFUNCTION(BlueprintCallable, Category = "Benchmark Exporter")
	static bool PushMetricsToPushgateway(const FString& PushgatewayURL, const FString& JobName);

	/** Push metrics to any HTTP endpoint (returns success) */
	UFUNCTION(BlueprintCallable, Category = "Benchmark Exporter")
	static bool PushMetricsToEndpoint(const FString& EndpointURL);
};