#include "RealSensePluginPrivatePCH.h"

DEFINE_LOG_CATEGORY_STATIC(LogRealSense, Log, All);


class RealSenseRunner :  public FRunnable, public IFrameSource
{
	FWeakObjectPtr Target;
	FRunnableThread *Thread;
	bool bDeviceFound;

	
	PXCSession *session;
	PXCSenseManager *senseManager;
	PXCCapture *capture;
	PXCCapture::Device *device;

	PXCCapture::DeviceInfo deviceInfo;
	


	bool bRunning;

	FCriticalSection Crit;
	int64 ColorSample;
	int64 DepthSample;
	int64 SentSample;

	// Depth Camera
	int32 DepthWidth;
	int32 DepthHeight;
	TArray<FVector> CameraSpacePoints;
	TArray<FVector2D> UVs;

	// Color camera
	int32 ColorWidth;
	int32 ColorHeight;
	TArray<uint8> Bgra;

	TArray<uint16> DepthBuffer;
	TArray<uint8> ColorBuffer;


	// Audio
	int32 Channels;
	int32 SampleRate;
	TArray<uint8> AudioData;
	// Remote Sender Data
	FRemoteFrameSource *Sender;
	TArray<int32> Triangles;
	TArray<FVector> Vertices;
	TArray<FColor> VertexColors;
	TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> SenderColorBuffer;


	void DoRemoteSend()
	{
		if (Sender != nullptr)
		{
			if (DepthSample == ColorSample && ColorSample > SentSample)
			{
				SentSample = GetFrameData(SentSample,
					Triangles, Vertices, VertexColors,
					SenderColorBuffer, ColorWidth, ColorHeight,
					AudioData, Channels, SampleRate);
				Sender->SendFrameData(Triangles, Vertices, VertexColors, SenderColorBuffer, 0, 0, AudioData, Channels, SampleRate); //@TODO tracking
				AudioData.Reset();
			}
		}
	}

public:

	void SetSender(FRemoteFrameSource *InSender)
	{
		Sender = InSender;
	}

	uint32 Run() override
	{
		pxcStatus status = senseManager->Init();
		if (status != PXC_STATUS_NO_ERROR)
		{
			UE_LOG(LogRealSense, Error, TEXT("SenseManager init failed."));
			return -1;
		}
		session = senseManager->QuerySession();
		device = senseManager->QueryCaptureManager()->QueryDevice();
		if (false)
		{
			// Loop through video capture devices to find a RealSense Camera
			PXCSession::ImplDesc desc1 = {};
			desc1.group = PXCSession::IMPL_GROUP_SENSOR;
			desc1.subgroup = PXCSession::IMPL_SUBGROUP_VIDEO_CAPTURE;
			for (int m = 0; ; m++) {
				if (device)
					break;

				PXCSession::ImplDesc desc2 = {};
				if (session->QueryImpl(&desc1, m, &desc2) != PXC_STATUS_NO_ERROR)
					break;

				PXCCapture* tmp;
				if (session->CreateImpl<PXCCapture>(&desc2, &tmp) != PXC_STATUS_NO_ERROR)
					continue;
				capture = tmp;

				for (int j = 0; ; j++) {
					if (capture->QueryDeviceInfo(j, &deviceInfo) != PXC_STATUS_NO_ERROR)
						break;
					if ((deviceInfo.model == PXCCapture::DeviceModel::DEVICE_MODEL_F200) ||
						(deviceInfo.model == PXCCapture::DeviceModel::DEVICE_MODEL_R200) ||
						(deviceInfo.model == PXCCapture::DeviceModel::DEVICE_MODEL_R200_ENHANCED) ||
						(deviceInfo.model == PXCCapture::DeviceModel::DEVICE_MODEL_SR300)) {
						device = capture->CreateDevice(j);
						if (device) break;
					}
				}
			}
		}
		if (device == nullptr)
		{
			UE_LOG(LogRealSense, Error, TEXT("No RealSense device found."));
			return -1;
		}
		 int32 Result = DoRun();
		 if (senseManager)
		 {
			 senseManager->Release();
			 senseManager = nullptr;
		 }
		 if (device)
		 {
		//	 device->Release();
			 device = nullptr;
		 }
		 if (capture)
		 {
			 capture->Release();
			 capture = nullptr;
		 }
		 
		 return Result;
	}

	void CopyColorImageToBuffer(PXCImage* image, TArray<uint8>& data, const uint32 width, const uint32 height)
	{
		// Extracts the raw data from the PXCImage object.
		PXCImage::ImageData imageData;
		pxcStatus result = image->AcquireAccess(PXCImage::ACCESS_READ, PXCImage::PIXEL_FORMAT_RGB24, &imageData);
		if (result != PXC_STATUS_NO_ERROR) {
			return;
		}

		uint32 i = 0;
		for (uint32 y = 0; y < height; ++y) {
			// color points to one row of color image data.
			const pxcBYTE* color = imageData.planes[0] + (imageData.pitches[0] * y);
			for (uint32 x = 0; x < width; ++x, color += 3) {
				data[i++] = color[0];
				data[i++] = color[1];
				data[i++] = color[2];
				data[i++] = 0xff; // alpha = 255
			}
		}

		image->ReleaseAccess(&imageData);
	}

	void CopyDepthImageToBuffer(PXCImage* image, TArray<uint16>& data, const uint32 width, const uint32 height)
	{
		// Extracts the raw data from the PXCImage object.
		PXCImage::ImageData imageData;
		pxcStatus result = image->AcquireAccess(PXCImage::ACCESS_READ, PXCImage::PIXEL_FORMAT_DEPTH, &imageData);
		if (result != PXC_STATUS_NO_ERROR)
			return;

		const uint32 numBytes = width * sizeof(uint16);

		uint32 i = 0;
		for (uint32 y = 0; y < height; ++y) {
			// depth points to one row of depth image data.
			const pxcBYTE* depth = imageData.planes[0] + (imageData.pitches[0] * y);
			for (uint32 x = 0; x < width; ++x, depth += 2) {
				data[i++] = *depth;
			}
		}

		image->ReleaseAccess(&imageData);
	}

	uint32 DoRun()
	{	
		
		PXCProjection* projection = device->CreateProjection();
		std::vector<PXCPoint3DF32> vertices(DepthWidth * DepthHeight);
		DepthBuffer.SetNumUninitialized(DepthWidth * DepthHeight);
		ColorBuffer.Reset(DepthWidth * DepthHeight);
		Bgra.SetNumUninitialized(ColorWidth * ColorHeight * 4);
		PXCAudio::AudioInfo ainfo;
		memset(&ainfo, 0, sizeof(ainfo));
		ainfo.bufferSize = 8820;                      // max number of bytes
		ainfo.format = PXCAudio::AUDIO_FORMAT_PCM;    // audio sample format
		ainfo.sampleRate = 44100;                     // sampling rate in Hz
		ainfo.nchannels = 2;                          // number of channels
		PXCAudio *audio = session->CreateAudio(&ainfo);
		//senseManager->EnablePersonTracking();
		
		while (bRunning)
		{
			auto status = senseManager->AcquireFrame(true);
			const float s = 1.0f / DepthWidth;
			const float t = 1.0f / DepthHeight;
			if (status == PXC_STATUS_NO_ERROR)
			{
				//auto personTrackingModule = senseManager->QueryPersonTracking();

				PXCCapture::Sample* sample = senseManager->QuerySample();
				status = projection->QueryVertices(sample->depth, vertices.data());
				if (status != PXC_STATUS_NO_ERROR)
				{
					UE_LOG(LogRealSense, Error, TEXT("Couldn't project vertices."));
				}
				PXCAudio::AudioData audioData;
				// Request as PCM
				audio->AcquireAccess(PXCAudio::ACCESS_READ, PXCAudio::AUDIO_FORMAT_PCM, &audioData);
				PXCImage* mapped = projection->CreateColorImageMappedToDepth(sample->depth, sample->color);
				Crit.Lock();
				//if (personTrackingModule)
				//{
				//	auto personTrackingConfig = personTrackingModule->QueryConfiguration();
				//	auto personTrackingData = personTrackingModule->QueryOutput();
				//}
				CameraSpacePoints.Reset();
				for (int32 i = 0; i < vertices.size(); i++)
				{
					const auto &Vertex = vertices.at(i);
					const float Scale = 0.1f; // mm to cm
					const float Forward = Vertex.z * Scale;
					const float Up = Vertex.y * Scale;
					const float Right = Vertex.x * Scale;
					CameraSpacePoints.Add(FVector(Forward, Right, Up));
				}
				CopyColorImageToBuffer(sample->color, Bgra, ColorWidth, ColorHeight);
				CopyDepthImageToBuffer(sample->depth, DepthBuffer, DepthWidth, DepthHeight);
				ColorBuffer.SetNumUninitialized(DepthWidth * DepthHeight * 4);
				CopyColorImageToBuffer(mapped, ColorBuffer, DepthWidth, DepthHeight);
				uint8 *ptr = audioData.dataPtr;
				const uint32 size = audioData.dataSize * sizeof(uint16);
				if (size > 0)
				{
					this->AudioData.Append(ptr, size);
				}
				DepthSample++;
				Crit.Unlock();
				audio->ReleaseAccess(&audioData);
				mapped->Release();
				//if (personTrackingModule != nullptr) personTrackingModule->Release();
			}
			senseManager->ReleaseFrame();
		}
		audio->Release();
		if (projection != nullptr) projection->Release();	
		return 0;
	}

	RealSenseRunner(ARealSenseActor *Actor) :
		Thread(nullptr), Target(Actor), 
		ColorWidth(0), ColorHeight(0), DepthWidth(0), DepthHeight(0),
		SampleRate(0), Channels(0), Sender(nullptr), ColorSample(0), DepthSample(0)
	{
	}

	void UpdateFrame()
	{
		/*
		DepthWidth = impl->GetDepthImageWidth();
		DepthHeight = impl->GetDepthImageHeight();
		ColorWidth = impl->GetColorImageWidth();
		ColorHeight = impl->GetColorImageHeight();
		auto DepthBuffer = impl->GetDepthBuffer();
		auto ColorBuffer = impl->GetColorBuffer();
		*/
	}

	void BeginPlay()
	{	
		capture = (nullptr);
		device = (nullptr);
		deviceInfo = {};
		senseManager = PXCSenseManager::CreateInstance();

		ARealSenseActor *Actor = (ARealSenseActor*)Target.Get();
		auto colorResolution = GetEColorResolutionValue(Actor->ColorResolution);
		auto status = senseManager->EnableStream(PXCCapture::StreamType::STREAM_TYPE_COLOR,
			colorResolution.width,
			colorResolution.height,
			colorResolution.fps);
		if (status != PXC_STATUS_NO_ERROR)
		{
			UE_LOG(LogRealSense, Error, TEXT("Couldn't enable color stream %d x %d, fps: %d"), colorResolution.width, colorResolution.height, colorResolution.fps);
			return;
		}
		ColorWidth = colorResolution.width;
		ColorHeight = colorResolution.height;

		Actor->Camera->SetDimensions(FIntPoint(ColorWidth, ColorHeight));
		Actor->Camera->UpdateResource();
		if (Actor->AudioOutput)
		{
			Actor->AudioOutput->NumChannels = 2;
			Actor->AudioOutput->SampleRate = 44100;
		}
		auto depthResolution = GetEDepthResolutionValue(Actor->DepthResolution);
		DepthWidth = depthResolution.width;
		DepthHeight = depthResolution.height;
		status = senseManager->EnableStream(PXCCapture::StreamType::STREAM_TYPE_DEPTH,
			depthResolution.width,
			depthResolution.height,
			depthResolution.fps);
		if (status != PXC_STATUS_NO_ERROR)
		{
			UE_LOG(LogRealSense, Error, TEXT("Couldn't enable depth stream %d x %d, fps: %d"), depthResolution.width, depthResolution.height, depthResolution.fps);
			return;
		}
		
		bRunning = true;
		Thread = FRunnableThread::Create(this, TEXT("RealSenseRunner"));
	}

	void EndPlay()
	{
		bRunning = false;
		if (Thread != nullptr) Thread->WaitForCompletion();
	}


	virtual int64 GetFrameData(
		int64 LastFrame,
		TArray<int32> &Triangles,
		TArray<FVector> &Vertices,
		TArray<FColor> &VertexColors,
		TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> &Camera,
		int32 &ColorWidth,
		int32 &ColorHeight,
		TArray<uint8> &AudioData,
		int32 &Channels,
		int32 &SampleRate) override
	{
		int64 Result;
		
		Crit.Lock();
		if (LastFrame < DepthSample)
		{
			ColorWidth = this->ColorWidth;
			ColorHeight = this->ColorHeight;
			GenerateVertexData(DepthWidth, DepthHeight, ColorWidth, ColorHeight, ColorBuffer, CameraSpacePoints, UVs, Triangles, Vertices, VertexColors);
			if (!Camera.IsValid())
			{
				Camera = TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe>(new TArray<uint8>());
			}
			*Camera = Bgra;
			if (this->AudioData.Num() > 0 && !(&AudioData == &this->AudioData))
			{
				AudioData.Append(this->AudioData);
				this->AudioData.Reset();
			}
			Channels = this->Channels;
			SampleRate = this->SampleRate;
		}
		Result = DepthSample;
		Crit.Unlock();
		return Result;
	}

	void GenerateVertexData(
		int32 DepthWidth,
		int32 DepthHeight,
		int32 ColorWidth,
		int32 ColorHeight,
		const TArray<uint8> &ColorBuffer,
		const TArray<FVector> &CameraSpacePoints,
		const TArray<FVector2D> &UVs,
		TArray<int32> &Triangles,
		TArray<FVector> &Vertices,
		TArray<FColor> &VertexColors)
	{
		ARealSenseActor *Actor = (ARealSenseActor*)Target.Get();
		Triangles.Reset();
		Vertices.Reset();
		VertexColors.Reset();
		if (Actor == nullptr || ColorWidth == 0 || ColorHeight == 0 || DepthWidth == 0 || DepthHeight == 0)
		{
			return;
		}
		const float Resolution = Actor->Resolution;
		const float ViewportWidth = Actor->ViewportWidth;
		const float ViewportHeight = Actor->ViewportHeight;
		const float MaxDistanceInCm = Actor->MaxDistanceInMeters * 100.0f;
		const float MinDistanceInCm = Actor->MinDistanceInMeters * 100.0f;
		const int32 MaxEdgeLength = Actor->MaxEdgeLength;
		const int step = 1;
		const int h = FMath::RoundToInt(DepthHeight * FMath::Clamp(Resolution, 0.1f, 1.0f));
		const int w = FMath::RoundToInt(DepthWidth * FMath::Clamp(Resolution, 0.1f, 1.0f));
		const int startx = FMath::RoundToInt((w - FMath::Clamp(ViewportWidth, 0.0f, 1.0f)*w) / 2.0f);
		const int starty = FMath::RoundToInt((h - FMath::Clamp(ViewportHeight, 0.0f, 1.0f)*h) / 2.0f);
		int32 Dropped = 0;
		FVector P[2][2];
		FColor C[2][2];
		const float iw = 1.0 / w;
		const float ih = 1.0 / h;


		for (int y = starty; y < h - step - starty; y += step)
		{
			for (int x = startx; x < w - step - startx; x += step)
			{
				const float scale = 1.0f;
				bool skip = false;
				for (int32 ix = 0; ix < 2 && !skip; ix++)
				{
					for (int32 iy = 0; iy < 2; iy++)
					{
						const int32 X0 = (x + ix*step);
						const int32 Y0 = (y + iy*step);
						const int32 X = FMath::RoundToInt((X0 * iw)*DepthWidth);
						const int32 Y = FMath::RoundToInt((Y0 * ih)*DepthHeight);
						const int32 DepthIndex = Y  * DepthWidth + X;

						const FVector &cameraSpacePoint = CameraSpacePoints[DepthIndex];
						if (cameraSpacePoint.X == 0)
						{
							skip = true;
							break;
						}
						const float dist = FMath::Sqrt(cameraSpacePoint.X * cameraSpacePoint.X +
							cameraSpacePoint.Y * cameraSpacePoint.Y +
							cameraSpacePoint.Z * cameraSpacePoint.Z);
						if (dist > MinDistanceInCm && dist <= MaxDistanceInCm)
						{
							const uint8 *bgra = &(ColorBuffer.GetData()[DepthIndex * 4]);
							FColor &Color = C[ix][iy];
							Color.B = bgra[0];
							Color.G = bgra[1];
							Color.R = bgra[2];
							Color.A = 0xff;
							P[ix][iy] = cameraSpacePoint;
						}
						else
						{
							skip = true;
							break;
						}

					}
				}
				if (skip)
				{
					Dropped++;
					continue;
				}
				const FVector &P00 = P[0][0];
				const FVector &P01 = P[0][1];
				const FVector &P10 = P[1][0];
				const FVector &P11 = P[1][1];
				const int32 max_edge_len = MaxEdgeLength;
				if (((P00.X > 0) && (P01.X > 0) && (P10.X > 0) && (P11.X > 0) && (P01.X > 0) && (P10.X > 0) &&// check for non valid values
					(abs(P00.X - P01.X) < max_edge_len) &&
					(abs(P10.X - P01.X) < max_edge_len) &&
					(abs(P11.X - P01.X) < max_edge_len) &&
					(abs(P10.X - P01.X) < max_edge_len)))
				{
					const int32 Next = Vertices.Num();

					Vertices.Add(P00);
					Vertices.Add(P01);
					Vertices.Add(P10);

					Triangles.Add(Next);
					Triangles.Add(Next + 1);
					Triangles.Add(Next + 2);


					VertexColors.Add(C[0][0]);
					VertexColors.Add(C[0][1]);
					VertexColors.Add(C[1][0]);

					//Vertices.Add(P01);
					Triangles.Add(Next + 1);
					Vertices.Add(P11);
					Triangles.Add(Next + 3);
					//Vertices.Add(P10);
					Triangles.Add(Next + 2);
					//VertexColors.Add(C[0][1]);
					VertexColors.Add(C[1][1]);
					//VertexColors.Add(C[1][0]);
				}
			}
		}
	}

};



ARealSenseActor::ARealSenseActor(const FObjectInitializer &Init) :
	Runner(nullptr)
	, ColorResolution(EColorResolution::RES4)
	, DepthResolution(EDepthResolution::RES5)
	, Camera(nullptr)
	, AudioOutput(nullptr)
	, ViewportWidth(1.0f)
	, ViewportHeight(1.0f)
	, MinDistanceInMeters(0)
	, MaxDistanceInMeters(10)
	, Resolution(1.0f)
	, MaxEdgeLength(12)
	, ServerHost(TEXT("localhost"))
	, ServerPort(8081)
	, WebSocketPath("/realsense/remote")
	, FrameIndex(0)
	, bMeshPending(false)
{
	MeshComp = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Mesh"));
	PrimaryActorTick.bCanEverTick = true;
}

void ARealSenseActor::BeginPlay()
{
	Runner = new RealSenseRunner(this);
	Camera = NewObject<URealSenseTexture>();
	Receiver = nullptr;
	Sender = nullptr;
	if (this->RemoteUser.Len() > 0)
	{
		Receiver = new FRemoteFrameSource(ServerHost, ServerPort, WebSocketPath, LocalUser, RemoteUser);
		FrameSource = Receiver;
	}
	if (this->LocalUser.Len() > 0)
	{
		if (Receiver != nullptr)
		{
			Sender = Receiver;
		}
		else
		{
			Sender = new FRemoteFrameSource(ServerHost, ServerPort, WebSocketPath, LocalUser, RemoteUser);
			FrameSource = Runner;
		}
		Runner->SetSender(Sender);
	}
	else
	{
		FrameSource = Runner;
	}
	Runner->BeginPlay();
	Super::BeginPlay();
}


void ARealSenseActor::Tick(float  DeltaSeconds)
{
	// process pending commands
	CommandDelegate Command;
	while (Commands.Dequeue(Command))
	{
		Command.Execute();
	}

	Runner->Tick();
	if (FrameSource != nullptr && !bMeshPending)
	{
		int32 ColorWidth, ColorHeight;
		int64 NextIndex = FrameSource->GetFrameData(FrameIndex,
			Triangles, Vertices, VertexColors,
			CurrentCameraFrame, ColorWidth, ColorHeight,
			AudioData, Channels, SampleRate);

		if (FrameIndex < NextIndex)
		{

			FrameIndex = NextIndex;
			// mesh reconstruction
			if (Triangles.Num() > 0)
			{
				MeshComp->CreateMeshSection(0, Vertices, Triangles, Normals, UVs, VertexColors, Tangents, EnablePhysics);
			}
			else
			{
				MeshComp->ClearAllMeshSections();
			}
			// camera feed
			if (Camera != nullptr && ColorWidth != 0 && ColorHeight != 0)
			{
				FIntPoint Dim = Camera->GetDimensions();
				if (Dim.X != ColorWidth || Dim.Y != ColorHeight)
				{
					Camera->SetDimensions(FIntPoint(ColorWidth, ColorHeight));
					Camera->UpdateResource();
				}
				Camera->SetCurrentFrame(CurrentCameraFrame);
			}
			// audio feed
			if (AudioOutput != nullptr && AudioData.Num() > 0)
			{
				AudioOutput->NumChannels = Channels;
				AudioOutput->SampleRate = SampleRate;
				AudioOutput->EnqueuePCMData(AudioData.GetData(), AudioData.Num());
				AudioData.Reset();
			}
			bMeshPending = true;
		}
	}
	else
	{
		bMeshPending = false;
	}
	Super::Tick(DeltaSeconds);
}

void ARealSenseActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Runner != nullptr) Runner->EndPlay();
	if (Receiver == Sender)
	{
		Sender = nullptr;
	}
	if (Receiver != nullptr)
	{
		Receiver->Close();
	}
	if (Sender != nullptr)
	{
		Sender->Close();
	}
	delete Receiver;
	Receiver = nullptr;
	delete Sender;
	Sender = nullptr;
	delete Runner;
	Runner = nullptr;
	Super::EndPlay(EndPlayReason);
}
