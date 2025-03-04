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

    SetLabyrinthEntrance(StartPath);
    SetLabyrinthExit(EndPath);
    StartRecursiveBacktracking();
    
}

void ACircularGrid::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    ClearVariables();
    GenerateGrid();
    GenerateGeometry();
}



void ACircularGrid::GenerateGrid()
{
    Cells.Empty();
    
    int32 CellIndex = 0;

    FLabyrinthCell CenterCell;
    CenterCell.Index = CellIndex++;
    CenterCell.Ring = 0;
    CenterCell.Sector = 0;
    CenterCell.bCurrent = false;
    CenterCell.bVisited = false;
    AddDebugTextRenderer(FVector::Zero(), FString::FromInt(CenterCell.Index));
    
    Cells.Add(CenterCell);
    
    for(int32 Ring = 1; Ring < MaxRings; Ring++)
    {
        const int32 CurrentSubdivisions = FMath::Pow(2.0f, FMath::FloorLog2(Ring) + SubdivisionFactor);

        for(int32 Sector = 0; Sector < CurrentSubdivisions; Sector++)
        {
            FLabyrinthCell NewCell;
            NewCell.Index = CellIndex++;
            NewCell.Ring = Ring;
            NewCell.Sector = Sector;
            NewCell.Location = CalculateCellLocation(NewCell.Ring, NewCell.Sector);
            AddDebugTextRenderer(NewCell.Location,  FString::FromInt(NewCell.Index));
            
            Cells.Add(NewCell);
        }
    }

    for(FLabyrinthCell& Cell : Cells)
    {
        CalculateCellNeighbors(Cell);
    }
}

void ACircularGrid::GenerateGeometry()
{
    CircularWalls->ClearInstances();
    Pillars->ClearInstances();
    
    const int32 MaxSubdivisions = FMath::Pow(2.0f, FMath::FloorLog2(MaxRings) + SubdivisionFactor);
    const float BaseAngleStep = 360.0f / MaxSubdivisions;
    
    for(int32 Ring = 0; Ring < MaxRings; Ring++)
    {
        const int32 CurrentSubdivisions = FMath::Pow(2.0f, FMath::FloorLog2(Ring + 1) + SubdivisionFactor);
        const int32 SubdivisionRatio = MaxSubdivisions / CurrentSubdivisions;
        const float Radius = BaseRadius + Ring * RingSpacing;
        const float EffectiveAngleStep = 360.0f / CurrentSubdivisions;

        // Génération des murs circulaires
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

        // Génération des murs radiaux entre les anneaux
        if(Ring < MaxRings - 1)
        {
            const float NextRadius = Radius + RingSpacing;

            for(int32 Sector = 0; Sector < CurrentSubdivisions; Sector++)
            {
                // Calcul des angles avec décalage de demi-step si nécessaire
                const float CurrentAngle = Sector * SubdivisionRatio * BaseAngleStep;
                const float NextAngle = CurrentAngle;

                // Points de départ et d'arrivée
                const FVector StartInner = PolarToCartesian(Radius, CurrentAngle);
                const FVector EndOuter = PolarToCartesian(NextRadius, NextAngle);

                // Configuration du mur radial
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
    Cell.Neighbors.Empty();

    if (Cell.Index == 0 && Cell.Ring == 0 && Cell.Sector == 0)
    {
        for (int i = 0; i < FMath::Pow(2.0f, FMath::FloorLog2(1) + SubdivisionFactor); i++)
        {
            Cell.Neighbors.Add(i + 1);
        }
    }
    else
    {
        const int32 Ring = Cell.Ring;
        const int32 Sector = Cell.Sector;
    
        // Voisins latéraux (même anneau)
        int32 CurrentSubdivisions = FMath::Pow(2.0f, FMath::FloorLog2(Ring) + SubdivisionFactor);
        const int32 LeftSector = (Sector - 1 + CurrentSubdivisions) % CurrentSubdivisions;
        const int32 RightSector = (Sector + 1) % CurrentSubdivisions;
    
        Cell.Neighbors.Add(GetCellIndex(Ring, LeftSector));
        Cell.Neighbors.Add(GetCellIndex(Ring, RightSector));

        // Voisin intérieur (centre ou anneau précédent)
        if(Ring == 1)
        {
            Cell.Neighbors.Add(0); // Connexion directe au centre
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

        // Voisins extérieurs (anneau suivant)
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
        TextComponent->DestroyComponent();
    }
    InstancedTextRenderComponents.Empty();
}

int32 ACircularGrid::GetCurrentCell()
{
    for (FLabyrinthCell Cell : Cells)
    {
        if (Cell.bCurrent)
        {
            return Cell.Index;
        }
    }

    return 0;
}

int32 ACircularGrid::GetCellIndex(int32 Ring, int32 Sector)
{
    if(Ring == 0) return 0;

    int32 Index = 1; // Commence après la cellule centrale
    
    for(int32 r = 1; r < Ring; r++)
    {
        const int32 Subdivisions = FMath::Pow(2.0f, FMath::FloorLog2(r) + SubdivisionFactor);
        Index += Subdivisions;
    }
    
    const int32 CurrentSubdivisions = FMath::Pow(2.0f, FMath::FloorLog2(Ring) + SubdivisionFactor);
    return Index + (Sector % CurrentSubdivisions);
}

void ACircularGrid::TestCellNeighbors(int32 index)
{
    for(int32 Neighbors : Cells[index].Neighbors)
    {
        FString DebugMessage = FString::Printf(TEXT("Selected Neighbor Index: %d"), Neighbors);
        GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, DebugMessage);
    }
    
}

FVector ACircularGrid::PolarToCartesian(float Radius, float Angle) const
{
    const float Radians = FMath::DegreesToRadians(Angle);
    return FVector(
        Radius * FMath::Cos(Radians),
        Radius * FMath::Sin(Radians),
        0.0f
    );
}

FVector ACircularGrid::CalculateCellLocation(int32 Ring, int32 Sector) const
{
    if(Ring == 0) return FVector::ZeroVector;

    // Ajustement du calcul du rayon pour les anneaux extérieurs
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
        Cell.Location = CalculateCellLocation(Cell.Ring, Cell.Sector);
    }
}

void ACircularGrid::AddDebugTextRenderer(FVector TextLoc, FString TextMessage)
{
    UTextRenderComponent* TextRenderer = NewObject<UTextRenderComponent>(this);
    InstancedTextRenderComponents.Add(TextRenderer);
    if (TextRenderer && DebugIndex)
    {
        TextRenderer->RegisterComponent();
        TextRenderer->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
        
        // Positionner le texte dans le monde
        TextRenderer->SetWorldLocation(TextLoc + this->GetActorLocation());
        TextRenderer->SetWorldRotation(FRotator(90.0f, 0.0f, 180.0f));
        
        // Définir le texte
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
    case ELabyrinthStart::Center:
        
        FirstCell.bCurrent = true;
        FirstCell.bVisited = true;
        
        break;

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
    
    FTransform MakeTransform;
    MakeTransform.SetLocation(Cell.Location + this->GetActorLocation());
    MakeTransform.SetRotation(FQuat(FRotator::ZeroRotator));
    MakeTransform.SetScale3D(FVector(1, 1, 1));
    
    Path->AddInstance(MakeTransform, true);
}

void ACircularGrid::StartRecursiveBacktracking()
{
    if (AnimationDelay <= 0)
    {
        AnimationDelay = 0.001;
    }
    GetWorld()->GetTimerManager().SetTimer(TimerHandleBacktracking, this, &ACircularGrid::RecursiveBacktrackingStep, AnimationDelay, true);
}

void ACircularGrid::RecursiveBacktrackingStep()
{
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
    if (GetPotentialNextNeighbor(CurrentPathCell, ChosenNeighbor))
    {
        NextPathCell = ChosenNeighbor;
        UpdatePathLocalisation(NextPathCell);
        RemoveWall(CurrentPathCell, NextPathCell);
        UpdateCurrentVisitedState(CurrentPathCell.Index, false, true);
        UpdateCurrentVisitedState(NextPathCell.Index, true, true);
        PathStackCells.Add(Cells[CurrentPathCell.Index]);

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
    FLabyrinthCell& UpdatedCell = Cells[CellIndex];
    UpdatedCell.bCurrent = Current;
    UpdatedCell.bVisited = Visited;
}

void ACircularGrid::FoundLongestPathAtRing(int32 Ring)
{
    
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
