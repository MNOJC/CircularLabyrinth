// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SLabyrinthCell.generated.h"

USTRUCT(BlueprintType)
struct FLabyrinthCell
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Index;

	UPROPERTY()
	int32 Ring;

	UPROPERTY()
	int32 Sector;

	UPROPERTY()
	FVector Location;

	UPROPERTY()
	TArray<int32> Neighbors;

	UPROPERTY()
	bool bCurrent;

	UPROPERTY()
	bool bVisited;
	
};
