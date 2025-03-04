// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

UENUM(BlueprintType)
enum class ELabyrinthExit : uint8
{
	Center				UMETA(DisplayName="Center"),
	Farest				UMETA(DisplayName="Farest Perimeter"),
	RandomPerimeter		UMETA(DisplayName="Random Perimeter"),
};
