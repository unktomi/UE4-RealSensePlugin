// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "Engine.h"

// You should place include statements to your module's private header files here.  You only need to
// add includes for headers that are used in most of your module's source files though.
#include "IRealSensePlugin.h"
#include <pxcimage.h>
#include <pxccapture.h>
#include <pxcprojection.h>
#include <pxcsensemanager.h>
#include "RealSenseActor.h"
#include "ProceduralMeshComponent.h"
#include "WeakObjectPtr.h"
#include "RealSenseTexture.h"
#include "RawAudioSoundWave.h"
#include "FrameSource.h"
#include "RemoteFrameSource.h"
#include "RealSenseUtils.h"