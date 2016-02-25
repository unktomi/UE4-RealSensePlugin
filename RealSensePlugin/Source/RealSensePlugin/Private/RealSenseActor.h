#pragma once
#include "Engine.h"
#include "Array.h"
#include "Vector.h"
#include "ProceduralMeshComponent.h"
#include "FrameSource.h"
#include "RealSenseTypes.h"
#include "RealSenseActor.generated.h"


UCLASS(BlueprintType, Blueprintable)
class ARealSenseActor : public AActor
{
	GENERATED_UCLASS_BODY()
public:
	UPROPERTY(Category = "RealSense", EditAnywhere, BlueprintReadOnly)
		EColorResolution ColorResolution;
	UPROPERTY(Category = "RealSense", EditAnywhere, BlueprintReadOnly)
		EDepthResolution DepthResolution;
	UPROPERTY(Category = "RealSense", EditAnywhere, BlueprintReadOnly)
		FString ServerHost;
	UPROPERTY(Category = "RealSense", EditAnywhere, BlueprintReadOnly)
		int32 ServerPort;
	UPROPERTY(Category = "RealSense", EditAnywhere, BlueprintReadOnly)
		FString WebSocketPath;
	UPROPERTY(Category = "RealSense", EditAnywhere, BlueprintReadOnly)
		FString RemoteUser;
	UPROPERTY(Category = "RealSense", EditAnywhere, BlueprintReadOnly)
		FString LocalUser;
	UPROPERTY(Category = "RealSense", EditAnywhere)
		bool bCompressMeshData;
	UPROPERTY(Category = "RealSense", EditAnywhere, BlueprintReadOnly)
		int32 NetworkFrameSizeKb;
	UPROPERTY(Category = "RealSense", EditAnywhere, BlueprintReadOnly)
	class URawAudioSoundWave *AudioOutput;
	UPROPERTY(Category = "RealSense", EditAnywhere)
	class UProceduralMeshComponent *MeshComp;
	UPROPERTY(Category = "RealSense", EditAnywhere, BlueprintReadOnly)
	class URealSenseTexture *Camera;
	UPROPERTY(Category = "RealSense", EditAnywhere)
		float ViewportWidth;
	UPROPERTY(Category = "RealSense", EditAnywhere)
		float ViewportHeight;
	UPROPERTY(Category = "RealSense", EditAnywhere)
		float MinDistanceInMeters;
	UPROPERTY(Category = "RealSense", EditAnywhere)
		float MaxDistanceInMeters;
	UPROPERTY(Category = "RealSense", EditAnywhere)
		float Resolution;
	UPROPERTY(Category = "RealSense", EditAnywhere)
		int32 MaxEdgeLength;
	UPROPERTY(Category = "RealSense", EditAnywhere, BlueprintReadOnly)
		bool EnablePhysics;

	/** Events have to occur on the main thread, so we have this queue to feed the ticker */
	DECLARE_DELEGATE(CommandDelegate)
	TQueue<CommandDelegate, EQueueMode::Mpsc> Commands;

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaSeconds) override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FColor> VertexColors;
	TArray<FProcMeshTangent> Tangents;
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	int32 Channels;
	int32 SampleRate;
	TArray<uint8> AudioData;
	class RealSenseRunner *Runner;
	IFrameSource *FrameSource;
	class FRemoteFrameSource *Receiver;
	class FRemoteFrameSource *Sender;
	TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> CurrentCameraFrame;
	int64 FrameIndex;
	bool bMeshPending;
};
