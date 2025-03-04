#pragma once

#include "CoreMinimal.h"
#include "ELabyrinthExit.h"
#include "ELabyrinthStart.h"
#include "SLabyrinthCell.h"
#include "Kismet/KismetArrayLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GameFramework/Actor.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "CircularGrid.generated.h"

UCLASS()
class CIRCULARLABYRINTH_API ACircularGrid : public AActor
{
	GENERATED_BODY()

public:
	ACircularGrid();

	virtual void BeginPlay() override;

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

	UPROPERTY(EditAnywhere, Category = "Grid Settings")
	FRandomStream Seed;

	UPROPERTY(EditAnywhere, Category = "Grid Settings")
	ELabyrinthStart StartPath;
	
	UPROPERTY(EditAnywhere, Category = "Grid Settings")
	ELabyrinthExit EndPath;

	UPROPERTY(EditAnywhere, Category = "Grid Settings")
	float AnimationDelay = 0.0f;
	
	UPROPERTY(EditAnywhere, Category = "Grid Settings")
	bool DebugIndex;

	UPROPERTY(EditDefaultsOnly)
	UHierarchicalInstancedStaticMeshComponent* CircularWalls;

	UPROPERTY(EditDefaultsOnly)
	UHierarchicalInstancedStaticMeshComponent* Pillars;

	UPROPERTY(EditDefaultsOnly)
	UHierarchicalInstancedStaticMeshComponent* Path;
	
	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY(BlueprintReadWrite, Category = "Grid Data")
	TArray<FLabyrinthCell> Cells;

	UFUNCTION(BlueprintCallable)
	int32 GetCellIndex(int32 Ring, int32 Sector);

	UFUNCTION(BlueprintCallable)
	void TestCellNeighbors(int32 index);



private:
	
	TArray<UTextRenderComponent*> InstancedTextRenderComponents;
	FTimerHandle TimerHandleBacktracking;

	FLabyrinthCell CurrentPathCell;
	FLabyrinthCell NextPathCell;

	TArray<FLabyrinthCell> PathStackCells;

	int32 LongestPath;
	FLabyrinthCell LongestPathCell;

	void GenerateGrid();
	void GenerateGeometry();
	void CalculateCellNeighbors(FLabyrinthCell& Cell);
	void ClearVariables();
	int32 GetCurrentCell();

	bool RecursiveBacktrackingFinished = false;
	
	FVector PolarToCartesian(float Radius, float Angle) const;
	
	FVector CalculateCellLocation(int32 Ring, int32 Sector) const;
	void UpdateCellLocations();

	void AddDebugTextRenderer(FVector TextLoc, FString TextMessage);

	void SetLabyrinthEntrance(ELabyrinthStart ELabyrinthEntrance);
	void SetLabyrinthExit(ELabyrinthExit ELabyrinthExit);
	
	int32 GetRandomPerimeterCell();
	void RemoveWall(FLabyrinthCell Cell1, FLabyrinthCell Cell2);
	bool GetPotentialNextNeighbor(FLabyrinthCell Cell, FLabyrinthCell& ChosenNeighbors);
	void OpenPerimeterCell(FLabyrinthCell Cell);
	void UpdatePathLocalisation(FLabyrinthCell Cell);
	void UpdateCurrentVisitedState(int32 CellIndex, bool Current, bool Visited);

	void FoundLongestPathAtRing(int32 Ring);
	void OpenCenterCell(FLabyrinthCell Cell);

	void StartRecursiveBacktracking();
	void RecursiveBacktrackingStep();

	void ProgressPath();
};


