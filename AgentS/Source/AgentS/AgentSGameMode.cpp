// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AgentSGameMode.h"
#include "AgentSCharacter.h"

AAgentSGameMode::AAgentSGameMode()
{
	// Set default pawn class to our character
	DefaultPawnClass = AAgentSCharacter::StaticClass();	
}
