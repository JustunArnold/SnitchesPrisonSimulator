// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "VREditorWorldInteraction.h"
#include "HAL/IConsoleManager.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "Misc/CommandLine.h"
#include "Modules/ModuleManager.h"
#include "VREditorMode.h"
#include "VREditorAssetContainer.h"
#include "ViewportInteractionTypes.h"
#include "Materials/MaterialInterface.h"
#include "Engine/Texture.h"
#include "LevelEditorViewport.h"
#include "ViewportWorldInteraction.h"
#include "Engine/BrushBuilder.h"
#include "Components/PrimitiveComponent.h"
#include "Kismet2/ComponentEditorUtils.h"
#include "Sound/SoundCue.h"
#include "Editor.h"
#include "VREditorUISystem.h"
#include "VREditorFloatingUI.h"
#include "VREditorInteractor.h"

// For actor placement
#include "ObjectTools.h"
#include "AssetSelection.h"
#include "IPlacementModeModule.h"

#include "LevelEditor.h"
#include "SLevelViewport.h"
#include "LevelEditorActions.h"

#define LOCTEXT_NAMESPACE "VREditor"


namespace VREd
{
	static FAutoConsoleVariable SizeOfActorsOverContentBrowserThumbnail( TEXT( "VREd.SizeOfActorsOverContentBrowserThumbnail" ), 6.0f, TEXT( "How large objects should be when rendered 'thumbnail size' over the Content Browser" ) );
	static FAutoConsoleVariable HoverHapticFeedbackStrength( TEXT( "VREd.HoverHapticFeedbackStrength" ), 0.1f, TEXT( "Default strength for haptic feedback when hovering" ) );
	static FAutoConsoleVariable HoverHapticFeedbackTime( TEXT( "VREd.HoverHapticFeedbackTime" ), 0.2f, TEXT( "The minimum time between haptic feedback for hovering" ) );
	static FAutoConsoleVariable PivotPointTransformGizmo( TEXT( "VREd.PivotPointTransformGizmo" ), 1, TEXT( "If the pivot point transform gizmo is used instead of the bounding box gizmo" ) );
	static FAutoConsoleVariable DragHapticFeedbackStrength( TEXT( "VREd.DragHapticFeedbackStrength" ), 1.0f, TEXT( "Default strength for haptic feedback when starting to drag objects" ) );
	static FAutoConsoleVariable PlacementInterpolationDuration( TEXT( "VREd.PlacementInterpolationDuration" ), 0.6f, TEXT( "How long we should interpolate newly-placed objects to their target location." ) );
}

UVREditorWorldInteraction::UVREditorWorldInteraction( const FObjectInitializer& Initializer ) : 
	Super( Initializer ),
	Owner( nullptr ),
	FloatingUIAssetDraggedFrom( nullptr ),
	PlacingMaterialOrTextureAsset( nullptr )
{
}

void UVREditorWorldInteraction::Init( UVREditorMode* InOwner, UViewportWorldInteraction* InViewportWorldInteraction )
{
	Owner = InOwner;
	ViewportWorldInteraction = InViewportWorldInteraction;

	// Find out when the user drags stuff out of a content browser
	FEditorDelegates::OnAssetDragStarted.AddUObject( this, &UVREditorWorldInteraction::OnAssetDragStartedFromContentBrowser );

	ViewportWorldInteraction->OnStopDragging().AddUObject( this, &UVREditorWorldInteraction::StopDragging );
	ViewportWorldInteraction->OnWorldScaleChanged().AddUObject( this, &UVREditorWorldInteraction::UpdateNearClipPlaneOnScaleChange );
}

void UVREditorWorldInteraction::Shutdown()
{
	FEditorDelegates::OnAssetDragStarted.RemoveAll( this );
	ViewportWorldInteraction->OnStopDragging().RemoveAll( this );
	ViewportWorldInteraction->OnWorldScaleChanged().RemoveAll( this );

	PlacingMaterialOrTextureAsset = nullptr;
	FloatingUIAssetDraggedFrom = nullptr;
	Owner = nullptr;
}

void UVREditorWorldInteraction::StopDragging( UViewportInteractor* Interactor )
{
	EViewportInteractionDraggingMode InteractorDraggingMode = Interactor->GetDraggingMode();

	// If we were placing a material, go ahead and do that now
	if ( InteractorDraggingMode == EViewportInteractionDraggingMode::Material )
	{
		PlaceDraggedMaterialOrTexture( Interactor );
	}	

	// If we were placing something, bring the window back
	if ( FloatingUIAssetDraggedFrom != nullptr && 
		( InteractorDraggingMode == EViewportInteractionDraggingMode::TransformablesAtLaserImpact ||
		  InteractorDraggingMode == EViewportInteractionDraggingMode::Material ) )
	{
		const bool bShouldShow = true;
		const bool bSpawnInFront = false;
		const bool bDragFromOpen = false;
		const bool bPlaySound = false;
		Owner->GetUISystem().ShowEditorUIPanel(FloatingUIAssetDraggedFrom, Cast<UVREditorInteractor>(Interactor->GetOtherInteractor()), bShouldShow, bSpawnInFront, bDragFromOpen, bPlaySound);
		FloatingUIAssetDraggedFrom = nullptr;
	}
}

void UVREditorWorldInteraction::UpdateNearClipPlaneOnScaleChange(const float NewWorldToMetersScale)
{
	// Adjust the clipping plane for the user's scale, but don't let it be larger than the engine default
	const float DefaultWorldToMetersScale = Owner->GetSavedEditorState().WorldToMetersScale;
	GNearClippingPlane = FMath::Min((Owner->GetDefaultVRNearClipPlane()) * (NewWorldToMetersScale / DefaultWorldToMetersScale), Owner->GetSavedEditorState().NearClipPlane);
}

void UVREditorWorldInteraction::StartDraggingMaterialOrTexture( UViewportInteractor* Interactor, const FViewportActionKeyInput& Action, const FVector HitLocation, UObject* MaterialOrTextureAsset )
{
	check( MaterialOrTextureAsset != nullptr );

	FVector LaserPointerStart, LaserPointerEnd;
	const bool bHaveLaserPointer = Interactor->GetLaserPointer( /* Out */ LaserPointerStart, /* Out */ LaserPointerEnd );
	if( bHaveLaserPointer )
	{
		PlacingMaterialOrTextureAsset = MaterialOrTextureAsset;

		FViewportInteractorData& InteractorData = Interactor->GetInteractorData();

		Interactor->SetDraggingMode( EViewportInteractionDraggingMode::Material );

		// Starting a new drag, so make sure the other hand doesn't think it's assisting us
		FViewportInteractorData& OtherInteractorData = Interactor->GetOtherInteractor()->GetInteractorData();
		OtherInteractorData.bWasAssistingDrag = false;

		InteractorData.bDraggingWithGrabberSphere = false;
		InteractorData.bIsFirstDragUpdate = true;
		InteractorData.bWasAssistingDrag = false;
		InteractorData.DragRayLength = ( HitLocation - LaserPointerStart ).Size();
		InteractorData.LastDragToLocation = HitLocation;
		InteractorData.InteractorRotationAtDragStart = InteractorData.Transform.GetRotation();
		InteractorData.GrabberSphereLocationAtDragStart = FVector::ZeroVector;
		InteractorData.ImpactLocationAtDragStart = HitLocation;
		InteractorData.DragTranslationVelocity = FVector::ZeroVector;
		InteractorData.DragRayLengthVelocity = 0.0f;
		InteractorData.bIsDrivingVelocityOfSimulatedTransformables = false;

		InteractorData.DraggingTransformGizmoComponent = nullptr;
	
		InteractorData.TransformGizmoInteractionType = ETransformGizmoInteractionType::None;
		InteractorData.GizmoStartTransform = FTransform::Identity;
		InteractorData.GizmoLastTransform = InteractorData.GizmoTargetTransform = InteractorData.GizmoUnsnappedTargetTransform = InteractorData.GizmoInterpolationSnapshotTransform = InteractorData.GizmoStartTransform;
		InteractorData.GizmoStartLocalBounds = FBox( 0 );

		InteractorData.GizmoSpaceFirstDragUpdateOffsetAlongAxis = FVector::ZeroVector;	// Will be determined on first update
		InteractorData.GizmoSpaceDragDeltaFromStartOffset = FVector::ZeroVector;	// Set every frame while dragging

		ViewportWorldInteraction->SetDraggedSinceLastSelection( false );
		ViewportWorldInteraction->SetLastDragGizmoStartTransform( FTransform::Identity );

		// Play a haptic effect when objects aTrackingTransactionre picked up
		Interactor->PlayHapticEffect( VREd::DragHapticFeedbackStrength->GetFloat() );
	}
}

void UVREditorWorldInteraction::OnAssetDragStartedFromContentBrowser( const TArray<FAssetData>& DraggedAssets, UActorFactory* FactoryToUse )
{
	FloatingUIAssetDraggedFrom = nullptr;
	if( ViewportWorldInteraction != nullptr )
	{
		// Figure out which controller pressed the button and started dragging
		// @todo vreditor placement: This logic could misfire.  Ideally we would be routed information from the pointer event, so we can determine the hand.
		UVREditorInteractor* PlacingWithInteractor = nullptr;

		TArray<UViewportInteractor*>& Interactors = ViewportWorldInteraction->GetInteractors();
		for( UViewportInteractor* Interactor : Interactors )
		{
			UVREditorInteractor* VRInteractor = Cast<UVREditorInteractor>( Interactor );
			FViewportActionKeyInput* SelectAndMove_Action = Interactor->GetActionWithName( ViewportWorldActionTypes::SelectAndMove );
			if( VRInteractor && SelectAndMove_Action != nullptr && SelectAndMove_Action->bIsInputCaptured )
			{
				if( VRInteractor->IsClickingOnUI() && !VRInteractor->IsRightClickingOnUI() )
				{
					PlacingWithInteractor = VRInteractor;
					break;
				}
			}
		}

		if( PlacingWithInteractor != nullptr )
		{
			FEditorDelegates::LoadSelectedAssetsIfNeeded.Broadcast();

			TArray< UObject* > DroppedObjects;
			DroppedObjects.Reserve( DraggedAssets.Num() );

			for( const FAssetData& AssetData : DraggedAssets )
			{
				UObject* AssetObj = AssetData.GetAsset();
				if( AssetObj != nullptr )
				{
					// Don't add the same asset more than once
					DroppedObjects.AddUnique( AssetObj );
				}
			}

			if( DroppedObjects.Num() > 0 )
			{
				// Hide the UI panel that's being used to drag
				FloatingUIAssetDraggedFrom = PlacingWithInteractor->GetLastHoveredWidgetComponent();
				Owner->GetUISystem().ShowEditorUIPanel( FloatingUIAssetDraggedFrom, PlacingWithInteractor, false, false, false );

				const bool bShouldInterpolateFromDragLocation = VREd::PlacementInterpolationDuration->GetFloat() > KINDA_SMALL_NUMBER;
				StartPlacingObjects( DroppedObjects, FactoryToUse, PlacingWithInteractor, bShouldInterpolateFromDragLocation );

				const UVREditorAssetContainer& AssetContainer = Owner->GetAssetContainer();
				Owner->PlaySound(AssetContainer.DropFromContentBrowserSound, PlacingWithInteractor->GetTransform().GetLocation());
			}
		}
	}
}

void UVREditorWorldInteraction::StartPlacingObjects( const TArray<UObject*>& ObjectsToPlace, UActorFactory* FactoryToUse, UVREditorInteractor* PlacingWithInteractor, const bool bShouldInterpolateFromDragLocation )
{
	if( ViewportWorldInteraction == nullptr || PlacingWithInteractor == nullptr )
	{
		return;
	}

	const bool bIsPreview = false;	 // @todo vreditor placement: Consider supporting a "drop preview" actor (so you can cancel placement interactively)

	bool bTransactionStarted = false;

	{
		// Cancel UI input
		{
			PlacingWithInteractor->SetIsClickingOnUI( false );
			PlacingWithInteractor->SetIsRightClickingOnUI( false );

			FViewportActionKeyInput* SelectAndMoveAction = PlacingWithInteractor->GetActionWithName( ViewportWorldActionTypes::SelectAndMove );
			if( SelectAndMoveAction != nullptr )
			{
				SelectAndMoveAction->bIsInputCaptured = false;
			}
		}


		TArray< UObject* > DroppedObjects;
		TArray< AActor* > AllNewActors;

		UObject* DraggingSingleMaterialOrTexture = nullptr;

		FVector PlaceAt = PlacingWithInteractor->GetInteractorData().LastHoverLocationOverUI;

		// Only place the object at the laser impact point if we're NOT going to interpolate to the impact
		// location.  When interpolation is enabled, it looks much better to blend to the new location
		if( !bShouldInterpolateFromDragLocation )
		{
			FVector HitLocation = FVector::ZeroVector;
			if( ViewportWorldInteraction->FindPlacementPointUnderLaser( PlacingWithInteractor, /* Out */ HitLocation ) )
			{
				PlaceAt = HitLocation;
			}
		}

		for( UObject* AssetObj : ObjectsToPlace )
		{
			bool bCanPlace = true;

			const FString ObjectPath = AssetObj->GetPathName();
			if( !ObjectTools::IsAssetValidForPlacing( ViewportWorldInteraction->GetWorld(), ObjectPath ) )
			{
				bCanPlace = false;
			}
			else
			{
				UClass* ClassObj = Cast<UClass>( AssetObj );
				if( ClassObj )
				{
					if( !ObjectTools::IsClassValidForPlacing( ClassObj ) )
					{
						bCanPlace = false;
					}

					AssetObj = ClassObj->GetDefaultObject();
				}
			}

			const bool bIsMaterialOrTexture = ( AssetObj->IsA( UMaterialInterface::StaticClass() ) || AssetObj->IsA( UTexture::StaticClass() ) );
			if( bIsMaterialOrTexture && ObjectsToPlace.Num() == 1 )
			{
				DraggingSingleMaterialOrTexture = AssetObj;
			}
			else
			{
				// @todo mesheditor: We're dragging actors, so deactivate mesh editor mode for this.  They'll contend over transformables.
				static FName MeshEditorModeName( TEXT( "MeshEditor" ) );
				GLevelEditorModeTools().DeactivateMode( MeshEditorModeName );
			}

			// Check if the asset has an actor factory
			bool bHasActorFactory = FActorFactoryAssetProxy::GetFactoryForAssetObject( AssetObj ) != nullptr;


			if( !( AssetObj->IsA( AActor::StaticClass() ) || bHasActorFactory ) &&
				!AssetObj->IsA( UBrushBuilder::StaticClass() ) )
			{
				bCanPlace = false;
			}


			FTrackingTransaction& TrackingTransaction = ViewportWorldInteraction->GetTrackingTransaction();
			if( bCanPlace )
			{
				CA_SUPPRESS(6240);
				if( !bTransactionStarted && !bIsPreview )
				{
					bTransactionStarted = true;

					TrackingTransaction.TransCount++;
					TrackingTransaction.Begin( LOCTEXT( "PlacingActors", "Placing Actors" ) );

					// Suspend actor/component modification during each delta step to avoid recording unnecessary overhead into the transaction buffer
					GEditor->DisableDeltaModification( true );
				}

				GEditor->ClickLocation = PlaceAt;
				GEditor->ClickPlane = FPlane( PlaceAt, FVector::UpVector );

				// Attempt to create actors from the dropped object
				const bool bSelectNewActors = true;
				const EObjectFlags NewObjectFlags = bIsPreview ? RF_Transient : RF_Transactional;

				TArray<AActor*> NewActors = FLevelEditorViewportClient::TryPlacingActorFromObject( ViewportWorldInteraction->GetWorld()->GetCurrentLevel(), AssetObj, bSelectNewActors, NewObjectFlags, FactoryToUse );

				AllNewActors.Append( NewActors );
				if( NewActors.Num() > 0 )
				{
					DroppedObjects.Add( AssetObj );
				}
			}
		}

		// Cancel the transaction if nothing was placed
		if( bTransactionStarted && AllNewActors.Num() == 0)
		{
			FTrackingTransaction& TrackingTransaction = ViewportWorldInteraction->GetTrackingTransaction();
			--TrackingTransaction.TransCount;
			TrackingTransaction.Cancel();
			GEditor->DisableDeltaModification( false );
		}

		if( AllNewActors.Num() > 0 )
		{
			if( !bIsPreview )
			{
				if( IPlacementModeModule::IsAvailable() )
				{
					IPlacementModeModule::Get().AddToRecentlyPlaced( DroppedObjects, FactoryToUse );
				}

				FEditorDelegates::OnNewActorsDropped.Broadcast( DroppedObjects, AllNewActors );
			}

			FBox BoundsOfAllActors;
			{
				BoundsOfAllActors.Init();
				for( AActor* NewActor : AllNewActors )
				{
					BoundsOfAllActors += NewActor->CalculateComponentsBoundingBoxInLocalSpace();
				}
			}
			const float BoundsOfAllActorsSize = BoundsOfAllActors.GetSize().GetAbsMax();
			const float DesiredScale = ( VREd::SizeOfActorsOverContentBrowserThumbnail->GetFloat() / BoundsOfAllActorsSize ) * ViewportWorldInteraction->GetWorldScaleFactor();

			// Start the placed objects off scaled down to match the content browser thumbnail
			if( bShouldInterpolateFromDragLocation )
			{
 				for( AActor* NewActor : AllNewActors )
 				{
 					NewActor->SetActorScale3D( FVector( DesiredScale ) );
 				}
			}

			// We changed the initial scale of selected actors, so make sure our transformables and gizmo start transform are up to date.
			GEditor->NoteSelectionChange();

			// Start dragging the new actor(s)
			UPrimitiveComponent* ClickedTransformGizmoComponent = nullptr;
			const bool bIsPlacingNewObjects = true;
			const bool bAllowInterpolationWhenPlacing = bShouldInterpolateFromDragLocation;
			const bool bStartTransaction = false;
			const bool bWithGrabberSphere = false;	// Always place using the laser, not the grabber sphere
			ViewportWorldInteraction->StartDragging( PlacingWithInteractor, ClickedTransformGizmoComponent, PlaceAt, bIsPlacingNewObjects, bAllowInterpolationWhenPlacing, bStartTransaction, bWithGrabberSphere );

			// If we're interpolating, update the target transform of the actors to use our overridden size.  When
			// we placed them we set their size to be 'thumbnail sized', and we want them to interpolate to
			// their actual size in the world
			if( bShouldInterpolateFromDragLocation )
			{
				const FVector NewScale( 1.0f / DesiredScale );
				FViewportInteractorData& InteractorData = PlacingWithInteractor->GetInteractorData();
				InteractorData.GizmoUnsnappedTargetTransform.SetScale3D( NewScale );
				InteractorData.GizmoLastTransform.SetScale3D( NewScale );
				InteractorData.GizmoTargetTransform.SetScale3D( NewScale );
			}
		}

		if( DraggingSingleMaterialOrTexture != nullptr )
		{
			const FViewportActionKeyInput Action( ViewportWorldActionTypes::SelectAndMove );

			// Start dragging the material
			StartDraggingMaterialOrTexture( PlacingWithInteractor,	Action, PlacingWithInteractor->GetHoverLocation(), DraggingSingleMaterialOrTexture );
		}
	}
}


void UVREditorWorldInteraction::PlaceDraggedMaterialOrTexture( UViewportInteractor* Interactor )
{
	if( ensure( Interactor->GetDraggingMode() == EViewportInteractionDraggingMode::Material ) &&
		PlacingMaterialOrTextureAsset != nullptr )
	{
		// Check to see if the laser pointer is over something we can drop on
		UPrimitiveComponent* HitComponent = nullptr;
		FVector HitLocation = FVector::ZeroVector;
		{
			const bool bIgnoreGizmos = true;	// Never place on top of gizmos, just ignore them
			const bool bEvenIfUIIsInFront = true;	// Don't let the UI block placement
			FHitResult HitResult = Interactor->GetHitResultFromLaserPointer( nullptr, bIgnoreGizmos, nullptr, bEvenIfUIIsInFront );
			if( HitResult.Actor.IsValid() )
			{
				if( ViewportWorldInteraction->IsInteractableComponent( HitResult.GetComponent() ) )	// @todo vreditor placement: We don't necessarily need to restrict to only VR-interactive components here
				{
					// Don't place materials on UI widget handles though!
					if( Cast<AVREditorFloatingUI>( HitResult.GetComponent()->GetOwner() ) == nullptr )
					{
						HitComponent = HitResult.GetComponent();
						HitLocation = HitResult.ImpactPoint;
					}
				}
			}
		}

		if( HitComponent != nullptr )
		{
			UObject* ObjToUse = PlacingMaterialOrTextureAsset;

			// Dropping a texture?
			UTexture* DroppedObjAsTexture = Cast<UTexture>( ObjToUse );
			if( DroppedObjAsTexture != NULL )
			{
				// Turn dropped textures into materials
				ObjToUse = FLevelEditorViewportClient::GetOrCreateMaterialFromTexture( DroppedObjAsTexture );
			}

			// Dropping a material?
			UMaterialInterface* DroppedObjAsMaterial = Cast<UMaterialInterface>( ObjToUse );
			if( DroppedObjAsMaterial )
			{
				// @todo vreditor placement: How do we get the material ID that was dropped on?  Regular editor uses hit proxies.  We may need to augment FHitResult.
				// @todo vreditor placement: Support optionally dropping on all materials, not only the impacted material
				bool bPlaced = false;
				check( Owner );
				Owner->OnPlaceDraggedMaterial().Broadcast( HitComponent, DroppedObjAsMaterial, bPlaced );
				if( !bPlaced )
				{
					const int32 TargetMaterialSlot = -1;	// All materials
					bool bPlaced = FComponentEditorUtils::AttemptApplyMaterialToComponent( HitComponent, DroppedObjAsMaterial, TargetMaterialSlot );

					const UVREditorAssetContainer& AssetContainer = Owner->GetAssetContainer();
					Owner->PlaySound(AssetContainer.DropFromContentBrowserSound, Interactor->GetTransform().GetLocation());
				}
			}
		}
	}

	this->PlacingMaterialOrTextureAsset = nullptr;
}

void UVREditorWorldInteraction::SnapSelectedActorsToGround()
{
	TSharedPtr<SLevelViewport> LevelEditorViewport = StaticCastSharedPtr<SLevelViewport>( ViewportWorldInteraction->GetDefaultOptionalViewportClient()->GetEditorViewportWidget() );
	if ( LevelEditorViewport.IsValid() )
	{
		FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>( TEXT( "LevelEditor" ) );
		const FLevelEditorCommands& Commands = LevelEditorModule.GetLevelEditorCommands();
		const TSharedPtr< FUICommandList >& CommandList = LevelEditorViewport->GetParentLevelEditor().Pin()->GetLevelEditorActions(); //@todo vreditor: Cast on leveleditor

		CommandList->ExecuteAction( Commands.SnapBottomCenterBoundsToFloor.ToSharedRef() );

		// Force transformables to refresh
		GEditor->NoteSelectionChange();
	}
}

#undef LOCTEXT_NAMESPACE
