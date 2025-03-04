// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

UENUM(BlueprintType)
enum class ELabyrinthStart : uint8
{
	Center		UMETA(DisplayName="Center"),
	Perimeter	UMETA(DisplayName="Perimeter"),
};
