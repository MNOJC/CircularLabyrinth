#include "CircularGrid.h"
#include "Math/UnrealMathUtility.h"

ACircularGrid::ACircularGrid()
{
    PrimaryActorTick.bCanEverTick = false;

    CircularWalls = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("CircularWalls"));
    RadialWalls = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("RadialWalls"));
    Pillars = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Pillars"));

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
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

    FCell CenterCell;
    CenterCell.Index = CellIndex++;
    CenterCell.Ring = 0;
    CenterCell.Sector = 0;
    AddDebugTextRenderer(FVector::Zero(), FString::FromInt(CenterCell.Index));
    
    Cells.Add(CenterCell);
    
    for(int32 Ring = 1; Ring < MaxRings; Ring++)
    {
        const int32 CurrentSubdivisions = FMath::Pow(2.0f, FMath::FloorLog2(Ring) + SubdivisionFactor);

        for(int32 Sector = 0; Sector < CurrentSubdivisions; Sector++)
        {
            FCell NewCell;
            NewCell.Index = CellIndex++;
            NewCell.Ring = Ring;
            NewCell.Sector = Sector;
            NewCell.Location = CalculateCellLocation(NewCell.Ring, NewCell.Sector);
            AddDebugTextRenderer(NewCell.Location,  FString::FromInt(NewCell.Index));
            
            Cells.Add(NewCell);
        }
    }

    for(FCell& Cell : Cells)
    {
        CalculateCellNeighbors(Cell);
    }
}

void ACircularGrid::GenerateGeometry()
{
    CircularWalls->ClearInstances();
    RadialWalls->ClearInstances();
    Pillars->ClearInstances();

    WallInstanceMap.Empty();
    PillarInstanceMap.Empty();
    
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
            const int32 WallInstanceIndex = CircularWalls->AddInstance(Transform);
            
            const int32 CellIndex = GetCellIndex(Ring, Sector);
            Cells[CellIndex].WallInstances.Add(WallInstanceIndex);
            WallInstanceMap.Add(WallInstanceIndex, CellIndex);

            // Placement des piliers principaux
            const int32 PillarInstanceIndex = Pillars->AddInstance(FTransform(FRotator(0.0f, PillarAngle, 0.0f), LocationPillar, FVector(1.0f, 1.0f, 1.2f)));
            Cells[CellIndex].PillarInstances.Add(PillarInstanceIndex);
            PillarInstanceMap.Add(PillarInstanceIndex, CellIndex);
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
                const FVector RadialMeshSize = RadialWalls->GetStaticMesh()->GetBoundingBox().GetSize();
                
                FTransform RadialTransform;
                RadialTransform.SetLocation((StartInner + EndOuter) * 0.5f + this->GetActorLocation());
                RadialTransform.SetRotation(Direction.Rotation().Quaternion());
                RadialTransform.SetScale3D(FVector(WallLength / RadialMeshSize.Y, 1.0f, 1.0f));
                
                const int32 RadialInstanceIndex = RadialWalls->AddInstance(RadialTransform);
                const int32 CellIndex = GetCellIndex(Ring, Sector);
                Cells[CellIndex].WallInstances.Add(RadialInstanceIndex);
                WallInstanceMap.Add(RadialInstanceIndex, CellIndex);
            }
        }
    }
}

void ACircularGrid::CalculateCellNeighbors(FCell& Cell)
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
        int32 CurrentSubdivisions = 8 * FMath::Pow(2.0f, Ring - 1);
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
    for(FCell& Cell : Cells)
    {
        Cell.Location = CalculateCellLocation(Cell.Ring, Cell.Sector);
    }
}

void ACircularGrid::AddDebugTextRenderer(FVector TextLoc, FString TextMessage)
{
    UTextRenderComponent* TextRenderer = NewObject<UTextRenderComponent>(this);
    InstancedTextRenderComponents.Add(TextRenderer);
    if (TextRenderer)
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