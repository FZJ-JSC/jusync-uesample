// Copyright Epic Games, Inc. All Rights Reserved.

#include "PrometheusHttpPush.h"
#include "BenchmarkMetricsCollector.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

// Static member initialization
FString UPrometheusHttpPush::LastError = TEXT("");

bool UPrometheusHttpPush::PushToPushgateway(const FString& PushgatewayURL, const FString& JobName)
{
	// Format Pushgateway URL
	FString FullURL = PushgatewayURL;
	if (!FullURL.EndsWith(TEXT("/")))
	{
		FullURL += TEXT("/");
	}
	FullURL += TEXT("metrics/job/") + JobName;

	return PushToEndpoint(FullURL);
}

bool UPrometheusHttpPush::PushToEndpoint(const FString& EndpointURL)
{
	// Get metrics from collector
	FString MetricsText;
	if (UBenchmarkMetricsCollector* Collector = UBenchmarkMetricsCollector::Get())
	{
		MetricsText = Collector->GetPrometheusMetrics();
	}
	else
	{
		LastError = TEXT("Metrics collector not initialized");
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("Pushing metrics to: %s"), *EndpointURL);
	UE_LOG(LogTemp, Log, TEXT("Metrics length: %d characters"), MetricsText.Len());

	// Create HTTP request
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(EndpointURL);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("text/plain; version=0.0.4"));
	Request->SetContentAsString(MetricsText);

	// Set up response handler
	Request->OnProcessRequestComplete().BindLambda(
		[](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
		{
			if (bWasSuccessful && Response.IsValid())
			{
				int32 ResponseCode = Response->GetResponseCode();
				if (ResponseCode >= 200 && ResponseCode < 300)
				{
					UE_LOG(LogTemp, Log, TEXT("HTTP push successful: %d"), ResponseCode);
					UPrometheusHttpPush::LastError = TEXT("");
				}
				else
				{
					UPrometheusHttpPush::LastError = FString::Printf(TEXT("HTTP error: %d - %s"), 
						ResponseCode, *Response->GetContentAsString());
					UE_LOG(LogTemp, Error, TEXT("HTTP push failed: %s"), *UPrometheusHttpPush::LastError);
				}
			}
			else
			{
				UPrometheusHttpPush::LastError = TEXT("HTTP request failed");
				UE_LOG(LogTemp, Error, TEXT("HTTP request failed"));
			}
		}
	);

	// Send request
	if (Request->ProcessRequest())
	{
		// Request sent successfully (async)
		return true;
	}
	else
	{
		LastError = TEXT("Failed to start HTTP request");
		return false;
	}
}