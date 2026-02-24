// Copyright Epic Games, Inc. All Rights Reserved.

#include "PrometheusHttpServer.h"
#include "BenchmarkMetricsCollector.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFilemanager.h"

// Static member initialization
int32 UPrometheusHttpServer::ServerPort = 9090;
bool UPrometheusHttpServer::bIsRunning = false;

bool UPrometheusHttpServer::StartServer(int32 Port)
{
	ServerPort = Port;
	bIsRunning = true;

	// Create metrics directory
	FString MetricsDir = FPaths::ProjectSavedDir() / TEXT("PrometheusMetrics");
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	
	if (!PlatformFile.DirectoryExists(*MetricsDir))
	{
		PlatformFile.CreateDirectory(*MetricsDir);
	}

	UE_LOG(LogTemp, Log, TEXT("Prometheus metrics exporter started on port %d"), ServerPort);
	UE_LOG(LogTemp, Log, TEXT("Metrics will be available via external HTTP server at: http://localhost:%d/metrics"), ServerPort);
	UE_LOG(LogTemp, Log, TEXT("Run: python -m http.server %d --directory \"%s\""), ServerPort, *MetricsDir);
	
	return true;
}

void UPrometheusHttpServer::StopServer()
{
	bIsRunning = false;
	UE_LOG(LogTemp, Log, TEXT("Prometheus metrics exporter stopped"));
}

bool UPrometheusHttpServer::IsServerRunning()
{
	return bIsRunning;
}

FString UPrometheusHttpServer::GetMetricsFilePath()
{
	FString MetricsDir = FPaths::ProjectSavedDir() / TEXT("PrometheusMetrics");
	FString MetricsFilePath = MetricsDir / TEXT("metrics.txt");
	return MetricsFilePath;
}