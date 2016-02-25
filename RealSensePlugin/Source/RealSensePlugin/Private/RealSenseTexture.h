#pragma once
#include "Engine.h"
#include "Engine/Texture.h"
#include "RealSenseTexture.generated.h"

struct FSampleInfo
{
	FIntPoint Dimensions;
	TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> CurrentFrame;
};

UCLASS()
class REALSENSEPLUGIN_API URealSenseTexture : public UTexture
{
	GENERATED_UCLASS_BODY()
public:
	virtual ~URealSenseTexture();
	/** The addressing mode to use for the X axis. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MediaTexture, AssetRegistrySearchable)
		TEnumAsByte<TextureAddress> AddressX;

	/** The addressing mode to use for the Y axis. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MediaTexture, AssetRegistrySearchable)
		TEnumAsByte<TextureAddress> AddressY;

	/** The color used to clear the texture if no video data is drawn. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MediaTexture)
		FLinearColor ClearColor;
	// UObject overrides.

	virtual void BeginDestroy() override;
	virtual void FinishDestroy() override;
	//virtual FString GetDesc() override;
	virtual SIZE_T GetResourceSize(EResourceSizeMode::Type Mode) override;
	virtual bool IsReadyForFinishDestroy() override;

	// UTexture overrides.
	virtual FTextureResource* CreateResource() override;
	virtual EMaterialValueType GetMaterialType() override;
	virtual float GetSurfaceWidth() const override;
	virtual float GetSurfaceHeight() const override;

	TEnumAsByte<EPixelFormat> GetFormat() const
	{
		return PF_B8G8R8A8;
	}

	FIntPoint GetDimensions() const
	{
		return SampleInfo->Dimensions;
	}

	TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> GetCurrentFrame() const
	{
		return SampleInfo->CurrentFrame;
	}

	void SetCurrentFrame(const TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> &InCurrentFrame)
	{
		SampleInfo->CurrentFrame = InCurrentFrame;
	}

	void SetDimensions(const FIntPoint &InDimensions)
	{
		SampleInfo->Dimensions = InDimensions;
	}
private:
	FSampleInfo *SampleInfo;
	/** Synchronizes access to this object from the render thread. */
	FRenderCommandFence* ReleasePlayerFence;
};
