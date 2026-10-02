#ifndef _WEEDSITEM_H
#define _WEEDSITEM_H

#include "ItemMgr.h"
#include "PikiAI.h"
#include "types.h"

/*
 * @brief TODO
 */
class Grass {
public:
	Grass() { }

	bool isAlive() { return mHealth != 0; }

	int nuku()
	{
		if (mHealth != 0) {
			mHealth = 0;
			return ACTOUT_Success;
		}
		return ACTOUT_Fail;
	}

	Vector3f mPosition;  // _00
	u8 mHealth;          // _0C
	u8 mGrassShapeId;    // _0D
	u8 mRotationDegrees; // _0E
};

/*
 * @brief TODO
 */
class Pebble {
public:
	Pebble() { }

	bool isAlive() { return mHealth != 0; }

	int attack()
	{
		if (mHealth != 0) {
			mHealth--;
			if (mHealth == 0) {
				return ACTOUT_Success;
			}
			return ACTOUT_Continue;
		}
		return ACTOUT_Fail;
	}

	// TODO: members
	Vector3f mPosition;  // _00
	u8 mRotationDegrees; // _0C, unknown
	u8 mShapeIndex;      // _0D
	u8 mHealth;          // _0E
};

/**
 * @brief TODO
 *
 * @note Size: 0x3E4.
 */
struct GrassGen : public ItemCreature {
#if defined(PIKI_PC_PORT)
 friend struct PcMiddayWorldAccess;
#endif
public:
	GrassGen(Shape*, CreatureProp*);

	virtual void startAI(int);                           // _34
	virtual f32 getSize() { return mSize; }              // _3C
	virtual void update();                               // _E0
	virtual void refresh(Graphics&);                     // _EC
	virtual bool isAlive() { return mActiveGrass != 0; } // _88
	virtual bool isVisible() { return true; }            // _74
	virtual bool needFlick(Creature*) { return false; }  // _94

	bool workable();
	void startWork();
	void finishWork();
	void resolve();
	void setSizeAndNum(f32, int);
	Grass* getRandomGrass();

	void killGrass() { mActiveGrass--; }

protected:
	void create(int, f32, int);

	// _00      = VTBL
	// _00-_3C8 = ItemCreature
	int mWorkingPikis
#if defined(PIKI_PC_PORT)
 {}
#endif
 ;    // _3C8
	Grass* mGrass
#if defined(PIKI_PC_PORT)
 {}
#endif
 ;        // _3CC
	u16 mActiveGrass
#if defined(PIKI_PC_PORT)
 {}
#endif
 ;     // _3D0
	u16 mTotalGrassCount
#if defined(PIKI_PC_PORT)
 {}
#endif
 ; // _3D2
	Vector3f _3D4;        // _3D4
	f32 mSize
#if defined(PIKI_PC_PORT)
 {}
#endif
 ;            // _3E0
};

/**
 * @brief TODO
 *
 * @note Size: 0x3E8
 */
struct RockGen : public ItemCreature {
#if defined(PIKI_PC_PORT)
 friend struct PcMiddayWorldAccess;
#endif
public:
	RockGen(Shape*, CreatureProp*);

	virtual void startAI(int);                             // _34
	virtual f32 getSize() { return mSize; }                // _3C
	virtual void doSave(RandomAccessStream&);              // _50
	virtual void doLoad(RandomAccessStream&);              // _54
	virtual void update();                                 // _E0
	virtual void refresh(Graphics&);                       // _EC
	virtual bool isAlive() { return mActivePebbles != 0; } // _88
	virtual bool isVisible() { return true; }              // _74
	virtual bool needFlick(Creature*) { return false; }    // _94

	bool workable();
	void startWork();
	void finishWork();
	void resolve();
	void setSizeAndNum(f32, int);
	void killPebble();
	Pebble* getRandomPebble();

protected:
	void create(int, f32, int);

	// _00      = VTBL
	// _00-_3C8 = ItemCreature
	int mWorkingPikis
#if defined(PIKI_PC_PORT)
 {}
#endif
 ;  // _3C8
	u8 _3CC
#if defined(PIKI_PC_PORT)
 {}
#endif
 ;            // _3CC
	Pebble* mPebbles
#if defined(PIKI_PC_PORT)
 {}
#endif
 ;   // _3D0
	u16 mActivePebbles
#if defined(PIKI_PC_PORT)
 {}
#endif
 ; // _3D4
	u16 mMaxPebbles
#if defined(PIKI_PC_PORT)
 {}
#endif
 ;    // _3D6
	Vector3f _3D8;      // _3D8
	f32 mSize
#if defined(PIKI_PC_PORT)
 {}
#endif
 ;          // _3E4
};

/*
 * @brief TODO
 */
struct WeedsGen : public ItemCreature {
	WeedsGen(Shape*, CreatureProp*);

	virtual void startAI(int);       // _34
	virtual void update();           // _E0
	virtual void refresh(Graphics&); // _EC

	// _00      = VTBL
	// _00-_3C8 = ItemCreature
	int mWeedsCount
#if defined(PIKI_PC_PORT)
 {}
#endif
 ;              // _3C8
	Shape* mWeedShape
#if defined(PIKI_PC_PORT)
 {}
#endif
 ;            // _3CC
	CreatureProp* mWeedsGenProps
#if defined(PIKI_PC_PORT)
 {}
#endif
 ; // _3D0
};

/*
 * @brief TODO
 */
struct Weed : public ItemCreature {
	Weed();

	virtual void startAI(int);                          // _34
	virtual bool isVisible();                           // _74
	virtual bool isAtari();                             // _84
	virtual bool isAlive();                             // _88
	virtual bool needFlick(Creature*) { return false; } // _94
	virtual void update();                              // _E0
	virtual void refresh(Graphics&);                    // _EC

	bool interactPullout(Creature*);

	// _00      = VTBL
	// _00-_3C8 = ItemCreature
	u16 mIsPulled
#if defined(PIKI_PC_PORT)
 {}
#endif
 ;     // _3C8
	u16 mPulloutTimer
#if defined(PIKI_PC_PORT)
 {}
#endif
 ; // _3CA
	WeedsGen* mGen
#if defined(PIKI_PC_PORT)
 {}
#endif
 ;    // _3CC
};

#endif
