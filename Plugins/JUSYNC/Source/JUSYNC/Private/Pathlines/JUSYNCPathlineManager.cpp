#include "JUSYNCPathlineManager.h"

#include "Async/Async.h"
#include "Engine/World.h"
#include "HAL/CriticalSection.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "JUSYNCPointCloudSpawner.h"
#include "Misc/ScopeLock.h"

#include <atomic>

namespace
{
	// 30 Hz keeps the flow band smooth at 0.55 bands/s while the rebuild stays
	// well under a millisecond for a few thousand nodes.
	constexpr double FlowRebuildIntervalSeconds = 1.0 / 30.0;

	struct FPathlineKey
	{
		int32 Rank = 0;
		uint64 Id = 0;

		bool operator==(const FPathlineKey& Other) const
		{
			return Rank == Other.Rank && Id == Other.Id;
		}

		friend uint32 GetTypeHash(const FPathlineKey& Key)
		{
			uint64 H = Key.Id ^ (static_cast<uint64>(static_cast<uint32>(Key.Rank)) << 32);
			H ^= H >> 33;
			H *= 0xff51afd7ed558ccdull;
			H ^= H >> 33;
			return static_cast<uint32>(H);
		}
	};

	struct FPathlineNode
	{
		FVector Position = FVector::ZeroVector;
		float Scalar = 0.0f;
		float Width = 1.0f;
		bool bHasScalar = false;
		FColor Color = FColor::White;
		bool bHasColor = false;
		bool bBreak = false;
		uint64 SampleId = 0;
	};

	struct FPathlineTrack
	{
		FPathlineKey Key;
		TArray<FPathlineNode> Nodes;
		uint64 LastSeenCounter = 0;
		double FirstSeenSeconds = 0.0;
	};

	struct FElementState
	{
		TMap<FPathlineKey, FPathlineTrack> Tracks;
		TMap<int32, TSet<FPathlineKey>> PreviousKeysByRank;
		TArray<FColor> Lut;
		uint64 SampleCounter = 0;
		uint64 LastSampleId = 0;
		bool bHasSample = false;
	};

	struct FSamplePoint
	{
		FVector Position = FVector::ZeroVector;
		float Scalar = 0.0f;
		float Width = 1.0f;
		bool bHasScalar = false;
		FColor Color = FColor::White;
		bool bHasColor = false;
	};

	struct FCandidate
	{
		FPathlineKey Key;
		uint64 Score = 0;
	};

	struct FWorkItem
	{
		enum class EType
		{
			Sample,
			RebuildAll
		};

		EType Type = EType::Sample;
		FString ElementName;
		FJUSYNCPointCloudRef Cloud;
		int32 Rank = 0;
		uint64 SampleId = 0;
		TArray<FColor> Lut;
		uint64 Generation = 0;
	};

	struct FPendingArrays
	{
		FString ElementName;
		TSharedPtr<FJUSYNCPathlineArrays, ESPMode::ThreadSafe> Arrays;
		uint64 Generation = 0;
	};

	uint64 Mix64(uint64 Value)
	{
		Value ^= Value >> 30;
		Value *= 0xbf58476d1ce4e5b9ull;
		Value ^= Value >> 27;
		Value *= 0x94d049bb133111ebull;
		Value ^= Value >> 31;
		return Value;
	}

	uint64 SelectionScore(const FString& ElementName, int32 Rank, uint64 Id, uint64 Seed)
	{
		uint64 Hash = Mix64(Seed ^ 0x9e3779b97f4a7c15ull);
		for (int32 i = 0; i < ElementName.Len(); ++i)
		{
			Hash = Mix64(Hash ^ static_cast<uint32>(ElementName[i]));
		}
		Hash = Mix64(Hash ^ static_cast<uint32>(Rank));
		Hash = Mix64(Hash ^ Id);
		return Hash;
	}

	uint64 DecodeParticleId(const FJUSYNCPointCloudData& Cloud, int32 Index,
		const FString& AttributeName, bool& bOutFromAttribute)
	{
		const FJUSYNCPointAttribute* Attribute = Cloud.FindAttribute(AttributeName);
		if (Attribute && Attribute->Components >= 1 && Attribute->Components <= 4)
		{
			const int64 Base = static_cast<int64>(Index) * Attribute->Components;
			if (Base >= 0 && Base + Attribute->Components <= Attribute->Values.Num())
			{
				uint64 Id = 0;
				for (int32 Component = 0; Component < Attribute->Components; ++Component)
				{
					const uint32 Chunk = static_cast<uint32>(
						FMath::RoundToInt(Attribute->Values[static_cast<int32>(Base) + Component]));
					Id |= static_cast<uint64>(Chunk & 0xFFFFu) << (16 * Component);
				}
				bOutFromAttribute = true;
				return Id;
			}
		}

		bOutFromAttribute = false;
		return static_cast<uint64>(static_cast<uint32>(Index));
	}

	const TArray<FColor>& DefaultScalarLut()
	{
		static const TArray<FColor> Ramp = []
		{
			const FLinearColor Stops[] = {
				FLinearColor(0.267f, 0.005f, 0.329f),
				FLinearColor(0.283f, 0.141f, 0.458f),
				FLinearColor(0.254f, 0.265f, 0.530f),
				FLinearColor(0.207f, 0.372f, 0.553f),
				FLinearColor(0.164f, 0.471f, 0.558f),
				FLinearColor(0.128f, 0.567f, 0.551f),
				FLinearColor(0.135f, 0.659f, 0.518f),
				FLinearColor(0.267f, 0.749f, 0.441f),
				FLinearColor(0.478f, 0.821f, 0.318f),
				FLinearColor(0.741f, 0.873f, 0.150f),
				FLinearColor(0.993f, 0.906f, 0.144f)
			};
			TArray<FColor> Out;
			Out.Reserve(UE_ARRAY_COUNT(Stops));
			for (const FLinearColor& Stop : Stops)
			{
				Out.Add(Stop.ToFColor(true));
			}
			return Out;
		}();
		return Ramp;
	}

	float HueDistance(float HueA, float HueB)
	{
		const float Delta = FMath::Abs(HueA - HueB);
		return FMath::Min(Delta, 360.0f - Delta);
	}

	void RgbToHS(const FLinearColor& Color, float& OutHue, float& OutSaturation)
	{
		const float Max = FMath::Max3(Color.R, Color.G, Color.B);
		const float Min = FMath::Min3(Color.R, Color.G, Color.B);
		const float Delta = Max - Min;

		OutSaturation = Max > KINDA_SMALL_NUMBER ? Delta / Max : 0.0f;

		if (Delta <= KINDA_SMALL_NUMBER)
		{
			OutHue = 0.0f;
			return;
		}
		if (Max == Color.R)
		{
			OutHue = 60.0f * FMath::Fmod((Color.G - Color.B) / Delta, 6.0f);
		}
		else if (Max == Color.G)
		{
			OutHue = 60.0f * ((Color.B - Color.R) / Delta + 2.0f);
		}
		else
		{
			OutHue = 60.0f * ((Color.R - Color.G) / Delta + 4.0f);
		}
		if (OutHue < 0.0f)
		{
			OutHue += 360.0f;
		}
	}

	FColor ResolveColor(const FElementState& Element, const FJUSYNCPathlineSettings& Settings,
		const FPathlineNode& Node)
	{
		FColor Flat = Settings.FlatColor.ToFColor(true);
		if (!Settings.bColorByScalar)
		{
			return Flat;
		}
		if (Node.bHasColor)
		{
			return Node.Color;
		}
		if (!Node.bHasScalar)
		{
			return Flat;
		}

		const TArray<FColor>& Lut = Element.Lut.Num() > 0 ? Element.Lut : DefaultScalarLut();
		const float Range = Settings.ColorMax - Settings.ColorMin;
		const float Alpha = FMath::IsNearlyZero(Range)
			? 0.0f
			: FMath::Clamp((Node.Scalar - Settings.ColorMin) / Range, 0.0f, 1.0f);
		const int32 Index = FMath::Clamp(
			FMath::RoundToInt(Alpha * static_cast<float>(Lut.Num() - 1)),
			0, Lut.Num() - 1);
		return Lut[Index];
	}
}

FString FJUSYNCPathlineManager::ResolveAttributeName(const FJUSYNCPointCloudData& Cloud,
	const FString& Name)
{
	if (Name.IsEmpty() || Cloud.FindAttribute(Name))
	{
		return Name;
	}
	const FString Prefixed = FString(TEXT("vtk_")) + Name;
	return Cloud.FindAttribute(Prefixed) ? Prefixed : Name;
}

struct FJUSYNCPathlineManager::FImpl
{
	FJUSYNCPathlineSettings Settings;
	FJUSYNCPathlineStats Stats;

	mutable FCriticalSection StateLock;
	FCriticalSection QueueLock;
	FCriticalSection PendingLock;

	TMap<FString, FElementState> Elements;
	TArray<TSharedPtr<FWorkItem>> Queue;
	TArray<FPendingArrays> PendingArrays;

	std::atomic<bool> bWorkerRunning{ false };
	std::atomic<bool> bShuttingDown{ false };
	std::atomic<uint64> Generation{ 0 };

	uint64 DisplaySampleId = TNumericLimits<uint64>::Max();
	// Sample that was on screen when the current tip blend was armed. The build
	// interpolates the nodes between it and DisplaySampleId, which is what makes
	// the trail advance and retract instead of popping in either direction.
	uint64 BlendFromSampleId = TNumericLimits<uint64>::Max();
	uint64 LiveHeadSampleId = TNumericLimits<uint64>::Max();
	bool bResampleRequested = false;
	bool bRebuildQueued = false;

	float TipBlendSeconds = 0.0f;
	float TipBlendAlpha = 0.0f;
	double TipBlendStartSeconds = 0.0;
	bool bTipBlendActive = false;

	// Flow moves with wall time, so it needs its own rebuild cadence; nothing
	// else re-emits the trail while the sender is idle.
	double LastFlowRebuildSeconds = 0.0;

	void ClearLocked()
	{
		Elements.Reset();
		Stats = FJUSYNCPathlineStats();
		DisplaySampleId = TNumericLimits<uint64>::Max();
		BlendFromSampleId = TNumericLimits<uint64>::Max();
		LiveHeadSampleId = TNumericLimits<uint64>::Max();
		bTipBlendActive = false;
		TipBlendAlpha = 0.0f;
		Generation.fetch_add(1);
	}

	void RefreshTotalsLocked()
	{
		Stats.ActiveTracks = 0;
		Stats.TotalNodes = 0;
		for (const TPair<FString, FElementState>& ElementPair : Elements)
		{
			Stats.ActiveTracks += ElementPair.Value.Tracks.Num();
			for (const TPair<FPathlineKey, FPathlineTrack>& TrackPair : ElementPair.Value.Tracks)
			{
				Stats.TotalNodes += TrackPair.Value.Nodes.Num();
			}
		}
	}

	void IngestLocked(const FString& ElementName, const FJUSYNCPointCloudRef& Cloud,
		int32 Rank, uint64 SampleId, const TArray<FColor>& Lut)
	{
		const double StartSeconds = FPlatformTime::Seconds();

		if (!Cloud.IsValid() || !Cloud->bFromRawGeo || !Cloud->bComplete ||
			Cloud->PointCount <= 0)
		{
			++Stats.IgnoredSamples;
			Stats.LastIngestMs = (FPlatformTime::Seconds() - StartSeconds) * 1000.0;
			return;
		}

		FElementState& Element = Elements.FindOrAdd(ElementName);
		if (!Element.bHasSample)
		{
			Element.bHasSample = true;
			Element.LastSampleId = SampleId;
		}
		else if (Element.LastSampleId != SampleId)
		{
			++Element.SampleCounter;
			Element.LastSampleId = SampleId;
		}
		const uint64 ThisCounter = Element.SampleCounter;
		Element.Lut = Lut;

		TMap<FPathlineKey, FSamplePoint> Current;
		Current.Reserve(Cloud->PointCount);

		const FString IdAttribute =
			FJUSYNCPathlineManager::ResolveAttributeName(*Cloud, Settings.IdAttribute);
		const FString ScalarAttribute =
			FJUSYNCPathlineManager::ResolveAttributeName(*Cloud, Settings.ScalarAttribute);

		int32 Duplicates = 0;
		for (int32 PointIndex = 0; PointIndex < Cloud->PointCount; ++PointIndex)
		{
			bool bFromAttribute = false;
			const uint64 Id = DecodeParticleId(*Cloud, PointIndex, IdAttribute, bFromAttribute);
			const FPathlineKey Key{ Rank, Id };
			if (Current.Contains(Key))
			{
				++Duplicates;
				continue;
			}

			FSamplePoint Point;
			Point.Position = Cloud->GetPosition(PointIndex);
			Point.Width = Cloud->GetWidth(PointIndex);
			Point.bHasColor = false;
			if (Cloud->bHasColors && Cloud->Colors.IsValidIndex(PointIndex))
			{
				Point.Color = Cloud->Colors[PointIndex];
				Point.bHasColor = true;
			}
			Point.bHasScalar = Cloud->GetAttributeScalar(
				PointIndex, ScalarAttribute, Settings.ScalarComponent, Point.Scalar);
			if (!FMath::IsFinite(Point.Scalar))
			{
				Point.bHasScalar = false;
				Point.Scalar = 0.0f;
			}
			Current.Add(Key, Point);
		}
		Stats.DuplicateIds += Duplicates;

		static std::atomic<bool> bScalarDiagLogged{ false };
		if (!bScalarDiagLogged.exchange(true))
		{
			bool bAnyScalar = false;
			float SampleValue = 0.0f;
			bool bAnyPointColor = false;
			FColor PointColor = FColor::Black;
			for (const TPair<FPathlineKey, FSamplePoint>& Pair : Current)
			{
				if (Pair.Value.bHasScalar)
				{
					bAnyScalar = true;
					SampleValue = Pair.Value.Scalar;
				}
				if (Pair.Value.bHasColor)
				{
					bAnyPointColor = true;
					PointColor = Pair.Value.Color;
				}
				if (bAnyScalar && bAnyPointColor)
				{
					break;
				}
			}
			const FString LutFirst = Element.Lut.Num() > 0
				? Element.Lut[0].ToString() : FString(TEXT("-"));
			const FString LutMid = Element.Lut.Num() > 1
				? Element.Lut[Element.Lut.Num() / 2].ToString() : FString(TEXT("-"));
			const TCHAR* Verdict = !Settings.bColorByScalar ? TEXT("flat")
				: (bAnyPointColor ? TEXT("POINT-COLOURED")
					: (!bAnyScalar ? TEXT("FLAT WHITE: no scalar, no point colours")
								   : TEXT("scalar-coloured")));
			UE_LOG(LogTemp, Display,
				TEXT("[Pathlines] color diag: pointColor=%d point=%s scalar='%s' present=%d "
					 "value=%.4f lut=%d first=%s mid=%s (trails will be %s)"),
				bAnyPointColor ? 1 : 0, *PointColor.ToString(),
				*ScalarAttribute, bAnyScalar ? 1 : 0, SampleValue, Element.Lut.Num(),
				*LutFirst, *LutMid, Verdict);
		}

		const int32 ExpirySamples = FMath::Max(0, Settings.TrackExpirySamples);
		for (auto It = Element.Tracks.CreateIterator(); It; ++It)
		{
			if (It.Key().Rank == Rank &&
				!Current.Contains(It.Key()) &&
				ThisCounter - It.Value().LastSeenCounter > static_cast<uint64>(ExpirySamples))
			{
				It.RemoveCurrent();
			}
		}

		TArray<FCandidate> Candidates;
		Candidates.Reserve(Current.Num() + Element.Tracks.Num());
		for (const TPair<FPathlineKey, FSamplePoint>& Pair : Current)
		{
			Candidates.Add({ Pair.Key,
				SelectionScore(ElementName, Pair.Key.Rank, Pair.Key.Id, Settings.SelectionSeed) });
		}
		for (const TPair<FPathlineKey, FPathlineTrack>& Pair : Element.Tracks)
		{
			if (!Current.Contains(Pair.Key))
			{
				Candidates.Add({ Pair.Key,
					SelectionScore(ElementName, Pair.Key.Rank, Pair.Key.Id, Settings.SelectionSeed) });
			}
		}

		Candidates.Sort([](const FCandidate& A, const FCandidate& B)
		{
			if (A.Score != B.Score)
			{
				return A.Score > B.Score;
			}
			if (A.Key.Id != B.Key.Id)
			{
				return A.Key.Id < B.Key.Id;
			}
			return A.Key.Rank < B.Key.Rank;
		});

		int32 SelectCount = FMath::Min(FMath::Max(0, Settings.MaxTracks), Candidates.Num());
		if (Settings.MaxNodes > 0)
		{
			const int32 BudgetTracks = Settings.MaxNodes / 2;
			SelectCount = FMath::Min(SelectCount, BudgetTracks);
		}

		TMap<FPathlineKey, FPathlineTrack> NextTracks;
		NextTracks.Reserve(SelectCount);
		for (int32 CandidateIndex = 0; CandidateIndex < SelectCount; ++CandidateIndex)
		{
			const FPathlineKey Key = Candidates[CandidateIndex].Key;
			FPathlineTrack Track;
			if (FPathlineTrack* Existing = Element.Tracks.Find(Key))
			{
				Track = MoveTemp(*Existing);
			}
			Track.Key = Key;

			if (const FSamplePoint* Point = Current.Find(Key))
			{
				const bool bWasPrevious = Element.PreviousKeysByRank.Contains(Rank) &&
					Element.PreviousKeysByRank[Rank].Contains(Key);
				const bool bGap = Track.Nodes.Num() > 0 && !bWasPrevious;

				// Parallel downloads and cached replays hand us timesteps out of
				// order. Appending by arrival left the history unsorted, and both
				// the display cutoff walk and the run splitting assume ascending
				// sample ids - that is what made tails jump and ribbons morph badly.
				int32 InsertAt = 0;
				{
					int32 Lo = 0;
					int32 Hi = Track.Nodes.Num();
					while (Lo < Hi)
					{
						const int32 Mid = (Lo + Hi) / 2;
						if (Track.Nodes[Mid].SampleId < SampleId)
						{
							Lo = Mid + 1;
						}
						else
						{
							Hi = Mid;
						}
					}
					InsertAt = Lo;
				}

				FPathlineNode Node;
				Node.Position = Point->Position;
				Node.Scalar = Point->Scalar;
				Node.Width = Point->Width;
				Node.bHasScalar = Point->bHasScalar;
				Node.Color = Point->Color;
				Node.bHasColor = Point->bHasColor;
				Node.SampleId = SampleId;
				// Only a node appended after everything else can start a new run;
				// a node slotted between two known samples continues the track.
				Node.bBreak = InsertAt >= Track.Nodes.Num() && bGap;

				if (Track.Nodes.IsValidIndex(InsertAt) &&
					Track.Nodes[InsertAt].SampleId == SampleId)
				{
					const bool bKeepBreak = InsertAt == Track.Nodes.Num() - 1 &&
						(Track.Nodes[InsertAt].bBreak || bGap);
					Track.Nodes[InsertAt] = Node;
					Track.Nodes[InsertAt].bBreak = bKeepBreak;
				}
				else
				{
					Track.Nodes.Insert(Node, InsertAt);
				}
				Track.LastSeenCounter = ThisCounter;
			}

			NextTracks.Add(Key, MoveTemp(Track));
		}
		Element.Tracks = MoveTemp(NextTracks);

		int32 EffectiveHistory = FMath::Max(2, FMath::Max(Settings.HistorySamples, Settings.RetentionSamples));
		if (Settings.MaxNodes > 0 && Element.Tracks.Num() > 0)
		{
			EffectiveHistory = FMath::Clamp(
				Settings.MaxNodes / Element.Tracks.Num(), 2, EffectiveHistory);
		}
		for (TPair<FPathlineKey, FPathlineTrack>& Pair : Element.Tracks)
		{
			if (Pair.Value.Nodes.Num() > EffectiveHistory)
			{
				Pair.Value.Nodes.RemoveAt(0,
					Pair.Value.Nodes.Num() - EffectiveHistory, EAllowShrinking::No);
			}
		}

		if (Settings.MaxNodes > 0)
		{
			int64 TotalNodes = 0;
			for (const TPair<FPathlineKey, FPathlineTrack>& Pair : Element.Tracks)
			{
				TotalNodes += Pair.Value.Nodes.Num();
			}
			while (TotalNodes > Settings.MaxNodes)
			{
				FPathlineTrack* Victim = nullptr;
				for (TPair<FPathlineKey, FPathlineTrack>& Pair : Element.Tracks)
				{
					if (!Victim || Pair.Value.Nodes.Num() > Victim->Nodes.Num())
					{
						Victim = &Pair.Value;
					}
				}
				if (!Victim || Victim->Nodes.Num() <= 1)
				{
					break;
				}
				Victim->Nodes.RemoveAt(0, 1, EAllowShrinking::No);
				--TotalNodes;
			}
		}

		TSet<FPathlineKey>& RankPreviousKeys = Element.PreviousKeysByRank.FindOrAdd(Rank);
		RankPreviousKeys.Reset();
		for (const TPair<FPathlineKey, FSamplePoint>& Pair : Current)
		{
			RankPreviousKeys.Add(Pair.Key);
		}

		LiveHeadSampleId = FMath::Max(LiveHeadSampleId, SampleId);

		RefreshTotalsLocked();
		Stats.LastIngestMs = (FPlatformTime::Seconds() - StartSeconds) * 1000.0;
	}

	bool BuildArraysLocked(const FString& ElementName, uint64 CutoffSampleId,
		FJUSYNCPathlineArrays& OutArrays, int32& OutVisibleTracks)
	{
		const double StartSeconds = FPlatformTime::Seconds();
		OutArrays.Reset();
		OutVisibleTracks = 0;

		FElementState* Element = Elements.Find(ElementName);
		if (!Element || !Settings.bEnabled)
		{
			Stats.LastBuildMs = (FPlatformTime::Seconds() - StartSeconds) * 1000.0;
			return false;
		}

		const float BaseRadius = FMath::Max(0.001f, Settings.Radius * Settings.RadiusScale);
		const float ColorRange = Settings.ColorMax - Settings.ColorMin;
		const float BlendAlpha = FMath::Clamp(TipBlendAlpha, 0.0f, 1.0f);
		const double NowSeconds = FPlatformTime::Seconds();

		// Interpolation window between the sample that was displayed and the one
		// being shown now. Only the nodes between the two cutoffs move, so a step
		// forward grows the trail out of its old tip and a step back retracts it,
		// instead of the newest segment popping in one direction only.
		// Callers already hold StateLock.
		uint64 BlendFromCutoff = CutoffSampleId;
		if (BlendAlpha > 0.0f && TipBlendSeconds > 0.0f &&
			BlendFromSampleId != TNumericLimits<uint64>::Max() &&
			BlendFromSampleId != CutoffSampleId)
		{
			BlendFromCutoff = BlendFromSampleId;
		}

		struct FCutoffBlend
		{
			float Alpha = 0.0f;
			uint64 LowCutoff = 0;
			uint64 DisplayCutoff = 0;
			FVector FromTip = FVector::ZeroVector;
			FVector ToTip = FVector::ZeroVector;
		};

		// One shaded vertex. The band is evaluated on these so it can be sampled
		// finer than the history points allow.
		struct FRunSample
		{
			FVector Position = FVector::ZeroVector;
			FLinearColor Color = FLinearColor::White;
			float Width = 0.0f;
			float Scalar = 0.0f;
			float ArcNorm = 0.0f;
		};

		// Refreshed per track, consumed by EmitRun.
		FCutoffBlend TrackBlend;
		float TrackReveal = 1.0f;
		float TrackIdleFade = 1.0f;
		float TrackSpeedRef = 1.0f;

		TArray<const FPathlineNode*> Visible;
		Visible.Reserve(Settings.HistorySamples + 4);
		TArray<FRunSample> RunSamples;

		float Order = 0.0f;
		bool bHasPreviousRun = false;
		FVector PreviousPosition = FVector::ZeroVector;
		FLinearColor PreviousColor = FLinearColor::White;

		auto ResolvePoint = [&](const FPathlineNode& Node, float& OutWidth,
			FLinearColor& OutColor, float& OutScalar)
		{
			float WidthScale = 1.0f;
			if (Settings.bUsePointWidth && Node.Width > 0.0f)
			{
				WidthScale = FMath::Clamp(Node.Width, 0.1f, 8.0f);
			}
			OutWidth = FMath::Max(BaseRadius * 2.0f * WidthScale, 0.01f);

			OutScalar = 0.0f;
			if (Node.bHasScalar && !FMath::IsNearlyZero(ColorRange))
			{
				OutScalar = FMath::Clamp(
					(Node.Scalar - Settings.ColorMin) / ColorRange, 0.0f, 1.0f);
			}

			OutColor = FLinearColor(ResolveColor(*Element, Settings, Node));
		};

		auto EmitPoint = [&](const FVector& Position, float Width,
			const FLinearColor& Color, float Scalar)
		{
			OutArrays.Positions.Add(Position);
			OutArrays.LinkOrders.Add(Order);
			OutArrays.Widths.Add(Width);
			OutArrays.Colors.Add(Color);
			OutArrays.Scalars.Add(Scalar);
			Order += 1.0f;
		};

		auto EmitRun = [&](int32 StartIndex, int32 EndIndex,
			const FCutoffBlend& Blend)
		{
			if (EndIndex <= StartIndex)
			{
				return;
			}

			float FirstWidth = 0.0f;
			float FirstScalar = 0.0f;
			FLinearColor FirstColor = FLinearColor::White;
			ResolvePoint(*Visible[StartIndex], FirstWidth, FirstColor, FirstScalar);

			if (bHasPreviousRun)
			{
				EmitPoint(PreviousPosition, 0.0f, PreviousColor, 0.0f);
				EmitPoint(Visible[StartIndex]->Position, 0.0f, FirstColor, 0.0f);
			}

			RunSamples.Reset();
			RunSamples.Reserve((EndIndex - StartIndex + 1) * Settings.SegmentSubdiv);

			for (int32 NodeIndex = StartIndex; NodeIndex <= EndIndex; ++NodeIndex)
			{
				const FPathlineNode& Node = *Visible[NodeIndex];
				float Width = 0.0f;
				float Scalar = 0.0f;
				FLinearColor Color = FLinearColor::White;
				ResolvePoint(Node, Width, Color, Scalar);

				FVector Position = Node.Position;
				float BlendFade = 1.0f;
				if (Blend.Alpha > 0.0f && Node.SampleId > Blend.LowCutoff)
				{
					if (Node.SampleId <= Blend.DisplayCutoff)
					{
						// Newly shown: grows out of where the tip used to be.
						Position = FMath::Lerp(Blend.FromTip, Position, Blend.Alpha);
					}
					else
					{
						// No longer shown: collapses into the new tip and fades,
						// so stepping back retracts instead of snapping.
						Position = FMath::Lerp(Position, Blend.ToTip, Blend.Alpha);
						BlendFade = 1.0f - Blend.Alpha;
					}
				}

				// 0 at the oldest point, 1 at the tip.
				const float IndexNorm = Visible.Num() > 1
					? static_cast<float>(NodeIndex) / static_cast<float>(Visible.Num() - 1)
					: 1.0f;

				// The reveal front sweeps tail to tip: points behind it are fully
				// drawn, points ahead have zero width, and the front itself is
				// brightened so a growing trail reads as a stroke being drawn.
				float RevealMask = 1.0f;
				float TipGlow = 1.0f;
				if (TrackReveal < 1.0f)
				{
					RevealMask = FMath::Clamp((TrackReveal - IndexNorm) / 0.25f, 0.0f, 1.0f);
					TipGlow = 1.0f + 6.0f * RevealMask * (1.0f - RevealMask);
				}

				const FVector& PrevPos = NodeIndex > 0
					? Visible[NodeIndex - 1]->Position : Node.Position;
				const FVector& NextPos = NodeIndex + 1 < Visible.Num()
					? Visible[NodeIndex + 1]->Position : Node.Position;
				const float SpeedNorm = FMath::Clamp(
					static_cast<float>(FVector::Dist(PrevPos, Node.Position)) / TrackSpeedRef,
					0.0f, 1.0f);
				const float CornerNorm = FMath::Clamp(
					(1.0f - FVector::DotProduct(
						(Node.Position - PrevPos).GetSafeNormal(),
						(NextPos - Node.Position).GetSafeNormal())) * 0.5f,
					0.0f, 1.0f);

				Width *= FMath::Lerp(1.0f, Settings.SpeedWidthScale, SpeedNorm);
				Width *= FMath::Lerp(Settings.TailTaper, 1.0f, IndexNorm);
				Width *= RevealMask * TrackIdleFade * BlendFade;
				Color *= (1.0f + Settings.CornerBoost * CornerNorm) *
					TipGlow * (0.35f + 0.65f * RevealMask) * TrackIdleFade * BlendFade;

				FRunSample Sample;
				Sample.Position = Position;
				Sample.Color = Color;
				Sample.Width = Width;
				Sample.Scalar = Scalar;
				RunSamples.Add(Sample);
			}

			// Measured along the drawn run rather than by node index, so the band
			// keeps an even speed where samples are unevenly spaced.
			float RunLength = 0.0f;
			for (int32 SampleIndex = 1; SampleIndex < RunSamples.Num(); ++SampleIndex)
			{
				RunLength += static_cast<float>(FVector::Dist(
					RunSamples[SampleIndex - 1].Position, RunSamples[SampleIndex].Position));
			}
			const float InvRunLength = RunLength > 1e-4f ? 1.0f / RunLength : 0.0f;
			float Travelled = 0.0f;
			for (int32 SampleIndex = 0; SampleIndex < RunSamples.Num(); ++SampleIndex)
			{
				if (SampleIndex > 0)
				{
					Travelled += static_cast<float>(FVector::Dist(
						RunSamples[SampleIndex - 1].Position, RunSamples[SampleIndex].Position));
				}
				RunSamples[SampleIndex].ArcNorm = FMath::Clamp(Travelled * InvRunLength, 0.0f, 1.0f);
			}

			auto ShadePoint = [&](const FRunSample& Sample)
			{
				float Width = Sample.Width;
				FLinearColor Color = Sample.Color;
				if (Settings.FlowStrength > 0.0f && Settings.FlowSpeed != 0.0f)
				{
					// The head of the band sits where the phase wraps, so it runs
					// towards the newest sample. Phase is kept in double precision:
					// at runtime the seconds value is far too large for a float to
					// resolve a sub-frame step.
					const double Phase = static_cast<double>(Sample.ArcNorm) *
						static_cast<double>(Settings.FlowCycles) -
						NowSeconds * static_cast<double>(Settings.FlowSpeed);
					const float Cycle =
						static_cast<float>(Phase - FMath::FloorToDouble(Phase));
					const float Band = FMath::Pow(
						Cycle, FMath::Max(Settings.FlowSharpness, 0.01f));
					Color = FMath::Lerp(Color,
						Color * (1.0f + Settings.FlowStrength * Band), Band);

					if (Settings.CometArrowWidth > 0.0f)
					{
						// Widest a little behind the head and converging to a point at
						// the head itself, so the front reads as an arrow head rather
						// than as a blob at the end of the band.
						const float Span = FMath::Max(Settings.CometArrowSpan, 0.005f);
						const float Behind = (1.0f - Cycle) / Span;
						const float Bulge = Behind * FMath::Exp(1.0f - Behind);
						Width *= 1.0f + Settings.CometArrowWidth * Bulge * Band;
					}
				}
				EmitPoint(Sample.Position, Width, Color, Sample.Scalar);
			};

			const int32 Subdiv = FMath::Clamp(Settings.SegmentSubdiv, 1, 16);
			const bool bResample = Subdiv > 1 && Settings.FlowStrength > 0.0f;

			if (!bResample)
			{
				for (const FRunSample& Sample : RunSamples)
				{
					ShadePoint(Sample);
				}
			}
			else
			{
				for (int32 SampleIndex = 1; SampleIndex < RunSamples.Num(); ++SampleIndex)
				{
					const FRunSample& A = RunSamples[SampleIndex - 1];
					const FRunSample& B = RunSamples[SampleIndex];
					for (int32 Step = 0; Step < Subdiv; ++Step)
					{
						const float T = static_cast<float>(Step) / static_cast<float>(Subdiv);
						FRunSample Sample;
						Sample.Position = FMath::Lerp(A.Position, B.Position, T);
						Sample.Color = FMath::Lerp(A.Color, B.Color, T);
						Sample.Width = FMath::Lerp(A.Width, B.Width, T);
						Sample.Scalar = FMath::Lerp(A.Scalar, B.Scalar, T);
						Sample.ArcNorm = FMath::Lerp(A.ArcNorm, B.ArcNorm, T);
						ShadePoint(Sample);
					}
				}
				const FRunSample Last = RunSamples.Last();
				ShadePoint(Last);
			}

			// Kept blended so a following run starts from the tip as drawn.
			PreviousPosition = RunSamples.Last().Position;
			PreviousColor = RunSamples.Last().Color;
			bHasPreviousRun = true;
		};

		for (TPair<FPathlineKey, FPathlineTrack>& TrackPair : Element->Tracks)
		{
			FPathlineTrack& Track = TrackPair.Value;
			Visible.Reset();
			for (const FPathlineNode& Node : Track.Nodes)
			{
				if (Node.SampleId <= CutoffSampleId)
				{
					Visible.Add(&Node);
				}
				else if (BlendFromCutoff > CutoffSampleId && !Node.bBreak &&
					Node.SampleId <= BlendFromCutoff)
				{
					// Stepping back: keep the nodes that just left the window so
					// they can retract into the new tip instead of vanishing.
					Visible.Add(&Node);
				}
				else
				{
					break;
				}
			}
			if (Visible.Num() > Settings.HistorySamples)
			{
				Visible.RemoveAt(0, Visible.Num() - Settings.HistorySamples);
			}
			if (Visible.Num() == 0)
			{
				continue;
			}

			TrackBlend = FCutoffBlend();
			if (BlendFromCutoff != CutoffSampleId)
			{
				const FPathlineNode* FromTip = nullptr;
				const FPathlineNode* ToTip = nullptr;
				const uint64 MaxCutoff = FMath::Max(BlendFromCutoff, CutoffSampleId);
				for (const FPathlineNode& Node : Track.Nodes)
				{
					if (Node.SampleId > MaxCutoff)
					{
						break;
					}
					if (Node.SampleId <= BlendFromCutoff)
					{
						FromTip = &Node;
					}
					if (Node.SampleId <= CutoffSampleId)
					{
						ToTip = &Node;
					}
				}
				if (FromTip && ToTip)
				{
					TrackBlend.Alpha = BlendAlpha;
					TrackBlend.DisplayCutoff = CutoffSampleId;
					TrackBlend.LowCutoff = FMath::Min(BlendFromCutoff, CutoffSampleId);
					TrackBlend.FromTip = FromTip->Position;
					TrackBlend.ToTip = ToTip->Position;
				}
			}

			if (Track.FirstSeenSeconds <= 0.0)
			{
				Track.FirstSeenSeconds = NowSeconds;
			}
			const float StaggerPhase =
				static_cast<float>(GetTypeHash(Track.Key) % 1000u) / 1000.0f;
			const float RevealElapsed = static_cast<float>(NowSeconds -
				Track.FirstSeenSeconds - StaggerPhase * Settings.RevealStaggerSeconds);
			TrackReveal = Settings.RevealSeconds > 0.0f
				? FMath::Clamp(RevealElapsed / Settings.RevealSeconds, 0.0f, 1.0f)
				: 1.0f;

			TrackIdleFade = 1.0f;
			if (Settings.TrackExpirySamples > 0)
			{
				const float Age = static_cast<float>(
					Element->SampleCounter - Track.LastSeenCounter);
				const float FadeSpan = FMath::Min(
					static_cast<float>(Settings.TrackExpirySamples) * 0.5f, 8.0f);
				TrackIdleFade = FMath::Clamp(
					(static_cast<float>(Settings.TrackExpirySamples) - Age) /
					FMath::Max(1.0f, FadeSpan), 0.0f, 1.0f);
			}

			// Normalising by this track's own longest segment keeps the speed
			// response independent of world scale and sample cadence.
			float MaxSegment = 0.0f;
			for (int32 NodeIndex = 1; NodeIndex < Visible.Num(); ++NodeIndex)
			{
				MaxSegment = FMath::Max(MaxSegment, static_cast<float>(FVector::Dist(
					Visible[NodeIndex - 1]->Position, Visible[NodeIndex]->Position)));
			}
			TrackSpeedRef = FMath::Max(MaxSegment, 1e-3f);

			// Retraction nodes sit past the cutoff; filters still describe what is
			// actually shown at this sample.
			int32 LatestIndex = Visible.Num() - 1;
			while (LatestIndex > 0 && Visible[LatestIndex]->SampleId > CutoffSampleId)
			{
				--LatestIndex;
			}
			const FPathlineNode& Latest = *Visible[LatestIndex];
			if (Settings.bFilterEnabled)
			{
				if (!Latest.bHasScalar ||
					Latest.Scalar < Settings.FilterMin ||
					Latest.Scalar > Settings.FilterMax)
				{
					continue;
				}
			}
			if (Settings.bFilterByHue)
			{
				float Hue = 0.0f;
				float Saturation = 0.0f;
				RgbToHS(FLinearColor(ResolveColor(*Element, Settings, Latest)), Hue, Saturation);
				if (HueDistance(Hue, Settings.FilterHue) > Settings.FilterHueTolerance)
				{
					continue;
				}
			}

			++OutVisibleTracks;
			if (Visible.Num() < 2)
			{
				continue;
			}

			int32 RunStart = 0;
			for (int32 VisibleIndex = 1; VisibleIndex < Visible.Num(); ++VisibleIndex)
			{
				if (Visible[VisibleIndex]->bBreak)
				{
					EmitRun(RunStart, VisibleIndex - 1, TrackBlend);
					RunStart = VisibleIndex;
				}
			}
			EmitRun(RunStart, Visible.Num() - 1, TrackBlend);
		}

		Stats.VisibleTracks = OutVisibleTracks;
		Stats.LastBuildMs = (FPlatformTime::Seconds() - StartSeconds) * 1000.0;
		return true;
	}

	void PostArrays(const FString& ElementName, FJUSYNCPathlineArrays&& Arrays,
		uint64 ItemGeneration)
	{
		if (ItemGeneration != Generation.load())
		{
			return;
		}

		TSharedPtr<FJUSYNCPathlineArrays, ESPMode::ThreadSafe> Shared =
			MakeShared<FJUSYNCPathlineArrays, ESPMode::ThreadSafe>();
		*Shared = MoveTemp(Arrays);

		FScopeLock Lock(&PendingLock);
		if (ItemGeneration != Generation.load())
		{
			return;
		}
		FPendingArrays Pending;
		Pending.ElementName = ElementName;
		Pending.Arrays = Shared;
		Pending.Generation = ItemGeneration;
		PendingArrays.Add(MoveTemp(Pending));
	}

	void BuildElementAndPost(const FString& ElementName, uint64 ItemGeneration)
	{
		FJUSYNCPathlineArrays Arrays;
		int32 VisibleTracks = 0;
		bool bFound = false;
		{
			FScopeLock Lock(&StateLock);
			bFound = BuildArraysLocked(ElementName, DisplaySampleId, Arrays, VisibleTracks);
		}
		if (bFound)
		{
			PostArrays(ElementName, MoveTemp(Arrays), ItemGeneration);
		}
	}

	void RebuildAllAndPost(uint64 ItemGeneration)
	{
		TArray<TPair<FString, FJUSYNCPathlineArrays>> LocalArrays;
		{
			FScopeLock Lock(&StateLock);
			if (!Settings.bEnabled)
			{
				return;
			}
			for (const TPair<FString, FElementState>& Pair : Elements)
			{
				FJUSYNCPathlineArrays Arrays;
				int32 VisibleTracks = 0;
				if (BuildArraysLocked(Pair.Key, DisplaySampleId, Arrays, VisibleTracks))
				{
					LocalArrays.Emplace(Pair.Key, MoveTemp(Arrays));
				}
			}
		}

		for (TPair<FString, FJUSYNCPathlineArrays>& Pair : LocalArrays)
		{
			PostArrays(Pair.Key, MoveTemp(Pair.Value), ItemGeneration);
		}
	}

	void ProcessItem(const TSharedPtr<FWorkItem>& Item)
	{
		if (!Item || Item->Generation != Generation.load())
		{
			return;
		}

		if (Item->Type == FWorkItem::EType::RebuildAll)
		{
			bRebuildQueued = false;
			RebuildAllAndPost(Item->Generation);
			return;
		}

		IngestSampleNow(Item->ElementName, Item->Cloud, Item->Rank, Item->SampleId, Item->Lut,
			Item->Generation);
		if (Item->Generation != Generation.load())
		{
			return;
		}
		BuildElementAndPost(Item->ElementName, Item->Generation);
	}

	void IngestSampleNow(const FString& ElementName, const FJUSYNCPointCloudRef& Cloud,
		int32 Rank, uint64 SampleId, const TArray<FColor>& Lut, uint64 ExpectedGeneration)
	{
		FScopeLock Lock(&StateLock);
		if (!Settings.bEnabled || ExpectedGeneration != Generation.load())
		{
			return;
		}
		IngestLocked(ElementName, Cloud, Rank, SampleId, Lut);
	}

	void EnqueueWorkItem(const TSharedPtr<FWorkItem>& Item)
	{
		bool bStartWorker = false;
		{
			FScopeLock Lock(&QueueLock);
			Queue.Add(Item);
			if (!bWorkerRunning.exchange(true))
			{
				bStartWorker = true;
			}
		}
		if (bStartWorker)
		{
			AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [this]()
			{
				WorkerLoop();
			});
		}
	}

	void QueueRebuildAll()
	{
		bool bHasWork = false;
		{
			FScopeLock Lock(&StateLock);
			bHasWork = Settings.bEnabled && Elements.Num() > 0;
		}
		if (!bHasWork)
		{
			return;
		}

		if (bRebuildQueued)
		{
			return;
		}
		bRebuildQueued = true;

		TSharedPtr<FWorkItem> Item = MakeShared<FWorkItem>();
		Item->Type = FWorkItem::EType::RebuildAll;
		Item->Generation = Generation.load();
		EnqueueWorkItem(Item);
	}

	void WorkerLoop()
	{
		for (;;)
		{
			TSharedPtr<FWorkItem> Item;
			{
				FScopeLock Lock(&QueueLock);
				if (bShuttingDown.load() || Queue.Num() == 0)
				{
					bWorkerRunning.store(false);
					break;
				}
				Item = Queue[0];
				Queue.RemoveAt(0, 1, EAllowShrinking::No);
			}
			ProcessItem(Item);
		}
	}
};

FJUSYNCPathlineManager::FJUSYNCPathlineManager()
	: Impl(MakeUnique<FImpl>())
{
}

FJUSYNCPathlineManager::~FJUSYNCPathlineManager()
{
	Impl->bShuttingDown.store(true);
	{
		FScopeLock Lock(&Impl->QueueLock);
		Impl->Queue.Reset();
	}
	while (Impl->bWorkerRunning.load())
	{
		FPlatformProcess::Sleep(0.001f);
	}
}

void FJUSYNCPathlineManager::SetSettings(const FJUSYNCPathlineSettings& NewSettings)
{
	bool bRebuild = false;
	{
		FScopeLock Lock(&Impl->StateLock);
		const bool bWasEnabled = Impl->Settings.bEnabled;
		const bool bStructural =
			NewSettings.IdAttribute != Impl->Settings.IdAttribute ||
			NewSettings.ScalarAttribute != Impl->Settings.ScalarAttribute ||
			NewSettings.ScalarComponent != Impl->Settings.ScalarComponent ||
			NewSettings.MaxTracks != Impl->Settings.MaxTracks ||
			NewSettings.HistorySamples != Impl->Settings.HistorySamples ||
			NewSettings.MaxNodes != Impl->Settings.MaxNodes ||
			NewSettings.TrackExpirySamples != Impl->Settings.TrackExpirySamples ||
			NewSettings.SelectionSeed != Impl->Settings.SelectionSeed;
		const bool bVisual =
			NewSettings.NiagaraSystem != Impl->Settings.NiagaraSystem ||
			NewSettings.Radius != Impl->Settings.Radius ||
			NewSettings.RadiusScale != Impl->Settings.RadiusScale ||
			NewSettings.bUsePointWidth != Impl->Settings.bUsePointWidth ||
			NewSettings.bFilterEnabled != Impl->Settings.bFilterEnabled ||
			NewSettings.FilterMin != Impl->Settings.FilterMin ||
			NewSettings.FilterMax != Impl->Settings.FilterMax ||
			NewSettings.bFilterByHue != Impl->Settings.bFilterByHue ||
			NewSettings.FilterHue != Impl->Settings.FilterHue ||
			NewSettings.FilterHueTolerance != Impl->Settings.FilterHueTolerance ||
			NewSettings.bColorByScalar != Impl->Settings.bColorByScalar ||
			NewSettings.ColorMin != Impl->Settings.ColorMin ||
			NewSettings.ColorMax != Impl->Settings.ColorMax ||
			NewSettings.FlatColor != Impl->Settings.FlatColor ||
			NewSettings.FlowSpeed != Impl->Settings.FlowSpeed ||
			NewSettings.FlowCycles != Impl->Settings.FlowCycles ||
			NewSettings.FlowSharpness != Impl->Settings.FlowSharpness ||
			NewSettings.FlowStrength != Impl->Settings.FlowStrength ||
			NewSettings.SegmentSubdiv != Impl->Settings.SegmentSubdiv ||
			NewSettings.CometArrowWidth != Impl->Settings.CometArrowWidth ||
			NewSettings.CometArrowSpan != Impl->Settings.CometArrowSpan ||
			NewSettings.Material != Impl->Settings.Material;

		Impl->Settings = NewSettings;

		if (!NewSettings.bEnabled)
		{
			if (bWasEnabled || Impl->Elements.Num() > 0)
			{
				Impl->ClearLocked();
			}
			Impl->bResampleRequested = false;
		}
		else if (!bWasEnabled || bStructural)
		{
			Impl->ClearLocked();
			Impl->bResampleRequested = true;
			bRebuild = Impl->Elements.Num() > 0;
		}
		else if (bVisual)
		{
			bRebuild = Impl->Elements.Num() > 0;
		}
	}

	if (bRebuild)
	{
		Impl->QueueRebuildAll();
	}
}

const FJUSYNCPathlineSettings& FJUSYNCPathlineManager::GetSettings() const
{
	return Impl->Settings;
}

bool FJUSYNCPathlineManager::ConsumeResampleRequest()
{
	FScopeLock Lock(&Impl->StateLock);
	const bool bRequested = Impl->bResampleRequested;
	Impl->bResampleRequested = false;
	return bRequested;
}

void FJUSYNCPathlineManager::Reset()
{
	{
		FScopeLock Lock(&Impl->StateLock);
		Impl->ClearLocked();
		Impl->bResampleRequested = false;
	}
	{
		FScopeLock Lock(&Impl->QueueLock);
		Impl->Queue.Reset();
	}
	{
		FScopeLock Lock(&Impl->PendingLock);
		Impl->PendingArrays.Reset();
	}
}

void FJUSYNCPathlineManager::RemoveElement(const FString& ElementName)
{
	bool bRemoved = false;
	{
		FScopeLock Lock(&Impl->StateLock);
		bRemoved = Impl->Elements.Remove(ElementName) > 0;
		if (bRemoved)
		{
			Impl->RefreshTotalsLocked();
			Impl->Generation.fetch_add(1);
		}
	}
	if (bRemoved)
	{
		Impl->QueueRebuildAll();
	}
}

void FJUSYNCPathlineManager::EnqueueSample(const FString& ElementName,
	const FJUSYNCPointCloudRef& Cloud, int32 Rank, uint64 SampleId, const TArray<FColor>& Lut)
{
	if (!Cloud.IsValid())
	{
		FScopeLock Lock(&Impl->StateLock);
		++Impl->Stats.IgnoredSamples;
		return;
	}

	TSharedPtr<FWorkItem> Item = MakeShared<FWorkItem>();
	{
		FScopeLock Lock(&Impl->StateLock);
		if (!Impl->Settings.bEnabled)
		{
			return;
		}
		Item->Generation = Impl->Generation.load();
	}
	Item->Type = FWorkItem::EType::Sample;
	Item->ElementName = ElementName;
	Item->Cloud = Cloud;
	Item->Rank = Rank;
	Item->SampleId = SampleId;
	Item->Lut = Lut;

	bool bStartWorker = false;
	bool bDropped = false;
	{
		FScopeLock Lock(&Impl->QueueLock);
		if (Impl->Queue.Num() >= 64)
		{
			// Drop the oldest queued timestep, not the incoming one: the older
			// sample is already superseded and discarding the newest made the
			// displayed ribbons lag behind the stream under a fast sender.
			Impl->Queue.RemoveAt(0, 1, EAllowShrinking::No);
			bDropped = true;
		}
		Impl->Queue.Add(Item);
		if (!Impl->bWorkerRunning.exchange(true))
		{
			bStartWorker = true;
		}
	}

	if (bDropped)
	{
		// A single skipped timestep does not mean the tracks broke - the next
		// sample still continues them. Marking a break here (and previously on
		// every element, not just this one) fragmented continuous tracks and is
		// what littered the viewport with short ribbons.
		FScopeLock Lock(&Impl->StateLock);
		++Impl->Stats.DroppedSamples;
	}

	if (bStartWorker)
	{
		AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [this]()
		{
			Impl->WorkerLoop();
		});
	}
}

void FJUSYNCPathlineManager::IngestSampleNow(const FString& ElementName,
	const FJUSYNCPointCloudRef& Cloud, int32 Rank, uint64 SampleId, const TArray<FColor>& Lut)
{
	Impl->IngestSampleNow(ElementName, Cloud, Rank, SampleId, Lut,
		Impl->Generation.load());
}

bool FJUSYNCPathlineManager::BuildArrays(const FString& ElementName,
	uint64 CutoffSampleId, FJUSYNCPathlineArrays& OutArrays, int32& OutVisibleTracks)
{
	FScopeLock Lock(&Impl->StateLock);
	return Impl->BuildArraysLocked(ElementName, CutoffSampleId, OutArrays, OutVisibleTracks);
}

void FJUSYNCPathlineManager::SetDisplaySampleId(uint64 SampleId)
{
	bool bChanged = false;
	{
		FScopeLock Lock(&Impl->StateLock);
		if (Impl->DisplaySampleId != SampleId)
		{
			const bool bContinueBlend = Impl->bTipBlendActive &&
				Impl->TipBlendAlpha > 0.0f && Impl->TipBlendAlpha < 1.0f &&
				Impl->BlendFromSampleId != TNumericLimits<uint64>::Max() &&
				SampleId != TNumericLimits<uint64>::Max();
			if (!bContinueBlend)
			{
				Impl->BlendFromSampleId = Impl->DisplaySampleId != TNumericLimits<uint64>::Max()
					? Impl->DisplaySampleId
					: Impl->LiveHeadSampleId;
				Impl->TipBlendAlpha = 0.0f;
				Impl->TipBlendStartSeconds = FPlatformTime::Seconds();
			}
			Impl->DisplaySampleId = SampleId;
			Impl->bTipBlendActive =
				Impl->TipBlendSeconds > 0.0f && SampleId != TNumericLimits<uint64>::Max();
			bChanged = true;
		}
	}
	if (bChanged)
	{
		Impl->QueueRebuildAll();
	}
}

uint64 FJUSYNCPathlineManager::GetDisplaySampleId() const
{
	FScopeLock Lock(&Impl->StateLock);
	return Impl->DisplaySampleId;
}

void FJUSYNCPathlineManager::SetDisplayTipBlendSeconds(float Seconds)
{
	FScopeLock Lock(&Impl->StateLock);
	Impl->TipBlendSeconds = FMath::Clamp(Seconds, 0.0f, 4.0f);
	if (Impl->TipBlendSeconds <= 0.0f)
	{
		Impl->bTipBlendActive = false;
		Impl->TipBlendAlpha = 0.0f;
	}
}

void FJUSYNCPathlineManager::TickDisplayBlend()
{
	bool bNeedsRebuild = false;
	{
		FScopeLock Lock(&Impl->StateLock);
		if (Impl->bTipBlendActive)
		{
			const float Elapsed = static_cast<float>(FPlatformTime::Seconds() - Impl->TipBlendStartSeconds);
			const float U = FMath::Clamp(Elapsed / FMath::Max(Impl->TipBlendSeconds, 0.001f), 0.0f, 1.0f);
			const float Alpha = U * U * (3.0f - 2.0f * U);
			if (Alpha >= 1.0f)
			{
				Impl->TipBlendAlpha = 1.0f;
				Impl->bTipBlendActive = false;
				bNeedsRebuild = true;
			}
			else if (!FMath::IsNearlyEqual(Alpha, Impl->TipBlendAlpha, 0.004f))
			{
				Impl->TipBlendAlpha = Alpha;
				bNeedsRebuild = true;
			}
		}

		// The band is evaluated from wall time inside the build, so it only moves
		// when the arrays are re-emitted. Without this tick a static dataset shows
		// a frozen gradient and an idle sender shows nothing at all.
		if (Impl->Settings.bEnabled && Impl->Settings.FlowStrength > 0.0f &&
			Impl->Settings.FlowSpeed != 0.0f && Impl->Elements.Num() > 0)
		{
			const double Now = FPlatformTime::Seconds();
			if (Now - Impl->LastFlowRebuildSeconds >= FlowRebuildIntervalSeconds)
			{
				Impl->LastFlowRebuildSeconds = Now;
				bNeedsRebuild = true;
			}
		}
	}
	if (bNeedsRebuild)
	{
		Impl->QueueRebuildAll();
	}
}

void FJUSYNCPathlineManager::RequestRebuild()
{
	Impl->QueueRebuildAll();
}

void FJUSYNCPathlineManager::DrainArrays(TArray<FCompletedArrays>& OutArrays)
{
	FScopeLock Lock(&Impl->PendingLock);
	OutArrays.Reset();
	OutArrays.Reserve(Impl->PendingArrays.Num());

	const uint64 CurrentGeneration = Impl->Generation.load();
	for (FPendingArrays& Pending : Impl->PendingArrays)
	{
		if (Pending.Generation != CurrentGeneration || !Pending.Arrays.IsValid())
		{
			continue;
		}
		FCompletedArrays Completed;
		Completed.ElementName = MoveTemp(Pending.ElementName);
		Completed.Arrays = MoveTemp(Pending.Arrays);
		OutArrays.Add(MoveTemp(Completed));
	}
	Impl->PendingArrays.Reset();
}

FJUSYNCPathlineStats FJUSYNCPathlineManager::GetStats() const
{
	FScopeLock Lock(&Impl->StateLock);
	FJUSYNCPathlineStats Out = Impl->Stats;
	Out.ActiveTracks = 0;
	Out.TotalNodes = 0;
	for (const TPair<FString, FElementState>& ElementPair : Impl->Elements)
	{
		Out.ActiveTracks += ElementPair.Value.Tracks.Num();
		for (const TPair<FPathlineKey, FPathlineTrack>& TrackPair : ElementPair.Value.Tracks)
		{
			Out.TotalNodes += TrackPair.Value.Nodes.Num();
		}
	}
	return Out;
}

TArray<FString> FJUSYNCPathlineManager::GetElementKeys() const
{
	FScopeLock Lock(&Impl->StateLock);
	TArray<FString> Keys;
	Impl->Elements.GetKeys(Keys);
	return Keys;
}

TArray<uint64> FJUSYNCPathlineManager::GetTrackIds(const FString& ElementName, int32 Rank) const
{
	FScopeLock Lock(&Impl->StateLock);
	TArray<uint64> Ids;
	if (const FElementState* Element = Impl->Elements.Find(ElementName))
	{
		Ids.Reserve(Element->Tracks.Num());
		for (const TPair<FPathlineKey, FPathlineTrack>& Pair : Element->Tracks)
		{
			if (Pair.Key.Rank == Rank)
			{
				Ids.Add(Pair.Key.Id);
			}
		}
	}
	Ids.Sort();
	return Ids;
}

int32 FJUSYNCPathlineManager::GetElementNodeCount(const FString& ElementName) const
{
	FScopeLock Lock(&Impl->StateLock);
	int32 Count = 0;
	if (const FElementState* Element = Impl->Elements.Find(ElementName))
	{
		for (const TPair<FPathlineKey, FPathlineTrack>& Pair : Element->Tracks)
		{
			Count += Pair.Value.Nodes.Num();
		}
	}
	return Count;
}

int32 FJUSYNCPathlineManager::GetTrackNodeCount(const FString& ElementName,
	int32 Rank, uint64 Id) const
{
	FScopeLock Lock(&Impl->StateLock);
	if (const FElementState* Element = Impl->Elements.Find(ElementName))
	{
		if (const FPathlineTrack* Track = Element->Tracks.Find(FPathlineKey{ Rank, Id }))
		{
			return Track->Nodes.Num();
		}
	}
	return 0;
}
