#pragma once
#include "FrameSource.h"
#include "easywsclient.hpp"

class FRemoteFrameSource: public IFrameSource
{
  FString UserId;
  FString RemoteId;
  FString ServerUrl;
  FString ServerHost;
  int32 ServerPort;
  easywsclient::WebSocket::pointer Sock;
  class ReceiveCallback {
    FRemoteFrameSource *Target;
  public:
  ReceiveCallback(FRemoteFrameSource *InTarget) : Target(InTarget) {}
    void operator()(const std::vector<uint8> &data) {
      Target->HandleWSData((void*)data.data(), data.size());
    }
  };
  ReceiveCallback Callback;
  class FDataInput *DataInput;
  class FDataOutput *DataOutput;

  FCriticalSection Crit;
  TArray<FVector> Vertices;
  TArray<FColor> VertexColors;
  TArray<uint16> AudioData;
  int32 Channels;
  int32 SampleRate;
  bool bCompressMeshData;
  int64 ReceivedFrame;

  virtual void HandleWSData(void *Data, int32 Size);
 public:
  FRemoteFrameSource(const FString &ServerHost, int32 ServerPort, const FString &ServerUrl, const FString &UserId, const FString &RemoteId);
  virtual ~FRemoteFrameSource();
  // IFrameSource interface
  virtual void SendFrameData(const TArray<int32> &Triangles,
	  const TArray<FVector> &Vertices,
	  const TArray<FColor> &VertexColors,
	  const TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> &ColorBgra,
	  int32 ColorWidth,
	  int32 ColorHeight,
	  const TArray<uint8> &AudioData,
	  int32 Channels,
	  int32 SampleRate);
  virtual int64 GetFrameData(
					int64 LastFrame,
                    TArray<int32> &Triangles,
                    TArray<FVector> &Vertices,
                    TArray<FColor> &VertexColors,
                    TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> &ColorBgra,
                    int32 &ColorWidth,
                    int32 &ColorHeight,
                    TArray<uint8> &AudioData,
                    int32 &Channels,
                    int32 &SampleRate) override;
  virtual void Tick() override;
  virtual void Close() override;
  virtual bool IsConnected() override;
};
