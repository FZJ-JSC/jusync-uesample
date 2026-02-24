// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PrometheusHttpPush.generated.h"

/**
 * Simple HTTP client for pushing Prometheus metrics
 */
UCLASS(BlueprintType)
class UNREALBENCHMARKEXPORTER_API UPrometheusHttpPush : public UObject
{
	GENERATED_BODY()

public:
	/** Push metrics to Prometheus Pushgateway (simple synchronous version) */
	UFUNCTION(BlueprintCallable, Category = "Prometheus")
	static bool PushToPushgateway(const FString& PushgatewayURL, const FString& JobName);

	/** Push metrics to any HTTP endpoint (simple synchronous version) */
	UFUNCTION(BlueprintCallable, Category = "Prometheus")
	static bool PushToEndpoint(const FString& EndpointURL);

	/** Get last error message */
	UFUNCTION(BlueprintCallable, Category = "Prometheus")
	static FString GetLastError() { return LastError; }

private:
	/** Last error message */
	static FString LastError;
};