#ifndef _NAVISTATE_H
#define _NAVISTATE_H

#include "Navi.h"
#include "StateMachine.h"
#include "Vector.h"
#include "Win.h"
#include "types.h"
#if defined(PIKI_PC_PORT)
#include "pc_whistle.h"
#endif

// Checkpoint capture can observe an inner phase before its scalar is first
// assigned. Define those otherwise indeterminate values only in the PC port.
#if defined(PIKI_PC_PORT)
#define PC_NAVI_CHECKPOINT_DEFAULT(value) = value
#else
#define PC_NAVI_CHECKPOINT_DEFAULT(value)
#endif
class NaviState;

/**
 * @brief TODO
 */
enum NaviStateID {
	NAVISTATE_NULL        = -1,
	NAVISTATE_Walk        = 0,
	NAVISTATE_Throw       = 1,
	NAVISTATE_ThrowWait   = 2,
	NAVISTATE_Gather      = 3,
	NAVISTATE_Release     = 4,
	NAVISTATE_Nuku        = 5,
	NAVISTATE_NukuAdjust  = 6,
	NAVISTATE_Pressed     = 7,
	NAVISTATE_Flick       = 8,
	NAVISTATE_Funbari     = 9,
	NAVISTATE_Rope        = 10,
	NAVISTATE_RopeExit    = 11,
	NAVISTATE_Container   = 12,
	NAVISTATE_Ufo         = 13,
	NAVISTATE_UfoAccess   = 14,
	NAVISTATE_PartsAccess = 15,
	NAVISTATE_Pick        = 16,
	NAVISTATE_Idle        = 17,
	NAVISTATE_Stuck       = 18,
	NAVISTATE_Bury        = 19,
	NAVISTATE_Geyzer      = 20, // dev spelling
	NAVISTATE_DemoWait    = 21,
	NAVISTATE_DemoInf     = 22,
	NAVISTATE_Starting    = 23,
	NAVISTATE_Pellet      = 24,
	NAVISTATE_DemoSunset  = 25,
	NAVISTATE_Sow         = 26,
	NAVISTATE_Water       = 27,
	NAVISTATE_Attack      = 28,
	NAVISTATE_Dead        = 29,
	NAVISTATE_Push        = 30,
	NAVISTATE_PushPiki    = 31,
	NAVISTATE_Lock        = 32,
	NAVISTATE_PikiZero    = 33,
	NAVISTATE_Clear       = 34,
	NAVISTATE_IroIro      = 35,
#if defined(PIKI_PC_PORT)
	NAVISTATE_DemonDrop = 36,
	NAVISTATE_DemonEscape = 37,
#endif
	NAVISTATE_Count, // PC 38; retail 36
};

/**
 * @brief TODO
 */
class NaviState : public AState<Navi> {
public:
	inline NaviState(int stateID)
	    : AState<Navi>(stateID)
	{
	}

	virtual bool invincible(Navi*) { return false; } // _50 (weak)

	// _00     = VTBL
	// _00-_10 = AState
};

/**
 * @brief TODO
 */
struct NaviStateMachine : public StateMachine<Navi> {
	virtual void init(Navi*); // _08
#if defined(PIKI_PC_PORT)
	void transit(Navi*, int) override;
#endif

	NaviState* getNaviState(Navi*);

	// _00     = VTBL
	// _00-_1C = StateMachine
	// TODO: members
};

/**
 * @brief TODO
 *
 * @note Size: 0x1C.
 */
struct NaviAttackState : public NaviState {
	NaviAttackState();

	virtual void procAnimMsg(Navi*, MsgAnim*); // _20
	virtual void init(Navi*);                  // _38
	virtual void exec(Navi*);                  // _3C
	virtual void cleanup(Navi*);               // _40
	virtual void resume(Navi*);                // _44
	virtual void restart(Navi*);               // _48

	// _00     = VTBL
	// _00-_10 = NaviState
	u16 mAttackPhase PC_NAVI_CHECKPOINT_DEFAULT(0);      // _10
	bool mGatherRequested PC_NAVI_CHECKPOINT_DEFAULT(false); // _12
	f32 _14 PC_NAVI_CHECKPOINT_DEFAULT(0);  // _14
	f32 _18 PC_NAVI_CHECKPOINT_DEFAULT(0);  // _18
};

/**
 * @brief TODO
 *
 * @note Size: 0x20.
 */
struct NaviBuryState : public NaviState {
	NaviBuryState();

	virtual void procAnimMsg(Navi*, MsgAnim*);      // _20
	virtual void init(Navi*);                       // _38
	virtual void exec(Navi*);                       // _3C
	virtual void cleanup(Navi*);                    // _40
	virtual bool invincible(Navi*) { return true; } // _50

	// _00     = VTBL
	// _00-_10 = NaviState
	Vector3f mPreviousStickInput; // _10
	u8 mBuryState PC_NAVI_CHECKPOINT_DEFAULT(0);                // _1C
	u8 mEscapeAttemptCounter PC_NAVI_CHECKPOINT_DEFAULT(0);     // _1D
	u8 mValidEscapeAttempts PC_NAVI_CHECKPOINT_DEFAULT(0);      // _1E
	u8 mEscapeTimer PC_NAVI_CHECKPOINT_DEFAULT(0);              // _1F
};

/**
 * @brief TODO
 *
 * @note Size: 0x10.
 */
struct NaviClearState : public NaviState {
	NaviClearState();

	virtual void procAnimMsg(Navi*, MsgAnim*); // _20
	virtual void init(Navi*);                  // _38
	virtual void exec(Navi*);                  // _3C
	virtual void cleanup(Navi*);               // _40

	// _00     = VTBL
	// _00-_10 = NaviState
};

/**
 * @brief TODO
 *
 * @note Size: 0x30.
 */
struct NaviContainerState : public NaviState, virtual public ContainerWin::Listener, virtual public GmWin::CloseListener {
	NaviContainerState();

	virtual void init(Navi*);                       // _38
	virtual void exec(Navi*);                       // _3C
	virtual void cleanup(Navi*);                    // _40
	virtual bool invincible(Navi*) { return true; } // _50
	virtual void informWin(int signedPikiCount);    // _54
	virtual void onCloseWindow();                   // _58

	void enterPikis(Navi*, int);
	void exitPikis(Navi*, int);

	// _00     = VTBL
	// _00-_10 = NaviState
	// _10     = ContainerWin::Listener ptr
	// _14     = GmWin::CloseListener ptr
	int mContainerWinEvent PC_NAVI_CHECKPOINT_DEFAULT(0); // _18
	int mContainerWinCount PC_NAVI_CHECKPOINT_DEFAULT(0); // _1C
	                        // ContainerWin::Listener
	                        // GmWin::CloseListener
};

/**
 * @brief TODO
 *
 * @note Size: 0x10.
 */
struct NaviDeadState : public NaviState {
	NaviDeadState();

	virtual void procAnimMsg(Navi*, MsgAnim*);      // _20
	virtual void init(Navi*);                       // _38
	virtual void exec(Navi*);                       // _3C
	virtual void cleanup(Navi*);                    // _40
	virtual void restart(Navi*);                    // _48
	virtual bool invincible(Navi*) { return true; } // _50

	// _00     = VTBL
	// _00-_10 = NaviState
#if defined(PIKI_PC_PORT)
	// Cooperativo: "caído" = ha muerto pero el otro Olimar sigue vivo. El
	// cuerpo se queda, el mundo no se pausa y el día no termina.
	bool mDowned = false;
#endif
};

/**
 * @brief TODO
 *
 * @note Size: 0x10.
 */
struct NaviDemoInfState : public NaviState {
	NaviDemoInfState();

	virtual void init(Navi*);                       // _38
	virtual void exec(Navi*);                       // _3C
	virtual void cleanup(Navi*);                    // _40
	virtual bool invincible(Navi*) { return true; } // _50

	// _00     = VTBL
	// _00-_10 = NaviState
};

/**
 * @brief TODO
 *
 * @note Size: 0x40.
 */
class NaviDemoSunsetState : public NaviState {
#if defined(PIKI_PC_PORT)
private:
	friend struct PcMiddayStateFactoryAccess;
	struct MiddayRestoreTag {};
	NaviDemoSunsetState(MiddayRestoreTag);
#endif
public:
	NaviDemoSunsetState();

	/**
	 * @brief TODO
	 */
	enum DemoStateID {
		DEMOSTATE_Go      = 0,
		DEMOSTATE_Look    = 1,
		DEMOSTATE_Whistle = 2,
		DEMOSTATE_Wait    = 3,
		DEMOSTATE_Sit     = 4,
		DEMOSTATE_Count, // 5
	};

	/**
	 * @brief TODO
	 */
	struct DemoStateMachine : public StateMachine<NaviDemoSunsetState> {
		virtual void init(NaviDemoSunsetState*); // _08

		// _00     = VTBL
		// _00-_1C = StateMachine
		// TODO: members
	};

	/**
	 * @brief TODO
	 */
	struct DemoState : public AState<NaviDemoSunsetState> {
		inline DemoState(int stateID)
		    : AState<NaviDemoSunsetState>(stateID)
		{
		}

		// _00     = VTBL
		// _00-_10 = AState
	};

	/**
	 * @brief TODO
	 */
	struct GoState : public DemoState {
		inline GoState()
		    : DemoState(DEMOSTATE_Go)
		{
		}

		virtual void procAnimMsg(NaviDemoSunsetState*, MsgAnim*); // _20
		virtual void init(NaviDemoSunsetState*);                  // _38
		virtual void exec(NaviDemoSunsetState*);                  // _3C
		virtual void cleanup(NaviDemoSunsetState*);               // _40

		// _00     = VTBL
		// _00-_0C = AState
		int mStumbleLoopCount PC_NAVI_CHECKPOINT_DEFAULT(0); // _10
		bool mIsStumbling PC_NAVI_CHECKPOINT_DEFAULT(false);     // _14
	};

	/**
	 * @brief TODO
	 */
	struct LookState : public DemoState {
		inline LookState()
		    : DemoState(DEMOSTATE_Look)
		{
		}

		virtual void procAnimMsg(NaviDemoSunsetState*, MsgAnim*); // _20
		virtual void init(NaviDemoSunsetState*);                  // _38
		virtual void exec(NaviDemoSunsetState*);                  // _3C
		virtual void cleanup(NaviDemoSunsetState*);               // _40

		// _00     = VTBL
		// _00-_0C = AState
		// TODO: members
	};

	/**
	 * @brief TODO
	 */
	struct SitState : public DemoState {
		inline SitState()
		    : DemoState(DEMOSTATE_Sit)
		{
		}

		virtual void init(NaviDemoSunsetState*);    // _38
		virtual void exec(NaviDemoSunsetState*);    // _3C
		virtual void cleanup(NaviDemoSunsetState*); // _40

		// _00     = VTBL
		// _00-_0C = AState
		// TODO: members
	};

	/**
	 * @brief TODO
	 */
	struct WaitState : public DemoState {
		inline WaitState()
		    : DemoState(DEMOSTATE_Wait)
		{
		}

		virtual void init(NaviDemoSunsetState*);    // _38
		virtual void exec(NaviDemoSunsetState*);    // _3C
		virtual void cleanup(NaviDemoSunsetState*); // _40

		// _00     = VTBL
		// _00-_0C = AState
		// TODO: members
	};

	/**
	 * @brief TODO
	 */
	struct WhistleState : public DemoState {
		inline WhistleState()
		    : DemoState(DEMOSTATE_Whistle)
		{
		}

		virtual void procAnimMsg(NaviDemoSunsetState*, MsgAnim*); // _20
		virtual void init(NaviDemoSunsetState*);                  // _38
		virtual void exec(NaviDemoSunsetState*);                  // _3C
		virtual void cleanup(NaviDemoSunsetState*);               // _40

		void enterAllPikis(NaviDemoSunsetState*);

		// _00     = VTBL
		// _00-_0C = AState
		int mWhistleLoopCount PC_NAVI_CHECKPOINT_DEFAULT(0); // _10
	};

	virtual void procAnimMsg(Navi*, MsgAnim*); // _20
	virtual void init(Navi*);                  // _38
	virtual void exec(Navi*);                  // _3C
	virtual void cleanup(Navi*);               // _40

	void setActors(Navi*);

	AState<NaviDemoSunsetState>* getCurrState() { return mCurrentState; }

	void setCurrState(AState<NaviDemoSunsetState>* state) { mCurrentState = state; }

	// _00     = VTBL
	// _00-_10 = NaviState
	Navi* mNavi PC_NAVI_CHECKPOINT_DEFAULT(nullptr);                                // _10
	Vector3f mStartPos;                         // _14
	Vector3f mGoalPos;                          // _20
	f32 mGoalDistance PC_NAVI_CHECKPOINT_DEFAULT(0);                          // _2C
	f32 mSunsetTimer PC_NAVI_CHECKPOINT_DEFAULT(0);                           // _30
	bool mOpenedAccount PC_NAVI_CHECKPOINT_DEFAULT(false);                        // _34
	DemoStateMachine* mStateMachine;            // _38
	AState<NaviDemoSunsetState>* mCurrentState; // _3C, unknown
};

/**
 * @brief TODO
 *
 * @note Size: 0x1C.
 */
struct NaviDemoWaitState : public NaviState {
	NaviDemoWaitState();

	virtual void init(Navi*);                       // _38
	virtual void exec(Navi*);                       // _3C
	virtual void cleanup(Navi*);                    // _40
	virtual bool invincible(Navi*) { return true; } // _50

	// _00     = VTBL
	// _00-_10 = NaviState
	Vector3f mLookAtPos; // _10
};

/**
 * @brief TODO
 *
 * @note Size: 0x24.
 */
struct NaviFlickState : public NaviState {
	NaviFlickState();

	virtual void procAnimMsg(Navi*, MsgAnim*);      // _20
	virtual void init(Navi*);                       // _38
	virtual void exec(Navi*);                       // _3C
	virtual void cleanup(Navi*);                    // _40
	virtual bool invincible(Navi*) { return true; } // _50

	// _00     = VTBL
	// _00-_10 = NaviState
	u16 mFlickState PC_NAVI_CHECKPOINT_DEFAULT(0);          // _10
	f32 mGetupAnimationTimer PC_NAVI_CHECKPOINT_DEFAULT(0); // _14
	f32 mDirection PC_NAVI_CHECKPOINT_DEFAULT(0);           // _18
	f32 mRandVariation PC_NAVI_CHECKPOINT_DEFAULT(0);       // _1C
	f32 mIntensity PC_NAVI_CHECKPOINT_DEFAULT(0);           // _20
};

/**
 * @brief TODO
 *
 * @note Size: 0x10.
 */
struct NaviFunbariState : public NaviState {
	NaviFunbariState();

	virtual void procAnimMsg(Navi*, MsgAnim*); // _20
	virtual void init(Navi*);                  // _38
	virtual void exec(Navi*);                  // _3C
	virtual void cleanup(Navi*);               // _40

	// _00     = VTBL
	// _00-_10 = NaviState
};

/**
 * @brief TODO
 *
 * @note Size: 0x1C.
 */
struct NaviGatherState : public NaviState {
#if defined(PIKI_PC_PORT)
	PcWhistleTapState mTapState;
	float mNextWhistlePluckTime = 0.0f; // "Whistle Pluck" mod: next pluck, on mWhistleTimer
#endif
	NaviGatherState();

	virtual void procAnimMsg(Navi*, MsgAnim*); // _20
	virtual void init(Navi*);                  // _38
	virtual void exec(Navi*);                  // _3C
	virtual void cleanup(Navi*);               // _40
	virtual void resume(Navi*);                // _44
	virtual void restart(Navi*);               // _48

	// _00     = VTBL
	// _00-_10 = NaviState
	u16 mWhistleAnimPhase PC_NAVI_CHECKPOINT_DEFAULT(0);       // _10
	f32 mWhistleCallRadius PC_NAVI_CHECKPOINT_DEFAULT(0);      // _14
	bool mWhistleEffectsStopped PC_NAVI_CHECKPOINT_DEFAULT(false); // _18
};

/**
 * @brief TODO
 *
 * @note Size: 0x34.
 */
struct NaviGeyzerState : public NaviState {
	NaviGeyzerState();

	virtual void procBounceMsg(Navi*, MsgBounce*);  // _0C
	virtual void procAnimMsg(Navi*, MsgAnim*);      // _20
	virtual void init(Navi*);                       // _38
	virtual void exec(Navi*);                       // _3C
	virtual void cleanup(Navi*);                    // _40
	virtual bool invincible(Navi*) { return true; } // _50

	// _00     = VTBL
	// _00-_10 = NaviState
	u16 mGeyserState PC_NAVI_CHECKPOINT_DEFAULT(0);     // _10
	f32 mGetupDelayTimer PC_NAVI_CHECKPOINT_DEFAULT(0); // _14
	f32 mPlayerDirection PC_NAVI_CHECKPOINT_DEFAULT(0); // _18
	f32 mSpinDelta PC_NAVI_CHECKPOINT_DEFAULT(0);       // _1C
	Vector3f mLaunchTargetPos;      // _20
	f32 mRiseTargetHeight PC_NAVI_CHECKPOINT_DEFAULT(0);          // _2C
	bool mHasAppliedLaunchVelocity PC_NAVI_CHECKPOINT_DEFAULT(false); // _30
};

/**
 * @brief TODO
 *
 * @note Size: 0x18.
 */
struct NaviIdleState : public NaviState {
	NaviIdleState();

	virtual void procAnimMsg(Navi*, MsgAnim*); // _20
	virtual void init(Navi*);                  // _38
	virtual void exec(Navi*);                  // _3C
	virtual void cleanup(Navi*);               // _40

	// _00     = VTBL
	// _00-_10 = NaviState
	u8 _10[0x4];         // _10, unknown
	bool mStopBeingIdle PC_NAVI_CHECKPOINT_DEFAULT(false); // _14
};

/**
 * @brief TODO
 *
 * @note Size: 0x10.
 */
struct NaviIroIroState : public NaviState {
	NaviIroIroState();

	virtual void init(Navi*);    // _38
	virtual void exec(Navi*);    // _3C
	virtual void cleanup(Navi*); // _40

	// _00     = VTBL
	// _00-_10 = NaviState
};

/**
 * @brief TODO
 *
 * @note Size: 0x10.
 */
struct NaviLockState : public NaviState {
	NaviLockState();

	virtual void init(Navi*);    // _38
	virtual void exec(Navi*);    // _3C
	virtual void cleanup(Navi*); // _40

	// _00     = VTBL
	// _00-_10 = NaviState
};

/**
 * @brief TODO
 *
 * @note Size: 0x30.
 */
struct NaviNukuAdjustState : public NaviState {
	NaviNukuAdjustState();

	virtual void init(Navi*);    // _38
	virtual void exec(Navi*);    // _3C
	virtual void cleanup(Navi*); // _40
	virtual void resume(Navi*);  // _44
	virtual void restart(Navi*); // _48

	// _00     = VTBL
	// _00-_10 = NaviState
	f32 mTargetFaceDirection PC_NAVI_CHECKPOINT_DEFAULT(0);   // _10
	Vector3f mApproachPosition; // _14
	bool _20 PC_NAVI_CHECKPOINT_DEFAULT(false);     // _20
	Vector3f mLastPosition; // _24
};

/**
 * @brief TODO
 *
 * @note Size: 0x18.
 */
struct NaviNukuState : public NaviState {
	NaviNukuState();

	virtual void procAnimMsg(Navi*, MsgAnim*);      // _20
	virtual void init(Navi*);                       // _38
	virtual void exec(Navi*);                       // _3C
	virtual void cleanup(Navi*);                    // _40
	virtual bool invincible(Navi*) { return true; } // _50

	// _00     = VTBL
	// _00-_10 = NaviState
	u16 mPullCountRemaining PC_NAVI_CHECKPOINT_DEFAULT(0); // _10
	bool _12 PC_NAVI_CHECKPOINT_DEFAULT(false); // _12
	bool mExtractKeyReleased PC_NAVI_CHECKPOINT_DEFAULT(false); // _13
	bool mWantsNextPluck PC_NAVI_CHECKPOINT_DEFAULT(false);     // _14
	bool _15 PC_NAVI_CHECKPOINT_DEFAULT(false); // _15
};

/**
 * @brief TODO
 *
 * @note Size: 0x14.
 */
struct NaviPartsAccessState : public NaviState {
	NaviPartsAccessState();

	virtual void procAnimMsg(Navi*, MsgAnim*);      // _20
	virtual void init(Navi*);                       // _38
	virtual void exec(Navi*);                       // _3C
	virtual void cleanup(Navi*);                    // _40
	virtual bool invincible(Navi*) { return true; } // _50

	// _00     = VTBL
	// _00-_10 = NaviState
	bool mHasShownPartText PC_NAVI_CHECKPOINT_DEFAULT(false); // _10
};

/**
 * @brief TODO
 *
 * @note Size; 0x14.
 */
struct NaviPelletState : public NaviState {
	NaviPelletState();

	virtual void procAnimMsg(Navi*, MsgAnim*);      // _20
	virtual void init(Navi*);                       // _38
	virtual void exec(Navi*);                       // _3C
	virtual void cleanup(Navi*);                    // _40
	virtual bool invincible(Navi*) { return true; } // _50

	// _00     = VTBL
	// _00-_10 = NaviState
	bool mIsFinished PC_NAVI_CHECKPOINT_DEFAULT(false); // _10
};

/**
 * @brief TODO
 *
 * @note Size: 0x10.
 */
struct NaviPickState : public NaviState {
	NaviPickState();

	virtual void procAnimMsg(Navi*, MsgAnim*); // _20
	virtual void init(Navi*);                  // _38
	virtual void exec(Navi*);                  // _3C
	virtual void cleanup(Navi*);               // _40

	// _00     = VTBL
	// _00-_10 = NaviState
};

/**
 * @brief TODO
 *
 * @note Size: 0x18.
 */
struct NaviPikiZeroState : public NaviState {
	NaviPikiZeroState();

	virtual void procAnimMsg(Navi*, MsgAnim*);      // _20
	virtual void init(Navi*);                       // _38
	virtual void exec(Navi*);                       // _3C
	virtual void cleanup(Navi*);                    // _40
	virtual bool invincible(Navi*) { return true; } // _50

	// _00     = VTBL
	// _00-_10 = NaviState
	bool _10; // _10
	bool _11; // _11
	u16 mGameOverCountdown PC_NAVI_CHECKPOINT_DEFAULT(0); // _12
	u32 _14;  // _14
};

/**
 * @brief TODO
 *
 * @note Size: 0x10.
 */
struct NaviPressedState : public NaviState {
	NaviPressedState();

	virtual void init(Navi*);                       // _38
	virtual void exec(Navi*);                       // _3C
	virtual void cleanup(Navi*);                    // _40
	virtual bool invincible(Navi*) { return true; } // _50

	// _00     = VTBL
	// _00-_10 = NaviState
};

/**
 * @brief TODO
 *
 * @note Size: 0x14.
 */
struct NaviPushPikiState : public NaviState {
	NaviPushPikiState();

	virtual void procCollideMsg(Navi*, MsgCollide*); // _1C
	virtual void procAnimMsg(Navi*, MsgAnim*);       // _20
	virtual void init(Navi*);                        // _38
	virtual void exec(Navi*);                        // _3C
	virtual void cleanup(Navi*);                     // _40

	// _00     = VTBL
	// _00-_10 = NaviState
	int mHasPushContact PC_NAVI_CHECKPOINT_DEFAULT(0); // _10
};

/**
 * @brief TODO
 *
 * @note Size: 0x14.
 */
struct NaviPushState : public NaviState {
	NaviPushState();

	virtual void procAnimMsg(Navi*, MsgAnim*);       // _20
	virtual void procOffWallMsg(Navi*, MsgOffWall*); // _2C
	virtual void init(Navi*);                        // _38
	virtual void exec(Navi*);                        // _3C
	virtual void cleanup(Navi*);                     // _40

	// _00     = VTBL
	// _00-_10 = NaviState
	bool mIsFinishing PC_NAVI_CHECKPOINT_DEFAULT(false); // _10
};

/**
 * @brief TODO
 *
 * @note Size: 0x14.
 */
struct NaviReleaseState : public NaviState {
	NaviReleaseState();

	virtual void procAnimMsg(Navi*, MsgAnim*); // _20
	virtual void init(Navi*);                  // _38
	virtual void exec(Navi*);                  // _3C
	virtual void cleanup(Navi*);               // _40

	// _00     = VTBL
	// _00-_10 = NaviState
	bool mCanInterruptToGather PC_NAVI_CHECKPOINT_DEFAULT(false); // _10
};

/**
 * @brief TODO
 *
 * @note Size: 0x10.
 */
struct NaviRopeExitState : public NaviState {
	NaviRopeExitState();

	virtual void procBounceMsg(Navi*, MsgBounce*); // _0C
	virtual void init(Navi*);                      // _38
	virtual void exec(Navi*);                      // _3C
	virtual void cleanup(Navi*);                   // _40

	// _00     = VTBL
	// _00-_10 = NaviState
};

/**
 * @brief TODO
 *
 * @note Size: 0x10.
 */
struct NaviRopeState : public NaviState {
	NaviRopeState();

	virtual void init(Navi*);    // _38
	virtual void exec(Navi*);    // _3C
	virtual void cleanup(Navi*); // _40

	// _00     = VTBL
	// _00-_10 = NaviState
};

/**
 * @brief TODO
 *
 * @note Size: 0x10.
 */
struct NaviSowState : public NaviState {
	NaviSowState();

	virtual void init(Navi*);    // _38
	virtual void exec(Navi*);    // _3C
	virtual void cleanup(Navi*); // _40

	// _00     = VTBL
	// _00-_10 = NaviState
};

/**
 * @brief TODO
 *
 * @note Size: 0x40.
 */
struct NaviStartingState : public NaviState {
	enum EStartPhase {
		STARTPHASE_Delay         = 0,
		STARTPHASE_MoveToGoal    = 1,
		STARTPHASE_PlayStartAnim = 2,
	};

	NaviStartingState();

	virtual void procCollideMsg(Navi*, MsgCollide*); // _1C
	virtual void procAnimMsg(Navi*, MsgAnim*);       // _20
	virtual void init(Navi*);                        // _38
	virtual void exec(Navi*);                        // _3C
	virtual void cleanup(Navi*);                     // _40

	// _00     = VTBL
	// _00-_10 = NaviState
	f32 mStartDelayTimer PC_NAVI_CHECKPOINT_DEFAULT(0);      // _10
	Vector3f mWalkTargetPos;   // _14
	Vector3f mLookAtTargetPos; // _20
	u32 _2C;      // _2C
	u16 mStartPhase PC_NAVI_CHECKPOINT_DEFAULT(0);           // _30
	bool mIsStartAnimComplete PC_NAVI_CHECKPOINT_DEFAULT(false); // _32
	Vector3f mLastPosition;    // _34
};

/**
 * @brief State when navi has puffmin stuck to it.
 *
 * @note Size: 0x24.
 */
struct NaviStuckState : public NaviState {
	NaviStuckState();

	virtual void init(Navi*);    // _38
	virtual void exec(Navi*);    // _3C
	virtual void cleanup(Navi*); // _40

	// _00     = VTBL
	// _00-_10 = NaviState
	Vector3f mPrevStickDir; // _10, last recorded main joystick direction
	f32 mIdleTimer PC_NAVI_CHECKPOINT_DEFAULT(0);         // _1C, resets recorded action attempts when this hits 0
	int mActionCount PC_NAVI_CHECKPOINT_DEFAULT(0);       // _20
};

/**
 * @brief TODO
 *
 * @note Size: 0x18.
 */
struct NaviThrowState : public NaviState {
	NaviThrowState();

	virtual void procTargetMsg(Navi*, MsgTarget*); // _18
	virtual void procAnimMsg(Navi*, MsgAnim*);     // _20
	virtual void init(Navi*);                      // _38
	virtual void exec(Navi*);                      // _3C
	virtual void cleanup(Navi*);                   // _40

	// _00     = VTBL
	// _00-_10 = NaviState
	bool mHasThrownPiki PC_NAVI_CHECKPOINT_DEFAULT(false); // _10
	bool _11 PC_NAVI_CHECKPOINT_DEFAULT(false);  // _11
#if defined(PIKI_PC_PORT)
	// Throw press seen during the wind-up, before KEY_Action0 released the
	// Pikmin. Kept so a rapid mash starts the next grab as soon as the throw
	// lands instead of being dropped (issues #37 / #40).
	bool mQueuedThrowPress PC_NAVI_CHECKPOINT_DEFAULT(false);
#endif
	Piki* mTargetPiki PC_NAVI_CHECKPOINT_DEFAULT(nullptr); // _14
};

/**
 * @brief TODO
 *
 * @note Size: 0x2C.
 */
struct NaviThrowWaitState : public NaviState {
	NaviThrowWaitState();

	virtual void procAnimMsg(Navi*, MsgAnim*); // _20
	virtual void init(Navi*);                  // _38
	virtual void exec(Navi*);                  // _3C
	virtual void cleanup(Navi*);               // _40
	virtual void resume(Navi*);                // _44
	virtual void restart(Navi*);               // _48

	void sortPikis(Navi*);

	// unused/inlined:
	void lockHangPiki(Navi*);

	// _00     = VTBL
	// _00-_10 = NaviState
	Piki* mHeldThrowPiki PC_NAVI_CHECKPOINT_DEFAULT(nullptr);     // _10
	Piki* mPendingThrowPiki PC_NAVI_CHECKPOINT_DEFAULT(nullptr);  // _14
	int mThrowChargeLevel PC_NAVI_CHECKPOINT_DEFAULT(0);    // _18
	bool mIsHoldingThrowPiki PC_NAVI_CHECKPOINT_DEFAULT(false); // _1C
	u32 _20 PC_NAVI_CHECKPOINT_DEFAULT(0);   // _20
	f32 mPendingThrowPikiTimeout PC_NAVI_CHECKPOINT_DEFAULT(0); // _24
	f32 mSortDelayTimer PC_NAVI_CHECKPOINT_DEFAULT(0);          // _28
};

/**
 * @brief TODO
 *
 * @note Size: 0x14.
 */
struct NaviUfoAccessState : public NaviState {
	NaviUfoAccessState();

	virtual void procAnimMsg(Navi*, MsgAnim*);      // _20
	virtual void init(Navi*);                       // _38
	virtual void exec(Navi*);                       // _3C
	virtual void cleanup(Navi*);                    // _40
	virtual bool invincible(Navi*) { return true; } // _50

	// _00     = VTBL
	// _00-_10 = NaviState
	bool mHasShownUfoText PC_NAVI_CHECKPOINT_DEFAULT(false); // _10
};

/**
 * @brief TODO
 *
 * @note Size: 0x24.
 */
struct NaviUfoState : public NaviState {
	NaviUfoState();

	virtual void procCollideMsg(Navi*, MsgCollide*); // _1C
	virtual void procAnimMsg(Navi*, MsgAnim*);       // _20
	virtual void init(Navi*);                        // _38
	virtual void exec(Navi*);                        // _3C
	virtual void cleanup(Navi*);                     // _40
	virtual bool invincible(Navi*) { return true; }  // _50

	// _00     = VTBL
	// _00-_10 = NaviState
	u16 mState PC_NAVI_CHECKPOINT_DEFAULT(0);             // _10
	u16 mRecoveryTimer PC_NAVI_CHECKPOINT_DEFAULT(0);     // _12
	Vector3f mLastPosition; // _14
	s8 mPunchCooldownTimer PC_NAVI_CHECKPOINT_DEFAULT(0); // _20
	bool mHasReachedUfo PC_NAVI_CHECKPOINT_DEFAULT(false);    // _21
};

/**
 * @brief TODO
 *
 * @note Size: 0x20.
 */
struct NaviWalkState : public NaviState {
	NaviWalkState();

	virtual void procCollideMsg(Navi*, MsgCollide*); // _1C
	virtual void procWallMsg(Navi*, MsgWall*);       // _28
	virtual void procOffWallMsg(Navi*, MsgOffWall*); // _2C
	virtual void init(Navi*);                        // _38
	virtual void exec(Navi*);                        // _3C
	virtual void cleanup(Navi*);                     // _40
	virtual void restart(Navi*);                     // _48

	// _00     = VTBL
	// _00-_10 = NaviState
	Creature* _10 PC_NAVI_CHECKPOINT_DEFAULT(nullptr); // _10, unknown
	f32 _14 PC_NAVI_CHECKPOINT_DEFAULT(0);       // _14
	int mIsTouchingWall PC_NAVI_CHECKPOINT_DEFAULT(0); // _18
	f32 _1C PC_NAVI_CHECKPOINT_DEFAULT(0);       // _1C
};

/**
 * @brief TODO
 *
 * @note Size: 0x10.
 */
struct NaviWaterState : public NaviState {
	NaviWaterState();

	virtual void init(Navi*);    // _38
	virtual void exec(Navi*);    // _3C
	virtual void cleanup(Navi*); // _40

	// _00     = VTBL
	// _00-_10 = NaviState
};

#undef PC_NAVI_CHECKPOINT_DEFAULT
#endif
