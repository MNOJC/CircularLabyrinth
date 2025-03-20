#include "CircularGrid.h"

#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Math/UnrealMathUtility.h"
#include "Runtime/Windows/D3D11RHI/Public/Windows/D3D11ThirdParty.h"


ACircularGrid::ACircularGrid()
{
    PrimaryActorTick.bCanEverTick = false;

    CircularWalls = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("CircularWalls"));
    Pillars = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Pillars"));
    Path = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Path"));

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ACircularGrid::BeginPlay()
{
    Super::BeginPlay();

    SetLabyrinthEntrance(StartPath); // setup start cell algo
    SetLabyrinthExit(EndPath); // setup end cell algo
    StartRecursiveBacktracking(); // start algo
    
}

void ACircularGrid::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    ClearVariables(); // Clear Instanced text component
    GenerateGrid(); // generate grid only init cells
    GenerateGeometry(); // generate walls & pillars
}



void ACircularGrid::GenerateGrid()
{
    Cells.Empty(); // clear cells
    
    int32 CellIndex = 0;

    // Create and store center cell
    FLabyrinthCell CenterCell;                     
    CenterCell.Index = CellIndex++; // Increment to next index cell
    CenterCell.Ring = 0;                            
    CenterCell.Sector = 0;
    CenterCell.bCurrent = false;
    CenterCell.bVisited = false;
    Cells.Add(CenterCell);
    
    AddDebugTextRenderer(FVector::Zero(), FString::FromInt(CenterCell.Index)); // add debug index cell text component
    
    

    // Setup grid cells with index, sector, ring & cell location
    for(int32 Ring = 1; Ring < MaxRings; Ring++)
    {
        const int32 CurrentSubdivisions = GetRingSubdivision(Ring); // store current ring subdivision

        for(int32 Sector = 0; Sector < CurrentSubdivisions; Sector++)
        {
            FLabyrinthCell NewCell;
            NewCell.Index = CellIndex++; // cell index
            NewCell.Ring = Ring; // cell current ring 
            NewCell.Sector = Sector; // cell current sector
            
            NewCell.Location = CalculateCellLocation(NewCell.Ring, NewCell.Sector); // cell location
            
            AddDebugTextRenderer(NewCell.Location,  FString::FromInt(NewCell.Index)); // add debug index cell text component
            
            Cells.Add(NewCell); // add the new cell 
        }
    }

    for(FLabyrinthCell& Cell : Cells)
    {
        CalculateCellNeighbors(Cell); // setup cells neighbors
    }
}

void ACircularGrid::GenerateGeometry()
{
    //Remove pillar, radial walls & circular walls instances
    CircularWalls->ClearInstances();
    Pillars->ClearInstances();
    
    const int32 MaxSubdivisions = GetRingSubdivision(MaxRings); // Store max subdivision of labyrinth
    const float BaseAngleStep = 360.0f / MaxSubdivisions; // Angle steps in degrees
    
    for(int32 Ring = 0; Ring < MaxRings; Ring++) // Loop the number of time there are rings
    {
        const int32 CurrentSubdivisions = FMath::Pow(2.0f, FMath::FloorLog2(Ring + 1) + SubdivisionFactor);
        const int32 SubdivisionRatio = MaxSubdivisions / CurrentSubdivisions;
        const float Radius = BaseRadius + Ring * RingSpacing;
        const float EffectiveAngleStep = 360.0f / CurrentSubdivisions;

        // Generate Circular walls & pillars
        for(int32 Sector = 0; Sector < CurrentSubdivisions; Sector++)
        {
            float CircularWallAngle = ((Sector + 0.5f) * SubdivisionRatio * BaseAngleStep);
            float PillarAngle = (Sector * SubdivisionRatio * BaseAngleStep);

            const FVector LocationCircularWall = PolarToCartesian(Radius, CircularWallAngle);
            const FVector LocationPillar = PolarToCartesian(Radius, PillarAngle) + this->GetActorLocation();
            const FRotator Rotation(0.0f, CircularWallAngle + 90.0f, 0.0f);
            
            const float ChordLength = 2 * Radius * FMath::Sin(FMath::DegreesToRadians(EffectiveAngleStep / 2));
            const float Scale = ChordLength / (CircularWalls->GetStaticMesh()->GetBoundingBox().GetSize().X * 2);
            
            FTransform Transform(Rotation, LocationCircularWall + this->GetActorLocation());
            Transform.SetScale3D(FVector(Scale * 2.0f, 1.0f, 1.0f));
            CircularWalls->AddInstance(Transform);
            Pillars->AddInstance(FTransform(FRotator(0.0f, PillarAngle, 0.0f), LocationPillar, FVector(0.2f, 0.2f, 1.2f)));

        }

        // Generate radial walls
        if(Ring < MaxRings - 1)
        {
            const float NextRadius = Radius + RingSpacing;

            for(int32 Sector = 0; Sector < CurrentSubdivisions; Sector++)
            {
                
                const float CurrentAngle = Sector * SubdivisionRatio * BaseAngleStep;
                const float NextAngle = CurrentAngle;

                
                const FVector StartInner = PolarToCartesian(Radius, CurrentAngle);
                const FVector EndOuter = PolarToCartesian(NextRadius, NextAngle);

                
                const FVector Direction = (EndOuter - StartInner).GetSafeNormal();
                const float WallLength = (EndOuter - StartInner).Size();
                const FVector RadialMeshSize = CircularWalls->GetStaticMesh()->GetBoundingBox().GetSize();
                
                FTransform RadialTransform;
                RadialTransform.SetLocation((StartInner + EndOuter) * 0.5f + this->GetActorLocation());
                RadialTransform.SetRotation(Direction.Rotation().Quaternion());
                RadialTransform.SetScale3D(FVector(WallLength / RadialMeshSize.Y, 1.0f, 1.0f));
                
                CircularWalls->AddInstance(RadialTransform);
            }
        }
    }
}

void ACircularGrid::CalculateCellNeighbors(FLabyrinthCell& Cell)
{
    Cell.Neighbors.Empty(); // Clear cell neighbors

    if (Cell.Index == 0 && Cell.Ring == 0 && Cell.Sector == 0) // Check if is center cell
    {
        for (int i = 0; i < FMath::Pow(2.0f, FMath::FloorLog2(1) + SubdivisionFactor); i++)
        {
            Cell.Neighbors.Add(i + 1);  // Setup center cell neighbors
        }
    }
    else
    {
        const int32 Ring = Cell.Ring;
        const int32 Sector = Cell.Sector;
    
        // Setup right & left cell neighbors
        int32 CurrentSubdivisions = FMath::Pow(2.0f, FMath::FloorLog2(Ring) + SubdivisionFactor);
        const int32 LeftSector = (Sector - 1 + CurrentSubdivisions) % CurrentSubdivisions;
        const int32 RightSector = (Sector + 1) % CurrentSubdivisions;
    
        Cell.Neighbors.Add(GetCellIndex(Ring, LeftSector));
        Cell.Neighbors.Add(GetCellIndex(Ring, RightSector));

        // Setup cell parent neighbors
        if(Ring == 1)
        {
            Cell.Neighbors.Add(0); 
        }
        else if(Ring > 1)
        {
            CurrentSubdivisions = FMath::Pow(2.0f, FMath::FloorLog2(Ring) + SubdivisionFactor);
        
            if (CurrentSubdivisions >  FMath::Pow(2.0f, FMath::FloorLog2(Ring - 1) + SubdivisionFactor))
            {
                Cell.Neighbors.Add(GetCellIndex(Ring - 1, floor(Sector / 2)));
            }
            else
            {
                Cell.Neighbors.Add(GetCellIndex(Ring - 1, Sector));
            }
        }

        // Setup cell children neighbors
        if(Ring < MaxRings - 1)
        {
            CurrentSubdivisions = FMath::Pow(2.0f, FMath::FloorLog2(Ring) + SubdivisionFactor);
        
            if (CurrentSubdivisions <  FMath::Pow(2.0f, FMath::FloorLog2(Ring + 1) + SubdivisionFactor))
            {
                Cell.Neighbors.Add(GetCellIndex(Ring + 1, Sector * 2));
                Cell.Neighbors.Add(GetCellIndex(Ring + 1, Sector * 2 + 1));
            }
            else
            {
                Cell.Neighbors.Add(GetCellIndex(Ring + 1, Sector));
            }
            
        }
    }
    
}

void ACircularGrid::ClearVariables()
{
    for (UTextRenderComponent* TextComponent : InstancedTextRenderComponents)
    {
        TextComponent->DestroyComponent(); // Clear Instanced text component
    }
    InstancedTextRenderComponents.Empty();
}

int32 ACircularGrid::GetCurrentCell()
{
    for (FLabyrinthCell Cell : Cells)
    {
        if (Cell.bCurrent)
        {
            return Cell.Index; // loop through all cells and return the current one
        }
    }

    return 0;
}

int32 ACircularGrid::GetRingSubdivision(int32 Ring)
{
    return FMath::Pow(2.0f, FMath::FloorLog2(Ring) + SubdivisionFactor); // formula to get subdivision at a ring && with a subdivision factor
}

int32 ACircularGrid::GetCellIndex(int32 Ring, int32 Sector)
{
    if(Ring == 0) return 0; // center cell

    int32 Index = 1; 
    
    for(int32 r = 1; r < Ring; r++)
    {
        const int32 Subdivisions = FMath::Pow(2.0f, FMath::FloorLog2(r) + SubdivisionFactor);
        Index += Subdivisions;
    }
    
    const int32 CurrentSubdivisions = FMath::Pow(2.0f, FMath::FloorLog2(Ring) + SubdivisionFactor);
    return Index + (Sector % CurrentSubdivisions); // Return the cell index at a ring & sector given
}

void ACircularGrid::TestCellNeighbors(int32 index)
{
    for(int32 Neighbors : Cells[index].Neighbors)
    {
        FString DebugMessage = FString::Printf(TEXT("Selected Neighbor Index: %d"), Neighbors);
        GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, DebugMessage); // debug funct to check neighbors index of a cell
    }
    
}

FVector ACircularGrid::PolarToCartesian(float Radius, float Angle) const
{
    const float Radians = FMath::DegreesToRadians(Angle); // give location point perimeter with a specific radius and angle 
    return FVector(
        Radius * FMath::Cos(Radians),
        Radius * FMath::Sin(Radians),
        0.0f
    );
}

FVector ACircularGrid::CalculateCellLocation(int32 Ring, int32 Sector) const
{
    if(Ring == 0) return FVector::ZeroVector; // center cell

    // do the same logic same as grid point but with offset (*0.5) to get the center of the cell
    const float MiddleRadius = BaseRadius + ((Ring - 1) * RingSpacing) + (RingSpacing * 0.5f);
    
    const int32 Subdivisions = FMath::Pow(2.0f, FMath::FloorLog2(Ring) + SubdivisionFactor);
    const float AngleStep = 360.0f / Subdivisions;
    const float MidAngle = (Sector + 0.5f) * AngleStep;

    return PolarToCartesian(MiddleRadius, MidAngle);
}

void ACircularGrid::UpdateCellLocations()
{
    for(FLabyrinthCell& Cell : Cells)
    {
        Cell.Location = CalculateCellLocation(Cell.Ring, Cell.Sector); // set cell location at all cells
    }
}

void ACircularGrid::AddDebugTextRenderer(FVector TextLoc, FString TextMessage)
{
    UTextRenderComponent* TextRenderer = NewObject<UTextRenderComponent>(this);
    InstancedTextRenderComponents.Add(TextRenderer); // create new text component
    if (TextRenderer && DebugIndex)
    {
        TextRenderer->RegisterComponent();
        TextRenderer->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
        
        // set text location
        TextRenderer->SetWorldLocation(TextLoc + this->GetActorLocation());
        TextRenderer->SetWorldRotation(FRotator(90.0f, 0.0f, 180.0f));
        
        // Define text
        TextRenderer->SetText(FText::FromString(TextMessage));
        TextRenderer->SetHorizontalAlignment(EHTA_Center);
        TextRenderer->SetVerticalAlignment(EVRTA_TextCenter);
        TextRenderer->SetWorldSize(100.0f);
    }
}

void ACircularGrid::SetLabyrinthEntrance(ELabyrinthStart ELabyrinthEntrance)
{
    int32 PerimeterCellChosen;
    FLabyrinthCell& FirstCell = Cells[0];
    
    switch (ELabyrinthEntrance)
    {
        // Set center cell current & visited 
    case ELabyrinthStart::Center:
        
        FirstCell.bCurrent = true;
        FirstCell.bVisited = true;
        
        break;
        
        // Set a random perimeter cell current & visited
    case ELabyrinthStart::Perimeter:

        PerimeterCellChosen = GetRandomPerimeterCell();
        FLabyrinthCell& RandomCell = Cells[PerimeterCellChosen];
        
        RandomCell.bCurrent = true;
        RandomCell.bVisited = true;

        OpenPerimeterCell(Cells[PerimeterCellChosen]);
        
        break;
        
    }
}

void ACircularGrid::SetLabyrinthExit(ELabyrinthExit ELabyrinthExit)
{
    FLabyrinthCell& EndCell = Cells[0];

    // set center cell visited
    switch (ELabyrinthExit)
    {
    case ELabyrinthExit::Center:

        EndCell.bCurrent = false;
        EndCell.bVisited = true;
        
        break;
    }
}

int32 ACircularGrid::GetRandomPerimeterCell()
{
    TArray<int32> PossibleIndex;

    // return a random perimeter cell
    for (FLabyrinthCell& Cell : Cells)
    {
        if (Cell.Ring == MaxRings - 1)
        {
            PossibleIndex.Add(Cell.Index);
        }
    }

    
    return PossibleIndex[UKismetMathLibrary::RandomIntegerInRange(0, FMath::Clamp(PossibleIndex.Num() - 1, 0, PossibleIndex.Num() - 1))];
}

void ACircularGrid::RemoveWall(FLabyrinthCell Cell1, FLabyrinthCell Cell2)
{
    FHitResult HitResult;
    TArray<AActor*> ActorsToIgnore;
    
    // Cast a line between two cell and remove the wall between
    
    UKismetSystemLibrary::LineTraceSingle(
        GetWorld(),
        Cell1.Location + this->GetActorLocation(),
        Cell2.Location + this->GetActorLocation(),
        UEngineTypes::ConvertToTraceType(ECC_Visibility), 
        false,
        ActorsToIgnore,
        EDrawDebugTrace::Persistent,
        HitResult,
        true
    );

    CircularWalls->RemoveInstance(HitResult.Item);
}

void ACircularGrid::OpenPerimeterCell(FLabyrinthCell Cell)
{
    FHitResult HitResult;
    TArray<AActor*> ActorsToIgnore;

    // remove wall of a perimeter cell
    
    UKismetSystemLibrary::LineTraceSingle(
        GetWorld(),
        Cell.Location + this->GetActorLocation(),
        (((Cell.Location + this->GetActorLocation()) - this->GetActorLocation()).GetSafeNormal()) *  RingSpacing + Cell.Location + this->GetActorLocation(),
        UEngineTypes::ConvertToTraceType(ECC_Visibility),
        false,
        ActorsToIgnore,
        EDrawDebugTrace::None,
        HitResult,
        true
        );

    CircularWalls->RemoveInstance(HitResult.Item);
}

void ACircularGrid::UpdatePathLocalisation(FLabyrinthCell Cell)
{
    Path->ClearInstances();

    // move debug cube to show the path when the algorithm run
    
    FTransform MakeTransform;
    MakeTransform.SetLocation(Cell.Location + this->GetActorLocation());
    MakeTransform.SetRotation(FQuat(FRotator::ZeroRotator));
    MakeTransform.SetScale3D(FVector(1, 1, 1));
    
    Path->AddInstance(MakeTransform, true);
}

void ACircularGrid::StartRecursiveBacktracking()
{
    // funct to apply delay between recursive algo
    if (AnimationDelay <= 0)
    {
        AnimationDelay = 0.001;
    }
    GetWorld()->GetTimerManager().SetTimer(TimerHandleBacktracking, this, &ACircularGrid::RecursiveBacktrackingStep, AnimationDelay, true);
}

void ACircularGrid::RecursiveBacktrackingStep()
{
    // Open labyrinth exit wall when algo finished
    if (RecursiveBacktrackingFinished)
    {
        GetWorld()->GetTimerManager().ClearTimer(TimerHandleBacktracking);

        switch (EndPath)
        {
        case ELabyrinthExit::Center:
            OpenCenterCell(LongestPathCell);
            break;

        case ELabyrinthExit::Farest:
            OpenPerimeterCell(LongestPathCell);
            break;

        case ELabyrinthExit::RandomPerimeter:
            OpenPerimeterCell(Cells[GetRandomPerimeterCell()]);
            break;
            
        }
    }

    CurrentPathCell = Cells[GetCurrentCell()];
    
    ProgressPath();
   
    
}

void ACircularGrid::ProgressPath()
{
    FLabyrinthCell ChosenNeighbor;
    if (GetPotentialNextNeighbor(CurrentPathCell, ChosenNeighbor)) // Check potential current cell neighbors
    {
        // Neighbors found & update current cell to progress path
        
        NextPathCell = ChosenNeighbor;
        UpdatePathLocalisation(NextPathCell);
        RemoveWall(CurrentPathCell, NextPathCell);
        UpdateCurrentVisitedState(CurrentPathCell.Index, false, true);
        UpdateCurrentVisitedState(NextPathCell.Index, true, true);
        PathStackCells.Add(Cells[CurrentPathCell.Index]);

        // Update the longest path
        switch (EndPath)
        {
        case ELabyrinthExit::Center:
            FoundLongestPathAtRing(1);
            break;

        case ELabyrinthExit::Farest:
            FoundLongestPathAtRing(MaxRings - 1);
            break;

        case ELabyrinthExit::RandomPerimeter:
            break;
        }
        
    }
    else
    {
        // No neighbors found start backtracking 
        UpdateCurrentVisitedState(CurrentPathCell.Index, false, true);
        UpdatePathLocalisation(CurrentPathCell);
        if (!PathStackCells.IsEmpty())
        {
            CurrentPathCell = PathStackCells[PathStackCells.Num() - 1];
            UpdateCurrentVisitedState(CurrentPathCell.Index, true, true);
            UpdateCurrentVisitedState(NextPathCell.Index, false, true);
            PathStackCells.RemoveAt(PathStackCells.Num() - 1);
            ProgressPath();
        }
        else
        {
            RecursiveBacktrackingFinished = true;
        }
    }
}

bool ACircularGrid::GetPotentialNextNeighbor(FLabyrinthCell Cell, FLabyrinthCell& ChosenNeighbors)
{
    TArray<FLabyrinthCell> PotentialNeighbors;
    TArray<int32> NeighborIndices;

    // Get all potential cell neighbors in a list and return a random cell of this list
    for (int32 Index : Cells[Cell.Index].Neighbors)
    {
        if (!Cells[Index].bVisited)
        {
            PotentialNeighbors.Add(Cells[Index]);
            NeighborIndices.Add(Index);
        }
    }
    
   if (PotentialNeighbors.IsEmpty())
    {
        return false;
    }
    
    int32 rndIndex = UKismetMathLibrary::RandomIntegerInRange(0, FMath::Clamp(PotentialNeighbors.Num() - 1, 0, PotentialNeighbors.Num() - 1));
    ChosenNeighbors = PotentialNeighbors[rndIndex];
    
    return !PotentialNeighbors.IsEmpty();
    
}

void ACircularGrid::UpdateCurrentVisitedState(int32 CellIndex, bool Current, bool Visited)
{
    // Update the specified cell his current & visited state
    FLabyrinthCell& UpdatedCell = Cells[CellIndex];
    UpdatedCell.bCurrent = Current;
    UpdatedCell.bVisited = Visited;
}

void ACircularGrid::FoundLongestPathAtRing(int32 Ring)
{
    //override the longest registered path by the new one
    if (PathStackCells.Num() - 1  > LongestPath && Ring == NextPathCell.Ring)
    {
        LongestPath = PathStackCells.Num() - 1;
        LongestPathCell = Cells[NextPathCell.Index];
    }
}

void ACircularGrid::OpenCenterCell(FLabyrinthCell Cell)
{
    FHitResult HitResult;
    TArray<AActor*> ActorsToIgnore;
    
    // remove wall between center cell and the given input cell
    UKismetSystemLibrary::LineTraceSingle(
        GetWorld(),
        Cell.Location + this->GetActorLocation(),
        (((this->GetActorLocation() - (Cell.Location + this->GetActorLocation())).GetSafeNormal()) *  RingSpacing) + Cell.Location + this->GetActorLocation(),
        UEngineTypes::ConvertToTraceType(ECC_Visibility),
        false,
        ActorsToIgnore,
        EDrawDebugTrace::None,
        HitResult,
        true
        );

    CircularWalls->RemoveInstance(HitResult.Item);
}
