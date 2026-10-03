#pragma once
#include "pc_p2_equipment_policy.h"
// Queries accepted receipts from the selected campaign; inactive in P1/AP,
// Challenge and Versus. No acquisition/persistence is performed by these calls.
bool pc_p2_equipment_has(p2equipment::Item item);
float pc_p2_equipment_damage(float original);
float pc_p2_equipment_whistle(float original);
float pc_p2_equipment_speed(float original);
// Apply source map side effects after accepted delivery or authenticated restore.
void pc_p2_equipment_reconcile_courses();
