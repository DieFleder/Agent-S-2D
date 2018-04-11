// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AgentSCharacter.h"
#include "PaperFlipbookComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Camera/CameraComponent.h"

#include "DrawDebugHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(SideScrollerCharacter, Log, All);

//////////////////////////////////////////////////////////////////////////
// AAgentSCharacter

AAgentSCharacter::AAgentSCharacter()
{
	// Use only Yaw from the controller and ignore the rest of the rotation.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	// Set the size of our collision capsule.
	GetCapsuleComponent()->SetCapsuleHalfHeight(96.0f);
	GetCapsuleComponent()->SetCapsuleRadius(40.0f);

	// Create a camera boom attached to the root (capsule)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 500.0f;
	CameraBoom->SocketOffset = FVector(0.0f, 0.0f, 75.0f);
	CameraBoom->bAbsoluteRotation = true;
	CameraBoom->bDoCollisionTest = false;
	CameraBoom->RelativeRotation = FRotator(0.0f, -90.0f, 0.0f);
	
	// Create an orthographic camera (no perspective) and attach it to the boom
	SideViewCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("SideViewCamera"));
	SideViewCameraComponent->ProjectionMode = ECameraProjectionMode::Orthographic;
	SideViewCameraComponent->OrthoWidth = 2048.0f;
	SideViewCameraComponent->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);

	// Prevent all automatic rotation behavior on the camera, character, and camera component
	CameraBoom->bAbsoluteRotation = true;
	SideViewCameraComponent->bUsePawnControlRotation = false;
	SideViewCameraComponent->bAutoActivate = true;
	GetCharacterMovement()->bOrientRotationToMovement = false;

	// Configure character movement
	GetCharacterMovement()->GravityScale = 2.0f;
	GetCharacterMovement()->AirControl = 0.80f;
	GetCharacterMovement()->JumpZVelocity = 1000.f;
	GetCharacterMovement()->GroundFriction = 3.0f;
	GetCharacterMovement()->MaxWalkSpeed = 600.0f;
	GetCharacterMovement()->MaxFlySpeed = 600.0f;

	// Lock character motion onto the XZ plane, so the character can't move in or out of the screen
	GetCharacterMovement()->bConstrainToPlane = true;
	GetCharacterMovement()->SetPlaneConstraintNormal(FVector(0.0f, -1.0f, 0.0f));

	// Behave like a traditional 2D platformer character, with a flat bottom instead of a curved capsule bottom
	// Note: This can cause a little floating when going up inclines; you can choose the tradeoff between better
	// behavior on the edge of a ledge versus inclines by setting this to true or false
	GetCharacterMovement()->bUseFlatBaseForFloorChecks = true;

    // 	TextComponent = CreateDefaultSubobject<UTextRenderComponent>(TEXT("IncarGear"));
    // 	TextComponent->SetRelativeScale3D(FVector(3.0f, 3.0f, 3.0f));
    // 	TextComponent->SetRelativeLocation(FVector(35.0f, 5.0f, 20.0f));
    // 	TextComponent->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));
    // 	TextComponent->SetupAttachment(RootComponent);

	// Enable replication on the Sprite component so animations show up when networked
	GetSprite()->SetIsReplicated(true);
	bReplicates = true;
}
//////////////////////////////////////////////////////////////////////////
// Input

void AAgentSCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// Note: the 'Jump' action and the 'MoveRight' axis are bound to actual keys/buttons/sticks in DefaultInput.ini (editable from Project Settings..Input)
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &AAgentSCharacter::AgentStartJump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &AAgentSCharacter::AgentStopJump);
	PlayerInputComponent->BindAction("UseWeapon", IE_Pressed, this, &AAgentSCharacter::UseWeapon);
	PlayerInputComponent->BindAxis("MoveRight", this, &AAgentSCharacter::MoveRight);


	//PlayerInputComponent->BindTouch(IE_Pressed, this, &AAgentSCharacter::TouchStarted);
	//PlayerInputComponent->BindTouch(IE_Released, this, &AAgentSCharacter::TouchStopped);
}
//void AAgentSCharacter::TouchStarted(const ETouchIndex::Type FingerIndex, const FVector Location)
//{
//	// Jump on any touch
//	Jump();
//}
//
//void AAgentSCharacter::TouchStopped(const ETouchIndex::Type FingerIndex, const FVector Location)
//{
//	// Cease jumping once touch stopped
//	StopJumping();
//}


void AAgentSCharacter::AgentStartJump()
{
	if (!bIsHanging)
	{
		Jump();
	}
}

void AAgentSCharacter::AgentStopJump()
{
	StopJumping();
}

void AAgentSCharacter::UseWeapon()
{

}

void AAgentSCharacter::MoveRight(float Value)
{
	/*UpdateChar();*/

	// Apply the input to the character motion
	AddMovementInput(FVector(1.0f, 0.0f, 0.0f), Value);
}

//////////////////////////////////////////////////////////////////////////
// Animation

void AAgentSCharacter::UpdateAnimation()
{
	const FVector PlayerVelocity = GetVelocity();
	const float PlayerSpeedSqr = PlayerVelocity.SizeSquared();

	// Are we moving or standing still?
	UPaperFlipbook* DesiredAnimation = (PlayerSpeedSqr > 0.0f) ? RunningAnimation : IdleAnimation;
	if (GetSprite()->GetFlipbook() != DesiredAnimation)
	{
		GetSprite()->SetFlipbook(DesiredAnimation);
	}
}

void AAgentSCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (PistolBlueprint != nullptr)
	{
		Pistol = GetWorld()->SpawnActor<APistol>(PistolBlueprint);
		Pistol->AttachToComponent(GetSprite(), FAttachmentTransformRules::KeepRelativeTransform, TEXT("WeaponSocket"));
		Pistol->AddActorLocalRotation(FRotator(0.f, 180.f, 0.f));
	}
}

void AAgentSCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	
	UpdateCharacter();	

	if (!GetCharacterMovement()->IsMovingOnGround() && bSearchForLedge)
	{
		UE_LOG(LogTemp, Warning, TEXT("In Air"))
		WallTrace();
		// TODO Impliment LedgeTrace. Will need some out parameters for the ledge and wall locations and normal vectors
	}
	else if (GetCharacterMovement()->IsMovingOnGround() && !bSearchForLedge)
	{
		bSearchForLedge = true;
	}
}

void AAgentSCharacter::WallTrace()
{
	FHitResult HitResult;
	FVector Start = GetActorLocation();
	FVector End = GetActorForwardVector() * 100 + Start;
	FQuat Rot;
	FCollisionShape Sphere = FCollisionShape::MakeSphere(25);
	bool bHit = GetWorld()->SweepSingleByChannel(HitResult, Start, End, Rot, ECollisionChannel::ECC_GameTraceChannel1, Sphere);
	//DrawDebugSphere(GetWorld(), Start, 25, 8, FColor::Red);
	//DrawDebugSphere(GetWorld(), End, 25, 8, FColor::Green);
	if (bHit)
	{
		//UE_LOG(LogTemp, Warning, TEXT("Hit"))
		bSearchForLedge = false;
	}
}

void AAgentSCharacter::UpdateCharacter()
{
	// Update animation to match the motion
	UpdateAnimation();

	// Now setup the rotation of the controller based on the direction we are travelling
	const FVector PlayerVelocity = GetVelocity();	
	float TravelDirection = PlayerVelocity.X;
	// Set the rotation so that the character faces his direction of travel.
	if (Controller != nullptr)
	{
		if (TravelDirection < 0.0f)
		{
			Controller->SetControlRotation(FRotator(0.0, 180.0f, 0.0f));
		}
		else if (TravelDirection > 0.0f)
		{
			Controller->SetControlRotation(FRotator(0.0f, 0.0f, 0.0f));
		}
	}
}
