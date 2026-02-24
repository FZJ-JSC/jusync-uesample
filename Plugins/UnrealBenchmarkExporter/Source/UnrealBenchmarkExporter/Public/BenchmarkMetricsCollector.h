// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "TimerManager.h"
#include "BenchmarkMetricsCollector.generated.h"

/**
 * Structure to hold benchmark metrics
 */
USTRUCT(BlueprintType)
struct FBenchmarkMetrics
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Benchmark")
	float FPS = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Benchmark")
	float FrameTimeMs = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Benchmark")
	float UsedPhysicalMemoryGB = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Benchmark")
	float AvailablePhysicalMemoryGB = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Benchmark")
	float UsedVirtualMemoryGB = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Benchmark")
	float CPUUsagePercent = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Benchmark")
	int32 TotalVertices = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Benchmark")
	int32 TotalTriangles = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Benchmark")
	int32 DrawCalls = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Benchmark")
	int32 RenderedPrimitives = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Benchmark")
	float GPUMemoryUsedGB = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Benchmark")
	float GPUMemoryAvailableGB = 0.0f;
};

/**
 * Singleton class for collecting benchmark metrics from Unreal Engine
 */
UCLASS(BlueprintType)
class UNREALBENCHMARKEXPORTER_API UBenchmarkMetricsCollector : public UObject
{
	GENERATED_BODY()

public:
	/** Initialize the metrics collector */
	static void Initialize();

	/** Shutdown the metrics collector */
	static void Shutdown();

	/** Get the singleton instance */
	static UBenchmarkMetricsCollector* Get();

	/** Get the benchmark metrics collector instance (Blueprint callable) */
	UFUNCTION(BlueprintCallable, Category = "Benchmark", meta = (DisplayName = "Get Benchmark Metrics Collector"))
	static UBenchmarkMetricsCollector* GetBenchmarkMetricsCollector();

	/** Collect all benchmark metrics */
	UFUNCTION(BlueprintCallable, Category = "Benchmark")
	void CollectMetrics();

	/** Get the current metrics */
	UFUNCTION(BlueprintCallable, Category = "Benchmark")
	FBenchmarkMetrics GetCurrentMetrics() const { return CurrentMetrics; }

	/** Format metrics as Prometheus text format */
	UFUNCTION(BlueprintCallable, Category = "Benchmark")
	FString GetPrometheusMetrics() const;

	/** Start automatic metrics collection at specified interval (in seconds) */
	UFUNCTION(BlueprintCallable, Category = "Benchmark", meta = (WorldContext = "WorldContextObject"))
	void StartAutoCollection(float IntervalSeconds = 1.0f, UObject* WorldContextObject = nullptr);

	/** Start automatic metrics collection with Pushgateway push */
	UFUNCTION(BlueprintCallable, Category = "Benchmark", meta = (WorldContext = "WorldContextObject"))
	void StartAutoCollectionWithPush(float IntervalSeconds = 1.0f, const FString& PushgatewayURL = TEXT("http://localhost:9091"), const FString& JobName = TEXT("unreal_benchmark"), UObject* WorldContextObject = nullptr);

	/** Stop automatic metrics collection */
	UFUNCTION(BlueprintCallable, Category = "Benchmark", meta = (WorldContext = "WorldContextObject"))
	void StopAutoCollection(UObject* WorldContextObject = nullptr);

	/** Check if auto collection is active */
	UFUNCTION(BlueprintCallable, Category = "Benchmark")
	bool IsAutoCollectionActive() const;

protected:
	/** Constructor */
	UBenchmarkMetricsCollector();

private:
	/** Get CPU usage percentage */
	float GetCPUUsage();

	/** Get GPU memory stats */
	void GetGPUMemory();

	/** Get rendering statistics */
	void GetRenderingStats();

	/** Get total GPU memory in GB */
	float GetTotalGPUMemory();

private:
	/** Singleton instance */
	static UBenchmarkMetricsCollector* Instance;

	/** Current metrics */
	FBenchmarkMetrics CurrentMetrics;

	/** Timer handle for periodic collection */
	FTimerHandle CollectionTimerHandle;

	/** Timer handle for retrying when world context is not available */
	FTimerHandle RetryTimerHandle;

	/** Pushgateway URL for auto-push (empty if not pushing) */
	FString AutoPushPushgatewayURL;

	/** Pending collection interval (used when world context is not available) */
	float PendingCollectionInterval;

	/** Pending job name (used when world context is not available) */
	FString PendingJobName;

	/** Collect FPS and frame time */
	void CollectPerformanceMetrics();

	/** Collect memory usage */
	void CollectMemoryMetrics();

	/** Collect CPU usage */
	void CollectCPUMetrics();

	/** Collect rendering statistics */
	void CollectRenderingMetrics();


};