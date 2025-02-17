#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "CircularGrid.generated.h"


USTRUCT(BlueprintType)
struct FCell
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cell Data")
	int32 Index;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cell Data")
	int32 Ring;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cell Data")
	int32 Sector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cell Data")
	FVector Location;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cell Data")
	TArray<int32> Neighbors;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cell Data")
	bool bCurrent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cell Data")
	bool bVisited;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cell Data")
	TArray<int32> WallInstances; // Index des instances HISM des murs

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cell Data")
	TArray<int32> PillarInstances; // Index des instances HISM des piliers
	
};

UCLASS()
class CIRCULARLABYRINTH_API ACircularGrid : public AActor
{
	GENERATED_BODY()

public:
	ACircularGrid();

	UPROPERTY(EditAnywhere, Category = "Grid Settings")
	int32 MaxRings = 3;

	UPROPERTY(EditAnywhere, Category = "Grid Settings")
	int32 SubdivisionFactor = 1;

	UPROPERTY(EditAnywhere, Category = "Grid Settings")
	float BaseRadius = 500.0f;

	UPROPERTY(EditAnywhere, Category = "Grid Settings")
	float RingSpacing = 200.0f;

	UPROPERTY(EditAnywhere, Category = "Grid Settings")
	float MeshLength;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UHierarchicalInstancedStaticMeshComponent* CircularWalls;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UHierarchicalInstancedStaticMeshComponent* RadialWalls;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UHierarchicalInstancedStaticMeshComponent* Pillars;
	
	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grid Data")
	TArray<FCell> Cells;

	UFUNCTION(BlueprintCallable)
	int32 GetCellIndex(int32 Ring, int32 Sector);

private:

	TMap<int32, int32> WallInstanceMap;
	TMap<int32, int32> PillarInstanceMap;
	TArray<UTextRenderComponent*> InstancedTextRenderComponents;

	void GenerateGrid();
	void GenerateGeometry();
	void CalculateCellNeighbors(FCell& Cell);
	void ClearVariables();
	
	FVector PolarToCartesian(float Radius, float Angle) const;
	
	FVector CalculateCellLocation(int32 Ring, int32 Sector) const;
	void UpdateCellLocations();

	void AddDebugTextRenderer(FVector TextLoc, FString TextMessage);
};
