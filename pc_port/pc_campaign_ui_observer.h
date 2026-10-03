#ifndef PC_CAMPAIGN_UI_OBSERVER_H
#define PC_CAMPAIGN_UI_OBSERVER_H

// Save eligibility is supported only for VERSION_GPIE01_01.
// Value snapshots only. Call on the engine thread between updates. Never a card-commit oracle.
struct PcPauseSnapshot {
    bool available = false;
    int state = -1;
    int mainState = -1;
    int mainSelection = -1;
    int subState = -1;
    int subSelection = -1;
    bool mainInputReady = false;
    bool sunsetInputReady = false;
};
// Copied from the actual outer memory-check/default-file update path only.
struct PcDefaultFileSnapshot {
    bool available = false;
    int memoryState = -1;
    int state = -1;
    bool successful = false;
    bool typingComplete = false;
    bool confirmationReady = false;
};
struct PcSaveUiSnapshot {
    bool available = false;
    int resultState = -1;
    int saveState = -1;
    bool resultsInputReady = false;
    bool primaryInputReady = false;
    bool primaryYes = false; // meaningful only when primaryInputReady
    bool secondaryInputReady = false;
    bool secondaryYes = false; // secondary prompt is NOT the save confirmation
    bool fileSelection = false;
    bool cardSlotInputReady = false;
    int cardSlot = -1; // meaningful only when cardSlotInputReady; save-mode selector only
    bool nestedUiBlocked = true;
    bool failureAvailable = false;
    bool failureInactive = false;
    bool fileAvailable = false;
    int fileState = -1;
    bool memoryAvailable = false;
    bool outerMemoryRouted = false;
    PcDefaultFileSnapshot defaultFile;
};
// Read-only day-end diagnostics. Unavailable after gameplay heap teardown.
struct PcDayendMovieSnapshot {
    int movie = -1;
    int scene = -1;
    int playbackMode = -1;
    bool playing = false;
    float sceneFrame = 0;
    float playbackTime = 0;
    float speed = 0;
};
struct PcDayendSnapshot {
    bool available = false;
    // 0 unknown, 1 intro, 2 running, 3 quitting, 4 message, 5 day-over.
    int mode = 0;
    int nextMode = 0;
    int dayOverPhase = -1;
    bool tutorial = false;
    bool pauseAll = false;
    bool movieAvailable = false;
    bool movieActive = false;
    bool moviePaused = false;
    int movieFrame = -1;
    int movieCount = 0;
    bool moviesTruncated = false;
    PcDayendMovieSnapshot movies[4];
    float currentFade = 0;
    float targetFade = 0;
    float fadeSpeed = 0;
    unsigned updateFlags = 0;
    unsigned long long modeUpdates = 0;
    unsigned long long postUpdates = 0;
};
PcDayendSnapshot pc_dayend_observe();
PcPauseSnapshot pc_pause_observe();
PcSaveUiSnapshot pc_save_ui_observe();
#endif
