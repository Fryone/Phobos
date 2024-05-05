#include <UnitClass.h>
#include <InfantryClass.h>
#include <TechnoClass.h>

#include <Ext/TechnoType/Body.h>
#include <Utilities/EnumFunctions.h>
#include <Utilities/GeneralUtils.h>
#include <Utilities/Macro.h>

namespace UnitDeployConvertHelpers
{
	void RemoveDeploying(REGISTERS* R);
	void ChangeAmmo(REGISTERS* R);
	void ChangeAmmoOnUnloading(REGISTERS* R);
}

void UnitDeployConvertHelpers::RemoveDeploying(REGISTERS* R)
{
	GET(TechnoClass*, pThis, ESI);
	auto const pThisType = TechnoTypeExt::ExtMap.Find(pThis->GetTechnoType());

	const bool canDeploy = pThis->CanDeploySlashUnload();
	R->AL(canDeploy);
	if (!canDeploy)
		return;

	const bool skipMinimum = pThisType->Ammo_DeployUnlockMinimumAmount < 0;
	const bool skipMaximum = pThisType->Ammo_DeployUnlockMaximumAmount < 0;

	if (skipMinimum && skipMaximum)
		return;

	const bool moreThanMinimum = pThis->Ammo >= pThisType->Ammo_DeployUnlockMinimumAmount;
	const bool lessThanMaximum = pThis->Ammo <= pThisType->Ammo_DeployUnlockMaximumAmount;

	if ((skipMinimum || moreThanMinimum) && (skipMaximum || lessThanMaximum))
		return;

	R->AL(false);
}

void UnitDeployConvertHelpers::ChangeAmmo(REGISTERS* R)
{
	GET(UnitClass*, pThis, ECX);
	auto const pThisExt = TechnoTypeExt::ExtMap.Find(pThis->Type);

	if (pThis->Deployed && !pThis->Deploying && pThisExt->Ammo_AddOnDeploy)
	{
		const int ammoCalc = std::max(pThis->Ammo + pThisExt->Ammo_AddOnDeploy, 0);
		pThis->Ammo = std::min(pThis->Type->Ammo, ammoCalc);
	}

	R->EAX(pThis->Type);
}

void UnitDeployConvertHelpers::ChangeAmmoOnUnloading(REGISTERS* R)
{
	GET(UnitClass*, pThis, ESI);
	auto const pThisExt = TechnoTypeExt::ExtMap.Find(pThis->Type);

	if (pThis->Type->IsSimpleDeployer && pThisExt->Ammo_AddOnDeploy && (pThis->Type->UnloadingClass == nullptr))
	{
		const int ammoCalc = std::max(pThis->Ammo + pThisExt->Ammo_AddOnDeploy, 0);
		pThis->Ammo = std::min(pThis->Type->Ammo, ammoCalc);
	}

	R->AL(pThis->Deployed);
}

DEFINE_HOOK(0x73FFE6, UnitClass_WhatAction_RemoveDeploying, 0xA)
{
	enum { Continue = 0x73FFF0 };
	UnitDeployConvertHelpers::RemoveDeploying(R);
	return Continue;
}

DEFINE_HOOK(0x730C70, DeployClass_Execute_RemoveDeploying, 0xA)
{
	enum { Continue = 0x730C7A };
	GET(TechnoClass*, pThis, ESI);

	if (abstract_cast<UnitClass*>(pThis) || abstract_cast<InfantryClass*>(pThis))
		UnitDeployConvertHelpers::RemoveDeploying(R);
	else
		R->AL(pThis->CanDeploySlashUnload());

	return Continue;
}

DEFINE_HOOK(0x739C74, UnitClass_ToggleDeployState_ChangeAmmo, 0x6) // deploying
{
	enum { Continue = 0x739C7A };
	UnitDeployConvertHelpers::ChangeAmmo(R);
	return Continue;
}

DEFINE_HOOK(0x739E5A, UnitClass_ToggleSimpleDeploy_ChangeAmmo, 0x6) // undeploying
{
	enum { Continue = 0x739E60 };
	UnitDeployConvertHelpers::ChangeAmmo(R);
	return Continue;
}

DEFINE_HOOK(0x73DE78, UnitClass_Unload_ChangeAmmo, 0x6) // converters
{
	enum { Continue = 0x73DE7E };
	UnitDeployConvertHelpers::ChangeAmmoOnUnloading(R);
	return Continue;
}

DEFINE_HOOK(0x51EB9A, InfantryClass_WhatAction_RemoveDeploying, 0x6)
{
	enum { Continue = 0x51EBA0, NoDeploy = 0x51ED00 };
	GET(InfantryClass*, pThis, EDI);
	auto const pThisType = TechnoTypeExt::ExtMap.Find(pThis->GetTechnoType());

	const bool isDeployer = pThis->Type->Deployer;
	R->AL(isDeployer);
	if (!isDeployer)
		return Continue;

	const bool skipMinimum = pThisType->Ammo_DeployUnlockMinimumAmount < 0;
	const bool skipMaximum = pThisType->Ammo_DeployUnlockMaximumAmount < 0;

	if (skipMinimum && skipMaximum)
		return Continue;

	const bool moreThanMinimum = pThis->Ammo >= pThisType->Ammo_DeployUnlockMinimumAmount;
	const bool lessThanMaximum = pThis->Ammo <= pThisType->Ammo_DeployUnlockMaximumAmount;

	if ((skipMinimum || moreThanMinimum) && (skipMaximum || lessThanMaximum))
		return Continue;

	return NoDeploy;
}

DEFINE_HOOK(0x51F702, InfantryClass_Unload_ChangeAmmo, 0xA) //undeploy
{
	enum { Continue = 0x51F716, Skip = 0x51F738 };
	GET(InfantryClass*, pThis, ESI);
	GET(int, currentSequence, EAX);

	auto const pThisExt = TechnoTypeExt::ExtMap.Find(pThis->Type);

	if (pThis->Type->Deployer && pThisExt->Ammo_AddOnDeploy)
	{
		const int ammoCalc = std::max(pThis->Ammo + pThisExt->Ammo_AddOnDeploy, 0);
		pThis->Ammo = std::min(pThis->Type->Ammo, ammoCalc);
	}

	if(currentSequence == 27 || currentSequence == 28 || currentSequence == 29 || currentSequence == 30 )
		return Continue;
	return Skip;
}

DEFINE_HOOK(0x443737, BuildingClass_ActiveClickWith_RemoveUnloading, 0x6)
{
	enum { Continue = 0x44373D, Skip = 0x44384E };
	GET(BuildingClass*, pBld, ECX);
	GET(int, act, EAX);

	if(act != 1) //not moving command
		return Skip;

	auto const pThisType = TechnoTypeExt::ExtMap.Find(pBld->Type);
	const bool skipMinimum = pThisType->Ammo_DeployUnlockMinimumAmount < 0;
	const bool skipMaximum = pThisType->Ammo_DeployUnlockMaximumAmount < 0;

	if (skipMinimum && skipMaximum)
		return Continue;

	const bool moreThanMinimum = pBld->Ammo >= pThisType->Ammo_DeployUnlockMinimumAmount;
	const bool lessThanMaximum = pBld->Ammo <= pThisType->Ammo_DeployUnlockMaximumAmount;

	if ((skipMinimum || moreThanMinimum) && (skipMaximum || lessThanMaximum))
		return Continue;

	return Skip;
}
