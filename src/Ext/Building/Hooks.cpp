#include "Body.h"

#include <BulletClass.h>
#include <UnitClass.h>
#include <SuperClass.h>
#include <GameOptionsClass.h>
#include <Ext/House/Body.h>
#include <Ext/WarheadType/Body.h>
#include <TacticalClass.h>

//After TechnoClass_AI?
DEFINE_HOOK(0x43FE69, BuildingClass_AI, 0xA)
{
	GET(BuildingClass*, pThis, ESI);

	// Do not search this up again in any functions called here because it is costly for performance - Starkku
	auto pExt = BuildingExt::ExtMap.Find(pThis);

	/*
	// Set only if unset or type has changed - Not currently useful as building type does not change.
	auto pType = pThis->Type;

	if (!pExt->TypeExtData || pExt->TypeExtData->OwnerObject() != pType)
		pExt->TypeExtData = BuildingTypeExt::ExtMap.Find(pType);
	*/

	if (RulesExt::Global()->DisplayIncome_AllowAI || pThis->Owner->IsControlledByHuman())
		pExt->DisplayIncomeString();
	pExt->ApplyPoweredKillSpawns();

	return 0;
}

DEFINE_HOOK(0x4403D4, BuildingClass_AI_ChronoSparkle, 0x6)
{
	enum { SkipGameCode = 0x44055D };

	GET(BuildingClass*, pThis, ESI);

	if (RulesClass::Instance->ChronoSparkle1)
	{
		auto const displayPositions = RulesExt::Global()->ChronoSparkleBuildingDisplayPositions;
		auto const pType = pThis->Type;
		bool displayOnBuilding = (displayPositions & ChronoSparkleDisplayPosition::Building) != ChronoSparkleDisplayPosition::None;
		bool displayOnSlots = (displayPositions & ChronoSparkleDisplayPosition::OccupantSlots) != ChronoSparkleDisplayPosition::None;
		bool displayOnOccupants = (displayPositions & ChronoSparkleDisplayPosition::Occupants) != ChronoSparkleDisplayPosition::None;
		int occupantCount = displayOnSlots ? pType->MaxNumberOccupants : pThis->GetOccupantCount();
		bool showOccupy = occupantCount && (displayOnOccupants || displayOnSlots);

		if (showOccupy)
		{
			for (int i = 0; i < occupantCount; i++)
			{
				if (!((Unsorted::CurrentFrame + i) % RulesExt::Global()->ChronoSparkleDisplayDelay))
				{
					auto muzzleOffset = pType->MaxNumberOccupants <= 10 ? pType->MuzzleFlash[i] : BuildingTypeExt::ExtMap.Find(pType)->OccupierMuzzleFlashes.at(i);
					auto offset = Point2D::Empty;
					auto coords = CoordStruct::Empty;
					auto const renderCoords = pThis->GetRenderCoords();
					offset = *TacticalClass::Instance->ApplyMatrix_Pixel(&offset, &muzzleOffset);
					coords.X += offset.X;
					coords.Y += offset.Y;
					coords += renderCoords;

					if (auto const pAnim = GameCreate<AnimClass>(RulesClass::Instance->ChronoSparkle1, coords))
						pAnim->ZAdjust = -200;
				}
			}
		}

		if ((!showOccupy || displayOnBuilding) && !(Unsorted::CurrentFrame % RulesExt::Global()->ChronoSparkleDisplayDelay))
		{
			GameCreate<AnimClass>(RulesClass::Instance->ChronoSparkle1, pThis->GetCenterCoords());
		}

	}

	return SkipGameCode;
}

DEFINE_HOOK(0x7396D2, UnitClass_TryToDeploy_Transfer, 0x5)
{
	GET(UnitClass*, pUnit, EBP);
	GET(BuildingClass*, pStructure, EBX);

	if (pUnit->Type->DeployToFire && pUnit->Target)
		pStructure->LastTarget = pUnit->Target;

	if (auto pStructureExt = BuildingExt::ExtMap.Find(pStructure))
		pStructureExt->DeployedTechno = true;

	return 0;
}

DEFINE_HOOK(0x449ADA, BuildingClass_MissionConstruction_DeployToFireFix, 0x0)
{
	GET(BuildingClass*, pThis, ESI);

	auto pExt = BuildingExt::ExtMap.Find(pThis);
	if (pExt && pExt->DeployedTechno && pThis->LastTarget)
	{
		pThis->Target = pThis->LastTarget;
		pThis->QueueMission(Mission::Attack, false);
	}
	else
	{
		pThis->QueueMission(Mission::Guard, false);
	}

	return 0x449AE8;
}

DEFINE_HOOK(0x4401BB, Factory_AI_PickWithFreeDocks, 0x6)
{
	GET(BuildingClass*, pBuilding, ESI);

	auto pRulesExt = RulesExt::Global();
	if (pRulesExt->AllowParallelAIQueues && !pRulesExt->ForbidParallelAIQueues_Aircraft)
		return 0;

	if (!pBuilding)
		return 0;

	HouseClass* pOwner = pBuilding->Owner;

	if (!pOwner)
		return 0;

	if (pOwner->Type->MultiplayPassive
		|| pOwner->IsCurrentPlayer()
		|| pOwner->IsNeutral())
		return 0;

	if (pBuilding->Type->Factory == AbstractType::AircraftType)
	{
		if (pBuilding->Factory
			&& !BuildingExt::HasFreeDocks(pBuilding))
		{
			if (auto pBldExt = BuildingExt::ExtMap.Find(pBuilding))
				pBldExt->UpdatePrimaryFactoryAI();
		}
	}

	return 0;
}

DEFINE_HOOK(0x44D455, BuildingClass_Mission_Missile_EMPPulseBulletWeapon, 0x8)
{
	GET(WeaponTypeClass*, pWeapon, EBP);
	GET_STACK(BulletClass*, pBullet, STACK_OFFSET(0xF0, -0xA4));

	pBullet->SetWeaponType(pWeapon);

	return 0;
}

DEFINE_HOOK(0x44224F, BuildingClass_ReceiveDamage_DamageSelf, 0x5)
{
	enum { SkipCheck = 0x442268 };

	REF_STACK(args_ReceiveDamage const, receiveDamageArgs, STACK_OFFSET(0x9C, 0x4));

	if (auto const pWHExt = WarheadTypeExt::ExtMap.Find(receiveDamageArgs.WH))
	{
		if (pWHExt->AllowDamageOnSelf)
			return SkipCheck;
	}

	return 0;
}

DEFINE_HOOK(0x4502F4, BuildingClass_Update_Factory_Phobos, 0x6)
{
	GET(BuildingClass*, pThis, ESI);
	HouseClass* pOwner = pThis->Owner;

	auto pRulesExt = RulesExt::Global();

	if (pOwner->Production && pRulesExt->AllowParallelAIQueues)
	{
		auto pOwnerExt = HouseExt::ExtMap.Find(pOwner);
		BuildingClass** currFactory = nullptr;

		switch (pThis->Type->Factory)
		{
		case AbstractType::BuildingType:
			currFactory = &pOwnerExt->Factory_BuildingType;
			break;
		case AbstractType::UnitType:
			currFactory = pThis->Type->Naval ? &pOwnerExt->Factory_NavyType : &pOwnerExt->Factory_VehicleType;
			break;
		case AbstractType::InfantryType:
			currFactory = &pOwnerExt->Factory_InfantryType;
			break;
		case AbstractType::AircraftType:
			currFactory = &pOwnerExt->Factory_AircraftType;
			break;
		default:
			break;
		}

		if (!currFactory)
		{
			Game::RaiseError(E_POINTER);
		}
		else if (!*currFactory)
		{
			*currFactory = pThis;
			return 0;
		}
		else if (*currFactory != pThis)
		{
			enum { Skip = 0x4503CA };

			switch (pThis->Type->Factory)
			{
			case AbstractType::BuildingType:
				if (pRulesExt->ForbidParallelAIQueues_Building)
					return Skip;
				break;
			case AbstractType::InfantryType:
				if (pRulesExt->ForbidParallelAIQueues_Infantry)
					return Skip;
				break;
			case AbstractType::AircraftType:
				if (pRulesExt->ForbidParallelAIQueues_Aircraft)
					return Skip;
				break;
			case AbstractType::UnitType:
				if (pThis->Type->Naval ? pRulesExt->ForbidParallelAIQueues_Navy : pRulesExt->ForbidParallelAIQueues_Vehicle)
					return Skip;
				break;
			default:
				break;
			}
		}
	}

	return 0;
}

DEFINE_HOOK(0x4CA07A, FactoryClass_AbandonProduction_Phobos, 0x8)
{
	GET(FactoryClass*, pFactory, ESI);

	auto pRulesExt = RulesExt::Global();

	if (!pRulesExt->AllowParallelAIQueues)
		return 0;

	auto const pOwnerExt = HouseExt::ExtMap.Find(pFactory->Owner);
	TechnoClass* pTechno = pFactory->Object;

	switch (pTechno->WhatAmI())
	{
	case AbstractType::Building:
		if (pRulesExt->ForbidParallelAIQueues_Building)
			pOwnerExt->Factory_BuildingType = nullptr;
		break;
	case AbstractType::Unit:
		if (!pTechno->GetTechnoType()->Naval)
		{
			if (pRulesExt->ForbidParallelAIQueues_Vehicle)
				pOwnerExt->Factory_VehicleType = nullptr;
		}
		else
		{
			if (pRulesExt->ForbidParallelAIQueues_Navy)
				pOwnerExt->Factory_NavyType = nullptr;
		}
		break;
	case AbstractType::Infantry:
		if (pRulesExt->ForbidParallelAIQueues_Infantry)
			pOwnerExt->Factory_InfantryType = nullptr;
		break;
	case AbstractType::Aircraft:
		if (pRulesExt->ForbidParallelAIQueues_Aircraft)
			pOwnerExt->Factory_AircraftType = nullptr;
		break;
	default:
		break;
	}

	return 0;
}

DEFINE_HOOK(0x444119, BuildingClass_KickOutUnit_UnitType_Phobos, 0x6)
{
	GET(UnitClass*, pUnit, EDI);
	GET(BuildingClass*, pFactory, ESI);

	auto pHouseExt = HouseExt::ExtMap.Find(pFactory->Owner);

	if (pUnit->Type->Naval)
		pHouseExt->Factory_NavyType = nullptr;
	else
		pHouseExt->Factory_VehicleType = nullptr;

	return 0;
}

DEFINE_HOOK(0x444131, BuildingClass_KickOutUnit_InfantryType_Phobos, 0x6)
{
	GET(HouseClass*, pHouse, EAX);

	HouseExt::ExtMap.Find(pHouse)->Factory_InfantryType = nullptr;

	return 0;
}

DEFINE_HOOK(0x44531F, BuildingClass_KickOutUnit_BuildingType_Phobos, 0xA)
{
	GET(HouseClass*, pHouse, EAX);

	HouseExt::ExtMap.Find(pHouse)->Factory_BuildingType = nullptr;

	return 0;
}

DEFINE_HOOK(0x443CCA, BuildingClass_KickOutUnit_AircraftType_Phobos, 0xA)
{
	GET(HouseClass*, pHouse, EDX);

	HouseExt::ExtMap.Find(pHouse)->Factory_AircraftType = nullptr;

	return 0;
}

// Ares didn't have something like 0x7397E4 in its UnitDelivery code
DEFINE_HOOK(0x44FBBF, CreateBuildingFromINIFile_AfterCTOR_BeforeUnlimbo, 0x8)
{
	GET(BuildingClass* const, pBld, ESI);

	if (auto pExt = BuildingExt::ExtMap.Find(pBld))
		pExt->IsCreatedFromMapFile = true;

	return 0;
}

DEFINE_HOOK(0x440B4F, BuildingClass_Unlimbo_SetShouldRebuild, 0x5)
{
	enum { ContinueCheck = 0x440B58, SkipSetShouldRebuild = 0x440B81 };
	GET(BuildingClass* const, pThis, ESI);

	if (SessionClass::IsCampaign())
	{
		// Preplaced structures are already managed before
		if (BuildingExt::ExtMap.Find(pThis)->IsCreatedFromMapFile)
			return SkipSetShouldRebuild;

		// Per-house dehardcoding: BaseNodes + SW-Delivery
		if (!HouseExt::ExtMap.Find(pThis->Owner)->RepairBaseNodes[GameOptionsClass::Instance->Difficulty])
			return SkipSetShouldRebuild;
	}
	// Vanilla instruction: always repairable in other game modes
	return ContinueCheck;
}

DEFINE_HOOK(0x440EBB, BuildingClass_Unlimbo_NaturalParticleSystem_CampaignSkip, 0x5)
{
	enum { DoNotCreateParticle = 0x440F61 };
	GET(BuildingClass* const, pThis, ESI);
	return BuildingExt::ExtMap.Find(pThis)->IsCreatedFromMapFile ? DoNotCreateParticle : 0;
}

// Note:
/*
Ares has a hook at 0x4571E0 (the beginning of BuildingClass::Infiltrate) and completely overwrites the function.
Our logic has to be executed at the end (0x4575A2). The hook there assumes that registers have the exact content
they had in the beginning (when Ares hook started, executed, and jumped) in order to work when Ares logic is used.

However, this will fail if Ares is not involved (either DLL not included or with SpyEffect.Custom=no on BuildingType),
because by the time we reach our hook, the registers will be different and we'll be reading garbage. That's why
there is a second hook at 0x45759D, which is only executed when Ares doesn't jump over this function. There,
we execute our custom logic and then use EAX (which isn't used later, so it's safe to write to it) to "mark"
that we're done with 0x77777777. This way, when we reach the other hook, we check for this very specific value
to prevent spy effects from happening twice.

The value itself doesn't matter, it just needs to be unique enough to not be accidentally produced by the game there.
*/
constexpr int INFILTRATE_HOOK_MAGIC = 0x77777777;

DEFINE_HOOK(0x45759D, BuildingClass_Infiltrate_NoAres, 0x5)
{
	GET_STACK(HouseClass*, pInfiltratorHouse, STACK_OFFSET(0x14, -0x4));
	GET(BuildingClass*, pBuilding, EBP);

	BuildingExt::HandleInfiltrate(pBuilding, pInfiltratorHouse);
	R->EAX<int>(INFILTRATE_HOOK_MAGIC);
	return 0;
}

DEFINE_HOOK(0x4575A2, BuildingClass_Infiltrate_AfterAres, 0xE)
{
	// Check if we've handled it already
	if (R->EAX<int>() == INFILTRATE_HOOK_MAGIC)
	{
		R->EAX<int>(0);
		return 0;
	}

	GET_STACK(HouseClass*, pInfiltratorHouse, -0x4);
	GET(BuildingClass*, pBuilding, ECX);

	BuildingExt::HandleInfiltrate(pBuilding, pInfiltratorHouse);
	return 0;
}


DEFINE_HOOK(0x43D6E5, BuildingClass_Draw_ZShapePointMove, 0x5)
{
	enum { Apply = 0x43D6EF, Skip = 0x43D712 };

	GET(BuildingClass*, pThis, ESI);
	GET(Mission, mission, EAX);

	if ((mission != Mission::Selling && mission != Mission::Construction) || BuildingTypeExt::ExtMap.Find(pThis->Type)->ZShapePointMove_OnBuildup)
		return Apply;

	return Skip;
}

DEFINE_HOOK(0x4511D6, BuildingClass_AnimationAI_SellBuildup, 0x7)
{
	enum { Skip = 0x4511E6, Continue = 0x4511DF };

	GET(BuildingClass*, pThis, ESI);

	auto const pTypeExt = BuildingTypeExt::ExtMap.Find(pThis->Type);

	return pTypeExt->SellBuildupLength == pThis->Animation.Value ? Continue : Skip;
}

//Addition to Ares' Extras
DEFINE_HOOK(0x6F5347, TechnoClass_DrawExtras_OfflinePlants, 0x7)
{
	GET(TechnoClass*, pThis, EBP);
	GET_STACK(RectangleStruct*, pRect, 0xA0);

	if(auto pBld = abstract_cast<BuildingClass*>(pThis))
	{
		if (!RulesExt::Global()->DrawPowerOffline)
		{
			R->ESI(pRect);
			return 0x6F534E;
		}

		const auto pBldExt = BuildingTypeExt::ExtMap.Find(pBld->Type);
		bool showLowPower = FileSystem::POWEROFF_SHP
			&& (pBld->Type->PowerBonus > 0)
			&& (pBld->Factory == nullptr)
			&& (pBld->IsPowerOnline() == false || pBld->IsBeingDrained())
			&& (pBld->IsBeingWarpedOut() == false)
			&& (pBld->WarpingOut == false)
			&& (pBldExt->DisablePowerOfflineIcon == false)
			&& ((pBld->GetCurrentMission() != Mission::Selling) && (pBld->GetCurrentMission() != Mission::Construction))
			&& (pBld->CloakState == CloakState::Uncloaked);

		if (!showLowPower || MapClass::Instance->GetCellAt(pBld->GetMapCoords())->IsShrouded())
		{
			R->ESI(pRect);
			return 0x6F534E;
		}

		Point2D nPoint;
		TacticalClass::Instance->CoordsToClient(pBld->GetRenderCoords(), &nPoint);

		nPoint.Y += 22; // wrench offset
		nPoint.Y -= RulesExt::Global()->DrawPowerOffline_Offset;

		const int speed = max(GameOptionsClass::Instance->GetAnimSpeed(14) / 4, 2);
		const int frame = (FileSystem::POWEROFF_SHP->Frames * (Unsorted::CurrentFrame % speed)) / speed;
		DSurface::Temp->DrawSHP(FileSystem::MOUSE_PAL, FileSystem::POWEROFF_SHP, frame, &nPoint, pRect, BlitterFlags(0xE00), 0, 0, ZGradient::Ground, 1000, 0, 0, 0, 0, 0);
	}
	R->ESI(pRect);
	return 0x6F534E;
}
