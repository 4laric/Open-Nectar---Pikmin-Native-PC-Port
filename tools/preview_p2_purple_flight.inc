#include "pc_p2_purple_direct.h"
#include "pc_p2_purple_feedback.h"
#include "pc_p2_purple_flight.h"
#include "pc_p2_purple.h"

static void purpleFlightFixture(Navi* n)
{
    static int state = 0, timer = 0;
    static Piki* source = nullptr;
    static BTeki* adult = nullptr;
    static Vector3f adultPosition;
    static float pauseY = 0.0f, adultHealth = 0.0f;
    static bool sawAscent = false, sawPause = false, sawDescent = false, sawRecovery = false;
    ++timer;

    auto throwTo = [&](const Vector3f& target) {
        source->changeMode(PikiMode::FreeMode, n);
        source->mFSM->transit(source, PIKISTATE_Flying);
        n->throwPiki(source, target);
        timer = 0;
    };

    if (state == 0 && timer >= 90) {
        int reds = 0;
        Iterator pikis(pikiMgr);
        CI_LOOP(pikis) {
            Piki* piki = static_cast<Piki*>(*pikis);
            if (!piki->isAlive()) continue;
            if (piki->mColor == Red) ++reds;
            if (!source) source = piki;
            else piki->resetPosition(Vector3f(-220.0f, mapMgr->getMinY(-220.0f, -180.0f, true), -180.0f));
        }
        Iterator enemies(tekiMgr);
        CI_LOOP(enemies) {
            BTeki* teki = static_cast<BTeki*>(*enemies);
            if (pc_p2_purple_direct_adult_registered(teki)) {
                require(!adult, "multiple Purple flight adult receivers");
                adult = teki;
            }
        }
        require(reds == 20 && source && adult, "fresh 20-red flight arena actors missing");
        pc_p2_make_purple(source);
        adultPosition = adult->mSRT.t;
        n->resetPosition(Vector3f(-175.0f, mapMgr->getMinY(-175.0f, -100.0f, true), -100.0f));
        throwTo(Vector3f(-85.0f, mapMgr->getMinY(-85.0f, 0.0f, true), 0.0f));
        state = 1;
        std::puts("P2_PURPLE_FLIGHT_FIXTURE_READY fresh_reds=20 window=960x540 centered throw=physical");
    } else if (state == 1) {
        PcP2PurpleFlightSample sample = pc_p2_purple_flight_sample(source);
        if (sample.phase == PcP2PurpleFlightPhase::Ascent) sawAscent = true;
        if (sample.phase == PcP2PurpleFlightPhase::EntryPause) {
            if (!sawPause) { pauseY = source->mSRT.t.y; sawPause = true; }
            require(std::fabs(source->mSRT.t.y - pauseY) < 1.0f, "entry pause drifted vertically");
        }
        if (sample.phase == PcP2PurpleFlightPhase::Descent) {
            sawDescent = true;
            require(sample.motionElapsed < 3.0f, "descent motion clock invalid");
        }
        if (sample.phase == PcP2PurpleFlightPhase::Recovery && !sawRecovery) {
            sawRecovery = true;
            capture("p2-purple-flight-impact.ppm");
        }
        if (sawRecovery && sample.phase == PcP2PurpleFlightPhase::None) {
            require(sawAscent && sawPause && sawDescent, "ground throw skipped a HipDrop phase");
            PcP2PurpleFeedbackStats feedback = pc_p2_purple_feedback_stats();
            require(feedback.entries == 1 && feedback.impacts == 1 && feedback.trailBursts >= 1,
                "ground feedback counts invalid");
            std::printf("P2_PURPLE_FLIGHT_GROUND_PASS entries=%u trail=%u impacts=%u sound=%u camera=%u rumble=%u\n",
                feedback.entries, feedback.trailBursts, feedback.impacts, feedback.soundRequests,
                feedback.cameraRequests, feedback.rumbleRequests);
            adultHealth = adult->mHealth;
            adult->resetPosition(adultPosition);
            adult->mVelocity.set(0.0f, 0.0f, 0.0f);
            adult->mTargetVelocity = adult->mVelocity;
            n->resetPosition(Vector3f(adultPosition.x - 70.0f,
                mapMgr->getMinY(adultPosition.x - 70.0f, adultPosition.z, true), adultPosition.z));
            throwTo(adultPosition);
            state = 2;
        }
        require(timer < 360, "physical ground HipDrop did not recover");
    } else if (state == 2) {
        adult->resetPosition(adultPosition);
        adult->mVelocity.set(0.0f, 0.0f, 0.0f);
        adult->mTargetVelocity = adult->mVelocity;
        if (adult->mStoredDamage >= 49.9f || adult->mHealth < adultHealth - 49.0f) {
            PcP2PurpleFeedbackStats feedback = pc_p2_purple_feedback_stats();
            require(feedback.entries == 2 && feedback.impacts == 2, "enemy feedback was not exactly once");
            require(!pc_p2_purple_flight_active(source), "enemy collision did not clean flight state");
            std::printf("PASS p2 Purple flight: physical ground recovery and adult direct collision health=%.1f stored=%.1f feedback=%u/%u/%u\n",
                adult->mHealth, adult->mStoredDamage, feedback.entries, feedback.trailBursts, feedback.impacts);
            std::fflush(stdout);
            std::_Exit(0);
        }
        require(timer < 360, "physical adult HipDrop did not collide");
    }
}
