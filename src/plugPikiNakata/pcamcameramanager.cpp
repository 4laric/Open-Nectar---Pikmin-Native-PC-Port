#include "DebugLog.h"
#include "NaviMgr.h"
#include "Pcam/Camera.h"
#include "Pcam/CameraManager.h"
#include "Pcam/MotionEvents.h"
#include "Peve/Condition.h"
#include "Peve/Event.h"
#include "sysNew.h"
#if defined(PIKI_PC_PORT)
#include "netplay/pc_netplay_det.h"
#include <cmath>
#include <cstdlib>
#endif

/**
 * @todo: Documentation
 * @note UNUSED Size: 00009C
 */
DEFINE_ERROR(__LINE__) // Never used in the DLL

/**
 * @todo: Documentation
 * @note UNUSED Size: 0000F4
 */
DEFINE_PRINT("pcamcameramanager")

/**
 * @todo: Documentation
 */
PcamCameraManager::PcamCameraManager(Camera* camera, Controller* controller)
    : Node("PcamCameraManager")
{
	mCamera          = new PcamCamera(camera);
	mController      = controller;
	mVibrationEvents = new PeveEvent*[PCAMVIB_VibrationCount];

	PcamVibrationEvent* vib1             = new PcamVibrationEvent(mCamera);
	vib1->mVibrationDuration             = 0.6f;
	vib1->mVibrationAmplitude            = 0.2f;
	vib1->mVibrationFrequency            = 8.0f;
	mVibrationEvents[PCAMVIB_Vibration1] = vib1;

	PcamVibrationEvent* vib2             = new PcamVibrationEvent(mCamera);
	vib2->mVibrationDuration             = 0.6f;
	vib2->mVibrationAmplitude            = 0.2f;
	vib2->mVibrationFrequency            = 4.0f;
	mVibrationEvents[PCAMVIB_Vibration2] = vib2;

	mVibrationEvents[PCAMVIB_LongVibration] = new PcamLongVibrationEvent(mCamera);

	// Opt-in Purple impact uses a separate short event; existing camera events
	// retain their original IDs and parameters. This is a P1 camera adaptation.
	PcamVibrationEvent* purpleImpact = new PcamVibrationEvent(mCamera);
	purpleImpact->mVibrationDuration = 0.12f;
	purpleImpact->mVibrationAmplitude = 0.08f;
	purpleImpact->mVibrationFrequency = 30.0f;
	mVibrationEvents[PCAMVIB_PurpleImpact] = purpleImpact;

	PcamDamageEvent* damage = new PcamDamageEvent(mCamera);
	// nice typo.
	vib2->mVibrationDuration  = 0.6f;
	vib2->mVibrationAmplitude = 0.2f;

	damage->mVibrationFrequency      = 30.0f;
	mVibrationEvents[PCAMVIB_Damage] = damage;

	PcamSideVibrationEvent* sideVib = new PcamSideVibrationEvent(mCamera);
	// nice typo.
	vib2->mVibrationDuration  = 0.6f;
	vib2->mVibrationAmplitude = 0.2f;

	sideVib->mMaxRotation                   = NMathF::pi / 48.0f;
	mVibrationEvents[PCAMVIB_SideVibration] = sideVib;
	mCurrEventIndex                         = -1;
}

/**
 * @todo: Documentation
 */
void PcamCameraManager::startCamera(Creature* target)
{
	mCamera->startCamera(target);
}

/**
 * @todo: Documentation
 */
void PcamCameraManager::update()
{
	mCamera->control(*mController);
	mCamera->update();
	updateVibrationEvent();
#if defined(PIKI_PC_PORT)
	// Netplay M2c test hook (issue #879): det-mode-only presentation yaw
	// wobble. PIKMIN_NETPLAY_TEST_CAMERA_WOBBLE=<degrees> rotates the local
	// presentation camera's yaw basis sinusoidally with a 97-tick period.
	// It never changes sim inputs during replay: det-mode Navis build
	// their stick basis from the recorded input yaw, not from this camera.
	// On the base exe (camera-read control) the same wobble diverges navi
	// movement early, which is the control experiment proving yaw-as-input.
	if (pc_netplay_deterministic()) {
		const char* wobEnv = std::getenv("PIKMIN_NETPLAY_TEST_CAMERA_WOBBLE");
		if (wobEnv != nullptr && *wobEnv != '\0') {
			const double deg = std::atof(wobEnv);
			if (deg != 0.0) {
				const double phase  = 6.283185307179586 * (double)pc_netplay_tick() / 97.0;
				const double wobRad = deg * 3.141592653589793 / 180.0 * std::sin(phase);
				const float s       = (float)std::sin(wobRad);
				const float c       = (float)std::cos(wobRad);
				Camera* cam         = (mCamera != nullptr) ? mCamera->mCamera : nullptr;
				if (cam != nullptr && (s != 0.0f || c != 1.0f)) {
					// RotY(wobble) under the engine's row convention
					// (v' = M v, rows (c,0,-s)/(s,0,c)): absolute per-tick
					// offset, so update()'s rebuild each tick means no drift.
					const Vector3f x = cam->mViewXAxis;
					const Vector3f z = cam->mViewZAxis;
					cam->mViewXAxis.set(c * x.x - s * x.z, x.y, s * x.x + c * x.z);
					cam->mViewZAxis.set(c * z.x - s * z.z, z.y, s * z.x + c * z.z);
				}
			}
		}
	}
#endif
}

/**
 * @todo: Documentation
 * @note UNUSED Size: 000024
 */
void PcamCameraManager::startMotion(PcamMotionInfo& info)
{
	mCamera->startMotion(info);
}

/**
 * @todo: Documentation
 * @note UNUSED Size: 000024
 */
void PcamCameraManager::finishMotion()
{
	mCamera->finishMotion();
}

/**
 * @todo: Documentation
 */
void PcamCameraManager::updateVibrationEvent()
{
	if (mCurrEventIndex < 0) {
		return;
	}

	PeveEvent* event = mVibrationEvents[mCurrEventIndex];
	if (event->isFinished()) {
		PRINT_NAKATA("updateVibrationEvent:event->isFinished:%08x\n", event);
		event->finish();
		mCurrEventIndex = PCAMVIB_NULL;
	} else {
		event->update();
	}
}

/**
 * @todo: Documentation
 */
#if defined(PIKI_PC_PORT)
PcamCameraManager* cameraMgrP2 = nullptr;
PcamCameraManager* cameraMgrP1 = nullptr;
#endif

#if defined(PIKI_PC_PORT)
void PcamCameraManager::startVibrationEvent(int eventIdx, immut Vector3f& p2, bool mirror)
#else
void PcamCameraManager::startVibrationEvent(int eventIdx, immut Vector3f& p2)
#endif
{
	PRINT("startVibrationEvent:%d,%d\n", mCurrEventIndex, eventIdx);
#if defined(PIKI_PC_PORT)
	if (mirror && cameraMgrP2 && this != cameraMgrP2 && this == cameraMgr) {
		cameraMgrP2->startVibrationEvent(eventIdx, p2, false);
	}
#endif
	if (mCurrEventIndex < 0 || mCurrEventIndex >= eventIdx) {
		NVector3f vec1;
		outputNaviPosition(vec1);
		f32 dist = vec1.distanceXZ(p2);
		if (dist > mCamera->getParameterF(PCAMF_VibrationDistance)) {
			PRINT("startVibrationEvent:distance>:%f\n", dist);
		} else {
			mCurrEventIndex  = eventIdx;
			PeveEvent* event = mVibrationEvents[mCurrEventIndex];
			if (mCurrEventIndex == PCAMVIB_Vibration1) {
				static_cast<PcamVibrationEvent*>(event)->makePcamVibrationEvent();
			} else if (mCurrEventIndex == PCAMVIB_Vibration2) {
				static_cast<PcamVibrationEvent*>(event)->makePcamVibrationEvent();
			} else if (mCurrEventIndex == PCAMVIB_LongVibration) {
				static_cast<PcamLongVibrationEvent*>(event)->makePcamLongVibrationEvent(0.4f, 0.6f, 0.2f, 3.0f);
			} else if (mCurrEventIndex == PCAMVIB_Damage) {
				static_cast<PcamDamageEvent*>(event)->makePcamDamageEvent();
			} else if (mCurrEventIndex == PCAMVIB_SideVibration) {
				static_cast<PcamSideVibrationEvent*>(event)->makePcamSideVibrationEvent();
			}
			event->reset();
		}
	}
}

/**
 * @todo: Documentation
 */
void PcamCameraManager::outputNaviPosition(Vector3f& naviPos)
{
	Navi* navi = naviMgr->getActiveNavi();
	if (!navi) navi = naviMgr->getNavi(0);
	naviPos.input(navi->getPosition());
}
