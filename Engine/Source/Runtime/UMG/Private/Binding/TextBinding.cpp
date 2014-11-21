// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UMGPrivatePCH.h"
#include "TextBinding.h"

#define LOCTEXT_NAMESPACE "UMG"

UTextBinding::UTextBinding()
{
}

bool UTextBinding::IsSupportedDestination(UProperty* Property) const
{
	return IsConcreteTypeCompatibleWithReflectedType<FText>(Property);
}

bool UTextBinding::IsSupportedSource(UProperty* Property) const
{
	return
		IsConcreteTypeCompatibleWithReflectedType<FText>(Property) ||
		IsConcreteTypeCompatibleWithReflectedType<FString>(Property);
}

void UTextBinding::Bind(UProperty* Property, FScriptDelegate* Delegate)
{
	if ( IsConcreteTypeCompatibleWithReflectedType<FText>(Property) )
	{
		static const FName BinderFunction(TEXT("GetTextValue"));
		Delegate->BindUFunction(this, BinderFunction);
	}
	else if ( IsConcreteTypeCompatibleWithReflectedType<FString>(Property) )
	{
		static const FName BinderFunction(TEXT("GetStringValue"));
		Delegate->BindUFunction(this, BinderFunction);
	}
}

FText UTextBinding::GetTextValue() const
{
	if ( UObject* Source = SourceObject.Get() )
	{
		if ( !bNeedsConversion.Get(false) )
		{
			FText TextValue;
			if ( SourcePath.GetValue<FText>(Source, TextValue) )
			{
				bNeedsConversion = false;
				return TextValue;
			}
		}

		if ( bNeedsConversion.Get(true) )
		{
			FString StringValue;
			if ( SourcePath.GetValue<FString>(Source, StringValue) )
			{
				bNeedsConversion = true;
				return FText::FromString(StringValue);
			}
		}
	}

	return FText::GetEmpty();
}

FString UTextBinding::GetStringValue() const
{
	if ( UObject* Source = SourceObject.Get() )
	{
		if ( !bNeedsConversion.Get(false) )
		{
			FString StringValue;
			if ( SourcePath.GetValue<FString>(Source, StringValue) )
			{
				bNeedsConversion = false;
				return StringValue;
			}
		}

		if ( bNeedsConversion.Get(true) )
		{
			FText TextValue;
			if ( SourcePath.GetValue<FText>(Source, TextValue) )
			{
				bNeedsConversion = true;
				return TextValue.ToString();
			}
		}
	}

	return FString();
}

#undef LOCTEXT_NAMESPACE
