#include "Globals.h"  // NOTE: MSVC stupidness requires this to be the same across all modules

#include "Horse.h"
#include "../World.h"
#include "../EffectID.h"
#include "../Entities/Player.h"
#include "Broadcaster.h"
#include "UI/HorseWindow.h"





cHorse::cHorse(int Type, int Color, int Style) :
	super("Horse", mtHorse, "entity.horse.hurt", "entity.horse.death", 1.4, 1.6),
	cEntityWindowOwner(this),
	m_bHasChest(false),
	m_bIsEating(false),
	m_bIsRearing(false),
	m_bIsMouthOpen(false),
	m_bIsTame(false),
	m_bIsJumping(false),
	m_Type(Type),
	m_Color(Color),
	m_Style(Style),
	m_Temper(100),
	m_RearTickCount(0),
	m_TameAttempt(0)
{
	std::random_device rd;
	std::mt19937 gen(rd());
	std::normal_distribution<float> HealthGen(22.5f, 3.0f);
	std::normal_distribution<float> SpeedGen(0.225f, 0.02f);
	std::normal_distribution<float> JumpGen(0.7f, 0.1f);
	m_MaxSpeed = SpeedGen(gen);
	m_MaxHealth = HealthGen(gen);
	m_JumpStrength = JumpGen(gen);
	m_StartJump = false;
	
}




cHorse::~cHorse()
{
	auto Window = GetWindow();
	if (Window != nullptr)
	{
		Window->OwnerDestroyed();
	}
}





void cHorse::Tick(std::chrono::milliseconds a_Dt, cChunk & a_Chunk)
{

	super::Tick(a_Dt, a_Chunk);

	if (!IsTicking())
	{
		// The base class tick destroyed us
		return;  
	}
	auto & Random = GetRandomProvider();
	if (!m_bIsMouthOpen)
	{
		if (Random.RandBool(0.02))
		{
			m_bIsMouthOpen = true;
		}
	}
	else
	{
		if (Random.RandBool(0.10))
		{
			m_bIsMouthOpen = false;
		}
	}

	/*
	if (!m_bIsRearing) // Rear occasionally if not already rearing
	{
		if (Random.RandBool(.001))
		{
			m_RearTickCount = 0;
			m_bIsRearing = true;
		}
	}
	*/

	if (m_bIsJumping)
	{
		m_RearTickCount = 0;
	}

	if (jumpTimer > 10)
	{
		m_bIsJumping = false;
		jumpTimer = 0;
	}
	else
	{
		jumpTimer++;
	}

	if (IsMounted() && (!m_bIsTame))
	{
		if (m_TameAttempt >= m_Temper)
		{
			if (Random.RandBool(0.05))
			{
				m_World->BroadcastSoundParticleEffect(EffectID::PARTICLE_SMOKE, FloorC(GetPosX()), FloorC(GetPosY()), FloorC(GetPosZ()), int(SmokeDirection::SOUTH_EAST));
				m_World->BroadcastSoundParticleEffect(EffectID::PARTICLE_SMOKE, FloorC(GetPosX()), FloorC(GetPosY()), FloorC(GetPosZ()), int(SmokeDirection::SOUTH_WEST));
				m_World->BroadcastSoundParticleEffect(EffectID::PARTICLE_SMOKE, FloorC(GetPosX()), FloorC(GetPosY()), FloorC(GetPosZ()), int(SmokeDirection::NORTH_EAST));
				m_World->BroadcastSoundParticleEffect(EffectID::PARTICLE_SMOKE, FloorC(GetPosX()), FloorC(GetPosY()), FloorC(GetPosZ()), int(SmokeDirection::NORTH_WEST));

				m_World->BroadcastSoundEffect("entity.horse.angry", GetPosition(), 1.0f, 1.0f);
				m_Attachee->Detach();
				m_Temper += 5;
			}
		}
		else
		{
			m_World->GetBroadcaster().BroadcastParticleEffect("heart", static_cast<Vector3f>(GetPosition()), Vector3f{0, 1, 2}, 0, 10);
			m_bIsTame = true;
		}
	}
	/*if (m_bIsRearing)
	{
		
		if (m_RearTickCount >= 15)
		{
			m_bIsRearing = false;
			m_RearTickCount = 0;
		}
		else
		{
			m_RearTickCount++;
		}
	}*/

	m_World->BroadcastEntityMetadata(*this);
	
}





void cHorse::OnRightClicked(cPlayer & a_Player)
{
	m_World->BroadcastEntityProperties(*this);
	super::OnRightClicked(a_Player);

	int heal = 0;
	int temper = 0;
	int grow = 0;
	auto EquippedItemType = a_Player.GetEquippedItem().m_ItemType;
	switch (EquippedItemType)
	{
	case E_ITEM_SUGAR:
		heal = 1;
		grow = 30;
		temper = 3;
		break;
	case E_ITEM_WHEAT:
		heal = 2;
		grow = 20;
		temper = 3;
		break;
	case E_ITEM_RED_APPLE:
		heal = 3;
		grow = 60;
		temper = 3;
		break;
	case E_ITEM_GOLDEN_CARROT:
		heal = 4;
		grow = 60;
		temper = 5;
		break;
	case E_ITEM_GOLDEN_APPLE:
		heal = 10;
		grow = 240;
		temper = 10;
		break;
	case E_BLOCK_HAY_BALE:
		if (IsTame())
		{
			heal = 20;
			grow = 180;
		}
		break;
	}
	if (heal || temper || grow)  
	{
		m_bIsEating = true;
		Heal(heal);
		if (IsBaby())
		{
			m_AgingTimer -= grow * 20;
			if (m_AgingTimer <= 0)
			{
				m_AgingTimer = 1;
			}
		}
		if (!IsTame())
		{
			m_Temper += temper;
		}
		if (!a_Player.IsGameModeCreative())
		{
			a_Player.GetInventory().RemoveOneEquippedItem();
		}
		return;
	}

	if (m_bIsTame)
	{
		if (a_Player.IsCrouched())
		{
			PlayerOpenWindow(a_Player);
			return;
		}
		if (
			!IsSaddled() &&
			(
				(EquippedItemType == E_ITEM_SADDLE) ||
				ItemCategory::IsHorseArmor(EquippedItemType)
			)
		)
		{
			// Player is holding a horse inventory item, open the window:
			PlayerOpenWindow(a_Player);
		}
		else
		{
			StopMovingToPosition();
			a_Player.AttachTo(this);
		}
	}
	else if (a_Player.GetEquippedItem().IsEmpty())
	{
		// Check if leashed / unleashed to player before try to ride
		if (!m_IsLeashActionJustDone)
		{
			if (IsMounted())
			{
				if (m_Attachee->GetUniqueID() == a_Player.GetUniqueID())
				{
					a_Player.Detach();
					return;
				}

				if (m_Attachee->IsPlayer()) //Is someone else mounted?
				{
					return;
				}

				m_Attachee->Detach();
			}
			m_TameAttempt = GetRandomProvider().RandInt(99);
			StopMovingToPosition();
			a_Player.AttachTo(this);
		}
	}
	else
	{
		//m_bIsRearing = true; 
		m_RearTickCount = 0;
		m_World->BroadcastSoundEffect("entity.horse.angry", GetPosition(), 1.0f, 0.8f);
	}
}





void cHorse::SetHorseSaddle(cItem a_Saddle)
{
	if (a_Saddle.m_ItemType == E_ITEM_SADDLE)
	{
		m_World->BroadcastSoundEffect("entity.horse.saddle", GetPosition(), 1.0f, 0.8f);
	}
	else if (!a_Saddle.IsEmpty())
	{
		return;  // Invalid item
	}

	m_Saddle = std::move(a_Saddle);
	m_World->BroadcastEntityMetadata(*this);
}





void cHorse::SetHorseArmor(cItem a_Armor)
{
	if (ItemCategory::IsHorseArmor(a_Armor.m_ItemType))
	{
		m_World->BroadcastSoundEffect("entity.horse.armor", GetPosition(), 1.0f, 0.8f);
	}
	else if (!a_Armor.IsEmpty())
	{
		return;  // Invalid item
	}

	m_Armor = std::move(a_Armor);
	m_World->BroadcastEntityMetadata(*this);
}





int cHorse::GetHorseArmour(void) const
{
	switch (m_Armor.m_ItemType)
	{
		case E_ITEM_EMPTY:               return 0;
		case E_ITEM_IRON_HORSE_ARMOR:    return 1;
		case E_ITEM_GOLD_HORSE_ARMOR:    return 2;
		case E_ITEM_DIAMOND_HORSE_ARMOR: return 3;

		default:
		{
			LOGWARN("cHorse::GetHorseArmour: Invalid armour item (%d)", m_Armor.m_ItemType);
			return 0;
		}
	}
}





void cHorse::GetDrops(cItems & a_Drops, cEntity * a_Killer)
{
	if (IsBaby())
	{
		return;  // Babies don't drop items
	}

	unsigned int LootingLevel = 0;
	if (a_Killer != nullptr)
	{
		LootingLevel = a_Killer->GetEquippedWeapon().m_Enchantments.GetLevel(cEnchantments::enchLooting);
	}
	AddRandomDropItem(a_Drops, 0, 2 + LootingLevel, E_ITEM_LEATHER);
	if (IsSaddled())
	{
		a_Drops.push_back(m_Saddle);
	}
	if (!m_Armor.IsEmpty())
	{
		a_Drops.push_back(m_Armor);
	}
}





void cHorse::InStateIdle(std::chrono::milliseconds a_Dt, cChunk & a_Chunk)
{
	// If horse is tame and someone is sitting on it, don't walk around
	if (IsMounted() && IsTame())
	{
		return;
	}
	super::InStateIdle(a_Dt, a_Chunk);
}





void cHorse::HandleSpeedFromAttachee(float a_Forward, float a_Sideways)
{
	if ((m_bIsTame) && IsSaddled())
	{
		float Forward = 0;
		float Sideways = 0;

		// If going backwards, walk slower
		if (a_Forward < 0.0f)
		{
			Forward = -1;
		}
		else if (a_Forward > 0)
		{
			Forward = m_MaxSpeed * 43.0f;
		}

		if (a_Sideways < 0)
		{
			Sideways = -3;
		}
		else if (a_Sideways > 0)
		{
			Sideways = 3;
		}
		super::HandleSpeedFromAttachee(Forward, Sideways);
	}
	
}




void cHorse::HandlePhysics(std::chrono::milliseconds a_Dt, cChunk & a_Chunk)
{
	super::HandlePhysics(a_Dt, a_Chunk);
}




void cHorse::HandleJumpFromAttachee(float a_JumpPower)
{
	if (m_bIsJumping)
	{
		return;
	}
	if (m_bIsTame && IsSaddled() && IsOnGround())
	{
		jumpTimer = 0;
		m_bIsJumping = true;
		m_StartJump = true;
		//m_bIsRearing = true;
		//super::HandleJumpFromAttachee((-0.1817584952 * pow(m_JumpStrength, 3)) + (3.689713992 * pow(m_JumpStrength, 2)) + (2.128599134 * m_JumpStrength) - -0.343930367);
		super::HandleJumpFromAttachee(m_JumpStrength * (a_JumpPower / 100.0));
		LOG("Jump strength: %f    Power: %f", m_JumpStrength, a_JumpPower);
	}
}





void cHorse::PlayerOpenWindow(cPlayer & a_Player)
{
	auto Window = GetWindow();
	if (Window == nullptr)
	{
		Window = new cHorseWindow(*this);
		OpenWindow(Window);
	}

	a_Player.OpenWindow(*Window);
}
