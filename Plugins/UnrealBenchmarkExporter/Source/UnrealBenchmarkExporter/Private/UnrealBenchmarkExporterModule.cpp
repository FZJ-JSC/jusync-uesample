// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnrealBenchmarkExporterModule.h"
#include "BenchmarkMetricsCollector.h"
#include "PrometheusHttpServer.h"

#define LOCTEXT_NAMESPACE "FUnrealBenchmarkExporterModule"

void FUnrealBenchmarkExporterModule::StartupModule()
{
	UE_LOG(LogTemp, Log, TEXT("UnrealBenchmarkExporter module starting up"));

	// Initialize the metrics collector
	UBenchmarkMetricsCollector::Initialize();

	// Start the Prometheus metrics exporter
	bool bExporterStarted = UPrometheusHttpServer::StartServer(9090);
	
	if (bExporterStarted)
	{
		UE_LOG(LogTemp, Log, TEXT("UnrealBenchmarkExporter module started. Metrics will be written to Saved/PrometheusMetrics/"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to start Prometheus metrics exporter. Metrics collection will continue."));
	}
}

void FUnrealBenchmarkExporterModule::ShutdownModule()
{
	UE_LOG(LogTemp, Log, TEXT("UnrealBenchmarkExporter module shutting down"));

	// Stop the HTTP server
	UPrometheusHttpServer::StopServer();

	// Clean up metrics collector
	UBenchmarkMetricsCollector::Shutdown();

	UE_LOG(LogTemp, Log, TEXT("UnrealBenchmarkExporter module shutdown complete"));
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FUnrealBenchmarkExporterModule, UnrealBenchmarkExporter)