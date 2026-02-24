// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnrealBenchmarkExporterBPLibrary.h"
#include "BenchmarkMetricsCollector.h"
#include "PrometheusHttpServer.h"
#include "PrometheusHttpPush.h"

void UUnrealBenchmarkExporterBPLibrary::InitializeBenchmarkCollector()
{
	UBenchmarkMetricsCollector::Initialize();
}

FBenchmarkMetrics UUnrealBenchmarkExporterBPLibrary::GetBenchmarkMetrics()
{
	if (UBenchmarkMetricsCollector* Collector = UBenchmarkMetricsCollector::Get())
	{
		return Collector->GetCurrentMetrics();
	}

	return FBenchmarkMetrics();
}

FString UUnrealBenchmarkExporterBPLibrary::GetPrometheusMetricsText()
{
	if (UBenchmarkMetricsCollector* Collector = UBenchmarkMetricsCollector::Get())
	{
		return Collector->GetPrometheusMetrics();
	}

	return TEXT("# ERROR: Metrics collector not initialized\n");
}

bool UUnrealBenchmarkExporterBPLibrary::IsPrometheusServerRunning()
{
	return UPrometheusHttpServer::IsServerRunning();
}

bool UUnrealBenchmarkExporterBPLibrary::StartPrometheusServer(int32 Port)
{
	return UPrometheusHttpServer::StartServer(Port);
}

void UUnrealBenchmarkExporterBPLibrary::StopPrometheusServer()
{
	UPrometheusHttpServer::StopServer();
}

FString UUnrealBenchmarkExporterBPLibrary::GetPrometheusServerURL()
{
	if (UPrometheusHttpServer::IsServerRunning())
	{
		return FString::Printf(TEXT("http://localhost:%d/metrics.txt"), UPrometheusHttpServer::GetServerPort());
	}

	return TEXT("Exporter not running");
}

bool UUnrealBenchmarkExporterBPLibrary::PushMetricsToPushgateway(const FString& PushgatewayURL, const FString& JobName)
{
	return UPrometheusHttpPush::PushToPushgateway(PushgatewayURL, JobName);
}

bool UUnrealBenchmarkExporterBPLibrary::PushMetricsToEndpoint(const FString& EndpointURL)
{
	return UPrometheusHttpPush::PushToEndpoint(EndpointURL);
}