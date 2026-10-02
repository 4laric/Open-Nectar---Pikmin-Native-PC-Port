#ifndef _NAVI_H
#define _NAVI_H

#include "Creature.h"
#include "Node.h"
#include "PaniAnimator.h"
#include "PaniPikiAnimator.h"
#include "PelletView.h"
#include "Piki.h"
#include "ShadowCaster.h"
#include "types.h"
#if defined(PIKI_PC_PORT)
#include "Dolphin/gx.h"
#endif

class CPlate;
struct BurnEffect;
struct RippleEffect;
struct PermanentEffect;
// PC: la estela del cursor usa el resplandor de la antena, ampliado para
// cubrir el anillo del cursor.
constexpr f32 kCursorTrailScale = 1.5f;
struct SlimeEffect;
struct Kontroller;
struct NaviDrawer;
struct NaviStateMachine;
class GoalItem;
class NaviState;
class Piki;
struct PikiHeadItem;

// Redundant parenthesis surrounding the call to `Parm::Operator()` fixes matching for the DLL
#define NAVI_PARM(parm)         C_NAVI_PARM(this, parm)
#define C_NAVI_PARM(navi, parm) (static_cast<NaviProp*>((navi)->mProps)->mNaviProps.parm())

#if defined(PIKI_PC_PORT)
extern "C" int pc_settings_get_whistle_radius_pct(void);
// Mod "Whistle Radius": el radio máximo escalado; el mínimo no cambia.
#define NAVI_WHISTLE_MAX_RADIUS(navi) \
	(C_NAVI_PARM(navi, mWhistleMaxRadius) * (f32)pc_settings_get_whistle_radius_pct() / 100.0f)
#else
#define NAVI_WHISTLE_MAX_RADIUS(navi) C_NAVI_PARM(navi, mWhistleMaxRadius)
#endif

/**
 * @brief TODO
 */
#if defined(PIKI_PC_PORT)
#define PC_NAVI_RUNTIME_DEFAULT(value) = value
#else
#define PC_NAVI_RUNTIME_DEFAULT(value)
#endif

class Navi : public Creature, public PaniAnimKeyListener, public PelletView {
#if defined(PIKI_PC_PORT)
private:
	friend struct PcMiddayActorShellAccess;
	struct MiddayRestoreTag {};
	// Inert root only: no updates, pool publication or complete-family bind until
	// the staged owner has allocated and validated every required subobject.
	Navi(MiddayRestoreTag, CreatureProp*, int);
#endif
public:
	struct Locus {
		Locus() { mCanBeThrown = TRUE; }; // Only the DLL has it, so it was probably inline.

		void update();

		Vector3f mPosition;      // _00
		Vector3f mVelocity;      // _0C
		PermanentEffect mEffect; // _18
		int mCanBeThrown;        // _28
		MapMgr* mMapMgr;         // _2C
	};

	Navi(CreatureProp*, int);

	virtual void viewKill();                                   // _154
	virtual void viewDraw(Graphics&, immut Matrix4f&);         // _158
	virtual f32 viewGetBottomRadius();                         // _15C
	virtual f32 viewGetHeight();                               // _160
	virtual void viewStartTrembleMotion(f32);                  // _164
	virtual f32 getiMass();                                    // _38
	virtual f32 getSize();                                     // _3C
	virtual bool isVisible();                                  // _74
	virtual bool isBuried();                                   // _80
	virtual bool isAtari();                                    // _84
	virtual bool ignoreAtari(Creature*);                       // _98
	virtual bool stimulate(immut Interaction&);                // _A0
	virtual void sendMsg(Msg*);                                // _A4
	virtual void collisionCallback(immut CollEvent&);          // _A8
	virtual void bounceCallback();                             // _AC
	virtual void jumpCallback();                               // _B0
	virtual void wallCallback(immut Plane&, DynCollObject*);   // _B4
	virtual void offwallCallback(DynCollObject*);              // _B8
	virtual void dump();                                       // _C8
	virtual bool isRopable();                                  // _D4
	virtual void update();                                     // _E0
	virtual void postUpdate(int unused, f32 deltaTime);        // _E4
	virtual void refresh(Graphics&);                           // _EC
	virtual void refresh2d(Graphics&);                         // _F0
	virtual void demoDraw(Graphics&, immut Matrix4f*);         // _FC
	virtual void doAI();                                       // _104
	virtual void doKill();                                     // _10C
	virtual void animationKeyUpdated(immut PaniAnimKeyEvent&); // _168
	virtual bool mayIstick() { return false; }                 // _D8 (weak)
	virtual f32 getShadowSize() { return 20.0f; }              // _70 (weak)

	bool demoCheck();
	bool isNuking();
	void startMovieInf();
	void incPlatePiki();
	void decPlatePiki();
	int getPlatePikis();
	void startDayEnd();
	void updateDayEnd(immut Vector3f&);
	void enterAllPikis();
	void startDamageEffect();
	void finishDamage();
	// Lane 12 (#130): pause the core on a downed captain only when no living
	// partner remains (source mDeadNavis != 2). Single-captain play always
	// pauses.
	void pauseForDownIfLast();
	void startKontroller();
	void rideUfo();
	void reset();
	void findNextThrowPiki();
	void startMotion(immut PaniMotionInfo&, immut PaniMotionInfo&);
	void enableMotionBlend();
	void updateWalkAnimation();
	void callPikis(f32, bool recallWorkers = true);
	void callDebugs(f32);
	void releasePikis();
	bool procActionButton();
	void letPikiWork();
	void reviseController(Vector3f&);
	void makeVelocity(bool);
	void makeCStick(bool);
	void draw(Graphics&);
	void renderCircle(Graphics&);
	bool orimaDamaged();
	void throwPiki(Piki*, immut Vector3f&);
	void swapMotion(immut PaniMotionInfo&, immut PaniMotionInfo&);
	void finishLook();
	void updateLook();

	// unused/inlined:
	void startMovie(bool);
	bool movieMode();
	bool startDamage();
	bool doMotionBlend();
	void doAttack();
	bool insideOnyon();
	void procDamage(f32);
	void throwLocus(immut Vector3f& pos);
	void renderParabola(Graphics&, f32, f32);

	AState<Navi>* getCurrState() { return mCurrState; }
	void setCurrState(AState<Navi>* state) { mCurrState = state; }

	// Lane 12 second-captain primitives (#130). Additive accessors only; the
	// engine already stores the spawn index in `mNaviID` (_92C) and NaviMgr
	// already keeps `mNaviShapeObject[2]`. `getOtherNaviIndex` is the
	// GET_OTHER_NAVI equivalent (source: `1 - mNaviIndex`).
	int getNaviIndex() const { return mNaviID; }
	int getOtherNaviIndex() const { return mNaviID == 0 ? 1 : (mNaviID == 1 ? 0 : -1); }

	void setPellet(bool isPellet) { mIsPellet = isPellet; }

	bool isPellet() { return mIsPellet; }

	void forceFinishLook()
	{
		mLookAtPosPtr     = nullptr;
		mHeadYawOffsetRel = mHeadPitchOffset = 0.0f;
		mLookTimer                           = 0;
	}

	void startLook(immut Vector3f* pos)
	{
		mLookAtPosPtr = pos;
		mLookTimer    = 0;
	}

protected: // Nothing else, just this.
	void updateHeadMatrix();

public:
	// _00       = VTBL
	// _000-_2B8 = Creature
	// _2B8-_2BC = PaniAnimKeyListener
	// _2BC-_2C4 = PelletView
	OdoMeter mOdoMeter;                   // _2C4
	zen::particleGenerator* mDamageEfxA;  // _2D4
	zen::particleGenerator* mDamageEfxB;  // _2D8
	zen::particleGenerator* mDamageEfxC;  // _2DC
	bool mIsRidingUfo PC_NAVI_RUNTIME_DEFAULT(false);                    // _2E0
	bool mIsPellet PC_NAVI_RUNTIME_DEFAULT(false);                       // _2E1, is lying down/carryable
	Kontroller* mKontroller PC_NAVI_RUNTIME_DEFAULT(nullptr);              // _2E4
	Camera* mNaviCamera PC_NAVI_RUNTIME_DEFAULT(nullptr);                  // _2E8, could be CullFrustum*, but probably Camera*
#if defined(PIKI_PC_PORT)
	/// Cámara con la que se interpretan stick/ratón. En coop con cámara
	/// dinámica es la vista realmente mostrada (lerp unificada->propia), no
	/// mNaviCamera; nullptr = mNaviCamera.
	Camera* mControlCamera = nullptr;
	Camera* controlCamera() { return mControlCamera ? mControlCamera : mNaviCamera; }
	/// Coop: color de la luz de la antena según capitán/tinte. Llamar tras
	/// cada changeEffect.
	void applyPlayerLightTint();
	/// Capitán de este Olimar (PcCaptain): Olimar, Louie o un Pikmin.
	int pcCaptain();
	/// Capitán Pikmin: dibuja el Pikmin del color elegido (con hoja) usando
	/// la animación de Olimar. false si el capitán no es un Pikmin.
	bool pcDrawAsPikmin(Graphics& gfx);
	PaniPikiAnimMgr mPcPikiAnimMgr;
	int mPcPikiAnimColor = -1;
	Vector3f mPcPikiLeafTip; ///< punta de la hoja: ahí brilla la luz del capitán
	/// Tinte de distinción (solo J2 cuando ambos llevan el mismo capitán).
	bool pcHasTint();
	GXColor pcTint();
#endif
	immut Vector3f* mLookAtPosPtr;        // _2EC
	u8 mLookTimer PC_NAVI_RUNTIME_DEFAULT(0);                        // _2F0
	f32 mHeadYawOffsetRel PC_NAVI_RUNTIME_DEFAULT(0);                // _2F4
	f32 mHeadPitchOffset PC_NAVI_RUNTIME_DEFAULT(0);                 // _2F8
	Creature* mCollidedWorkObj PC_NAVI_RUNTIME_DEFAULT(nullptr);           // _2FC
	f32 mCollidedWorkObjTimer PC_NAVI_RUNTIME_DEFAULT(0);            // _300
	Pellet* mSelectedShipPart PC_NAVI_RUNTIME_DEFAULT(nullptr);            // _304
	bool mIsInWater PC_NAVI_RUNTIME_DEFAULT(false);                      // _308
	int mPluckCursorVisibilityTimer PC_NAVI_RUNTIME_DEFAULT(0);      // _30C, when a Pikmin is plucked, this timer counts up to make the cursor visible again
	BOOL mIsCursorVisible PC_NAVI_RUNTIME_DEFAULT(0);                // _310
	BurnEffect* mBurnEffect PC_NAVI_RUNTIME_DEFAULT(nullptr);              // _314
	RippleEffect* mRippleEffect PC_NAVI_RUNTIME_DEFAULT(nullptr);          // _318
	SlimeEffect* mSlimeEffect PC_NAVI_RUNTIME_DEFAULT(nullptr);            // _31C
	NaviStateMachine* mStateMachine;      // _320
	ShadowCaster mShadowCaster;           // _324, cast mDrawer to NaviDrawer*
	f32 mMotionSpeed PC_NAVI_RUNTIME_DEFAULT(0);                     // _6BC
	int mIsDayEnd PC_NAVI_RUNTIME_DEFAULT(0);                        // _6C0
	ShapeDynMaterials mAnimatedMaterials; // _6C4
	Vector3f mCursorPosition;             // _6D4, where cursor (whistle) currently is
	f32 mCursorNaviDist PC_NAVI_RUNTIME_DEFAULT(0);                  // _6E0, how far is the cursor from us?
	Vector3f mCursorTargetPosition;       // _6E4, where we want cursor to be
	Vector3f mCursorWorldPos;             // _6F0, also cursor related?
#if defined(PIKI_PC_PORT)
	void pcUpdateLockOn();
	void pcPinCursorToLock();
	void pcPinCursorFirstPerson();
	Creature* mPcLockTarget = nullptr; ///< Mod "Lock-On": enemigo fijado.
#endif
	int mPendingLowerMotionId PC_NAVI_RUNTIME_DEFAULT(0);            // _6FC
	int mLowerMotionCooldown PC_NAVI_RUNTIME_DEFAULT(0);             // _700
	f32 mFlickIntensity PC_NAVI_RUNTIME_DEFAULT(0);                  // _704
	GoalItem* mGoalItem PC_NAVI_RUNTIME_DEFAULT(nullptr);                  // _708
	bool mWithinContainer;                // _70C, not used anywhere, its a weird variable
	CPlate* mPlateMgr;                    // _710, manages pikis in navi's party
	f32 mPlateYaw PC_NAVI_RUNTIME_DEFAULT(0);                        // _714
	bool mPlateDirLocked PC_NAVI_RUNTIME_DEFAULT(false);                 // _718
	bool mRearrangePending PC_NAVI_RUNTIME_DEFAULT(false);               // _719
	int mFormationBand PC_NAVI_RUNTIME_DEFAULT(0);                   // _71C
	int mFormationBandStableTimer PC_NAVI_RUNTIME_DEFAULT(0);        // _720
	bool mIsCStickNeutral PC_NAVI_RUNTIME_DEFAULT(false);                // _724
	u8 _725[0x72C - 0x725];               // _725, TODO: work out members
	u32 mSeedCollectionCount PC_NAVI_RUNTIME_DEFAULT(0);             // _72C, seeded from flow controller and incremented when seeds are picked up
	u32 _730;                             // _730, functionally unknown
	int mCurrKeyCount PC_NAVI_RUNTIME_DEFAULT(0);                    // _734
	f32 mNeutralTime PC_NAVI_RUNTIME_DEFAULT(0);                     // _738, sleep button held timer?
	u8 _73C[0x4];                         // _73C, TODO: work out members
	Vector3f mPrevMainStick;              // _740
	Vector3f mMainStick;                  // _74C
	Vector3f mPrevCStick;                 // _758
	Vector3f mCStick;                     // _764
	u32 _770;                             // _770, unused
	PermanentEffect* mNaviLightEfx PC_NAVI_RUNTIME_DEFAULT(nullptr);       // _774
	PermanentEffect* mNaviLightGlowEfx PC_NAVI_RUNTIME_DEFAULT(nullptr);   // _778
	PermanentEffect* mCursorTrailEfx PC_NAVI_RUNTIME_DEFAULT(nullptr);     // _77C, unused in retail; PC: estela del cursor (nav_blur)
	PermanentEffect* _780;                // _780, unused
	Vector3f mCursorTrailLastPos;         // PC: última posición con la que emitió la estela
	Vector3f mNaviLightPosition;          // _784
	Vector3f mDayEndPosition;             // _790
	Vector3f mWalkAnimPrevPos;            // _79C
	f32 mAiTickTimer PC_NAVI_RUNTIME_DEFAULT(0);                     // _7A8
	immut Plane* mWallPlane;              // _7AC
	DynCollObject* mWallCollObj PC_NAVI_RUNTIME_DEFAULT(nullptr);          // _7B0
	int mAiHitWall PC_NAVI_RUNTIME_DEFAULT(0);                       // _7B4
	int _7B8;                             // _7B8, unused
	Piki* mPikiToPluck PC_NAVI_RUNTIME_DEFAULT(nullptr);                   // _7BC
	PikiHeadItem* mSproutToPluck PC_NAVI_RUNTIME_DEFAULT(nullptr);         // _7C0, only for delayed piki plucks (set true by default)
	Vector3f _7C4;                        // _7C4, unused
	f32 _7D0;                             // _7D0, unused
	u8 _7D4[0x7D8 - 0x7D4];               // _7D4, TODO: work out members
	SmartPtr<Creature> mAttackTarget;     // _7D8, target consumed by doAttack
	f32 mWalkAnimPrevDir PC_NAVI_RUNTIME_DEFAULT(0);                 // _7DC
	int mPreBlendLowerMotionID PC_NAVI_RUNTIME_DEFAULT(0);           // _7E0
	bool mIsPlucking PC_NAVI_RUNTIME_DEFAULT(false);                     // _7E4
	u8 mFastPluckKeyTaps PC_NAVI_RUNTIME_DEFAULT(0);                 // _7E5, number of times A has been pressed to continue (fast) plucking
	u8 mNoPluckTimer PC_NAVI_RUNTIME_DEFAULT(0);                     // _7E6, count after plucking stops to zoom out camera/stop fast pluck
	u8 _7E7[0x7F0 - 0x7E7];               // _7E7, TODO: work out members
	int mLociCount;                       // _7F0
	Locus* mLoci;                         // _7F4
	Piki* mNextThrowPiki PC_NAVI_RUNTIME_DEFAULT(nullptr);                 // _7F8
	bool _7FC;                            // _7FC, unused
	f32 mThrowHoldTime PC_NAVI_RUNTIME_DEFAULT(0);                   // _800
	f32 mThrowDistance PC_NAVI_RUNTIME_DEFAULT(0);                   // _804
	f32 mThrowHeight PC_NAVI_RUNTIME_DEFAULT(0);                     // _808
	int mFormationPriMode PC_NAVI_RUNTIME_DEFAULT(0);                // _80C, only ever 0
	u32 _810;                             // _810, unused
	f32 mPressedTimer PC_NAVI_RUNTIME_DEFAULT(0);                    // _814
	f32 _818;                             // _818, unused
	Vector3f _81C;                        // _81C, unused
	u32 _828;                             // _828, unused
	PikiShapeObject* mNaviShapeObject PC_NAVI_RUNTIME_DEFAULT(nullptr);    // _82C
	bool mForcePikiDistCheck PC_NAVI_RUNTIME_DEFAULT(false);             // _830
	PaniPikiAnimMgr mNaviAnimMgr;         // _834
	SearchData mNaviSearchData[6];        // _8E0
	u32 _928;                             // _928, unused
	int mNaviID PC_NAVI_RUNTIME_DEFAULT(0);                          // _92C
	bool _930;                            // _930, unused
	int _934;                             // _934, unused

	///////// Whistle /////////
	Vector3f mWhistleFxPosArr[32]; // _938
	f32 mWhistleTimer PC_NAVI_RUNTIME_DEFAULT(0);             // _AB8
	int mWhistleCircleMode PC_NAVI_RUNTIME_DEFAULT(0);        // _ABC
	f32 mWhistleRadiusFrac PC_NAVI_RUNTIME_DEFAULT(0);        // _AC0
	f32 _AC4 PC_NAVI_RUNTIME_DEFAULT(0);                      // _AC4
	f32 mWhistleCircleRadius PC_NAVI_RUNTIME_DEFAULT(0);      // _AC8
	bool _ACC;                     // _ACC, unused
	CollTriInfo* _AD0;             // _AD0, functionally unused
	u8 _AD4[0x4];                  // _AD4, unknown
	f32 _AD8 PC_NAVI_RUNTIME_DEFAULT(0);                      // _AD8, cliff distance?
	AState<Navi>* mCurrState;      // _ADC
};

/**
 * @brief TODO
 */
struct NaviDrawer : public Node {
	NaviDrawer(Navi* navi)
	    : Node("")
	{
		mNavi = navi;
	}

	virtual void draw(Graphics& gfx) { mNavi->draw(gfx); } // _14 (weak)

	// _00     = VTBL
	// _00-_20 = Node
	Navi* mNavi; // _20
};

extern bool DelayPikiBirth;

#if defined(PIKI_PC_PORT)
/// Colour chosen with the mouse wheel, or -1 when no preference is active.
int pc_preferred_throw_color(); // Selection class, including bomb yellows.
int pc_throw_selection_class(Piki*);
/// Handle one D-pad color-selection edge; returns a different-color candidate.
Piki* pc_cycle_throw_color(Navi*, Piki* current);
/// Color preferido de la rueda para un Olimar concreto: solo cuenta para el
/// jugador que tiene teclado y ratón; el otro no tiene rueda (-1).
int pc_preferred_throw_color_for(Navi* navi);
/// Avanza el color preferido con rueda/táctil/cruceta; true si la cruceta lo
/// cambió en este tick (para cambiar el Pikmin ya sujeto, issue #43).
bool pc_navi_step_throw_color(Navi* navi);
bool pcIsLastNaviStanding(Navi* navi);
#endif

#endif

#undef PC_NAVI_RUNTIME_DEFAULT
