/*
 * Team and league frames on the console HUD, and stepping through them from the
 * pad.
 */
#include "UI/uiConsoleHudInternal.h"

static const char *shortButtonName(int button)
{
	static const char *names[GP_BUTTON_COUNT] = {
		"A",	 "B",	     "X",	   "Y",		 "Back",
		"Start", "L3",	     "R3",	   "LB",	 "RB",
		"Up",	 "Down",     "Left",	   "Right",	 "LT",
		"RT",	 "Stick up", "Stick down", "Stick left", "Stick right",
	};
	return button >= 0 && button < GP_BUTTON_COUNT ? names[button] : "";
}

// The chord bound to "team_select_other n" in the keybinds, written as LB + A
// and the like. Empty if none.
static void teamSlotShortcut(int slot, char *out, size_t outSize)
{
	char wanted[32];
	int modifiers, button;

	out[0] = 0;
	snprintf(wanted, sizeof(wanted), "team_select_other %d", slot);

	for (modifiers = 0; modifiers < 16; modifiers++) {
		for (button = 0; button < GP_BUTTON_COUNT; button++) {
			const char *command =
				gamepadBoundCommand(modifiers, button);

			if (!command || stricmp(command, wanted))
				continue;

			snprintf_s(out, outSize, outSize - 1, "%s%s%s%s%s",
				   (modifiers & GP_MOD_LT) ? "LT + " : "",
				   (modifiers & GP_MOD_RT) ? "RT + " : "",
				   (modifiers & GP_MOD_LB) ? "LB + " : "",
				   (modifiers & GP_MOD_RB) ? "RB + " : "",
				   shortButtonName(button));
			return;
		}
	}
}

// Returns how many icons it drew.
static int drawTeamBuffs(Entity *mate, F32 x, F32 y, F32 hudScale)
{
	const BasePower *shown[TEAM_BUFF_MAX];
	const BasePower **buffs =
		basePowersFromPowerBuffs((void **)mate->ppowBuffs);
	int count = 0;
	int i, j;

	for (i = 0; i < eaSize(&buffs) && count < TEAM_BUFF_MAX; i++) {
		const BasePower *buff = buffs[i];
		AtlasTex *icon;
		F32 size = TEAM_BUFF_SIZE * hudScale;
		int repeat = 0;

		if (!buff || buff->eType == kPowerType_Auto)
			continue;
		for (j = 0; j < count; j++)
			repeat |= shown[j] == buff;
		if (repeat)
			continue;

		icon = basepower_GetIcon(buff);
		if (!icon)
			continue;
		display_sprite(icon, x + count * (size + 2 * hudScale), y,
			       HUD_Z + 6, size / icon->width,
			       size / icon->height, CLR_WHITE);
		shown[count++] = buff;
	}
	return count;
}

// The pet window opens on the left, where the team frames go, and a
// mastermind's pets covered the team. When it would sit over the room a full
// team takes, it moves to the right side. Anywhere else the player put it, it
// stays.
void keepPetsOffTeam(HudFrame team, F32 teamHeight, int screenWidth,
		     F32 hudScale)
{
	F32 x, y, z, wd, ht, sc;
	F32 fullTeam = (MAX_TEAM_MEMBERS - 1) *
		       (TEAM_FRAME_HEIGHT + TEAM_FRAME_GAP + TEAM_BUFF_SIZE) *
		       team.scale;
	F32 right = team.x + TEAM_FRAME_WIDTH * team.scale;
	F32 bottom = team.y + (teamHeight > fullTeam ? teamHeight : fullTeam);
	int color, back;

	if (window_getMode(WDW_PET) != WINDOW_DISPLAYING ||
	    !window_getDims(WDW_PET, &x, &y, &z, &wd, &ht, &sc, &color, &back))
		return;
	if (x < right && x + wd > team.x && y < bottom && y + ht > team.y)
		window_setDims(WDW_PET, screenWidth - wd - 20 * hudScale,
			       TEAM_FRAME_TOP * hudScale, -1, -1);
}

HudMember hudMembers[HUD_MEMBER_MAX];
int hudMemberCount;
HudMember hudMembersShown[HUD_MEMBER_MAX];
int hudMembersShownCount;

static void hudAddMember(TeamMembers *members, int index, F32 x, F32 y)
{
	if (hudMemberCount < HUD_MEMBER_MAX) {
		hudMembers[hudMemberCount].members = members;
		hudMembers[hudMemberCount].index = index;
		hudMembers[hudMemberCount].x = x;
		hudMembers[hudMemberCount].y = y;
		hudMemberCount++;
	}
}

void consoleTeamStep(const char *direction)
{
	Entity *e = playerPtr();
	int dx = 0, dy = 0;
	int from = -1, best = -1;
	F32 bestScore = 0;
	int i;

	if (!e || !direction)
		return;
	if (!stricmp(direction, "up"))
		dy = -1;
	else if (!stricmp(direction, "down"))
		dy = 1;
	else if (!stricmp(direction, "left"))
		dx = -1;
	else if (!stricmp(direction, "right"))
		dx = 1;

	// Off the HUD, or with nobody shown, the first teammate as before.
	if (!consoleHudActive() || !hudMembersShownCount) {
		team_select_other(1);
		return;
	}

	for (i = 0; i < hudMembersShownCount; i++) {
		HudMember *m = &hudMembersShown[i];
		int id = m->members->ids[m->index];
		Entity *mate = entFromDbId(id);

		if ((mate && current_target == mate) ||
		    (!mate && gSelectedDBID == id))
			from = i;
	}

	if (from < 0) {
		// Nobody picked yet: the first one in the lists that is not
		// you.
		for (i = 0; i < hudMembersShownCount; i++)
			if (hudMembersShown[i]
				    .members->ids[hudMembersShown[i].index] !=
			    e->db_id) {
				best = i;
				break;
			}
	} else {
		HudMember *here = &hudMembersShown[from];

		for (i = 0; i < hudMembersShownCount; i++) {
			HudMember *m = &hudMembersShown[i];
			F32 along =
				(m->x - here->x) * dx + (m->y - here->y) * dy;
			F32 sideways = dx ? fabsf(m->y - here->y)
					  : fabsf(m->x - here->x);
			F32 score;

			if (i == from || along <= 1 ||
			    m->members->ids[m->index] == e->db_id)
				continue;
			score = along + sideways * 3;
			if (best < 0 || score < bestScore) {
				best = i;
				bestScore = score;
			}
		}
	}

	if (best >= 0)
		team_select(hudMembersShown[best].members,
			    hudMembersShown[best].index + 1);
}

// The other teams of a league, each a block of names and health a piece of its
// own, so it can be moved, sized or hidden in the edit mode. Your own team is
// the team frames.
#define LEAGUE_BLOCK_WIDTH 170.0f
#define LEAGUE_ROW_HEIGHT 22.0f
#define LEAGUE_HEADER_HEIGHT 22.0f
#define LEAGUE_BLOCK_GAP 12.0f

static int leagueOwnTeam(Entity *e)
{
	League *league = e->league;
	int t, k;

	for (t = 0; t < eaSize(&league->teamRoster); t++)
		for (k = 0; k < eaiSize(&league->teamRoster[t]); k++) {
			int index = league->teamRoster[t][k];

			if (index >= 0 && index < league->members.count &&
			    league->members.ids[index] == e->db_id)
				return t;
		}
	return -1;
}

// A team as a short list: the members of `members` at `roster`. Returns the
// height drawn.
F32 drawMemberBlock(Entity *e, TeamMembers *members, const int *roster,
		    int count, int team, F32 left, F32 top, F32 scale)
{
	static const int teamColors[] = { CLR_MAGENTA, CLR_YELLOW, CLR_CYAN,
					  CLR_RED,     CLR_ORANGE, CLR_NAVY };
	F32 width = LEAGUE_BLOCK_WIDTH * scale;
	F32 rowHeight = LEAGUE_ROW_HEIGHT * scale;
	F32 header = LEAGUE_HEADER_HEIGHT * scale;
	F32 height = header + count * rowHeight + 6 * scale;
	char title[32];
	int k;

	drawFlatFrame(PIX2, 8, left, top, HUD_Z, width, height, scale,
		      FRAME_COLOR, BACK_COLOR);
	display_sprite(white_tex_atlas, left + 3 * scale, top + 4 * scale,
		       HUD_Z + 2, 4 * scale / white_tex_atlas->width,
		       (height - 8 * scale) / white_tex_atlas->height,
		       teamColors[team % ARRAY_SIZE(teamColors)]);
	if (e->league && members == &e->league->members)
		snprintf(title, sizeof(title), "Team %d", team + 1);
	else
		snprintf(title, sizeof(title), "Team");
	drawText(&game_12, left + 14 * scale, top + header / 2 + 2 * scale,
		 HUD_Z + 6, scale, 0xffffffc0, CENTER_Y, title);

	for (k = 0; k < count; k++) {
		int index = roster[k];
		Entity *mate;
		int onMap, targeted, defeated;
		F32 y = top + header + k * rowHeight;
		CBox box;

		if (index < 0 || index >= members->count)
			continue;
		mate = entFromDbId(members->ids[index]);
		hudAddMember(members, index, left + width / 2,
			     y + rowHeight / 2);
		onMap = mate && mate->pchar && members->onMapserver &&
			members->onMapserver[index];
		defeated = onMap && mate->pchar->attrCur.fHitPoints <= 0;
		targeted = (mate && current_target == mate) ||
			   (!mate && gSelectedDBID == members->ids[index]);

		// A click picks them out as a teammate's frame does; a right
		// click opens their menu.
		BuildCBox(&box, left + 10 * scale, y, width - 14 * scale,
			  rowHeight - 2 * scale);
		if (mouseCollision(&box)) {
			int leftClick = mouseClickHit(&box, MS_LEFT);
			int rightClick = mouseClickHit(&box, MS_RIGHT);

			cursorOverHud = 1;
			if (leftClick || rightClick) {
				team_select(members, index + 1);
				if (rightClick) {
					if (current_target)
						contextMenuForTarget(
							current_target);
					else
						contextMenuForOffMap();
				}
			}
		}
		if (targeted)
			drawFlatFrame(PIX2, 4, left + 9 * scale, y, HUD_Z + 1,
				      width - 12 * scale, rowHeight - 2 * scale,
				      scale, FRAME_COLOR_HELD, BACK_COLOR_HELD);
		else if (mate == e)
			drawFlatFrame(PIX2, 4, left + 9 * scale, y, HUD_Z + 1,
				      width - 12 * scale, rowHeight - 2 * scale,
				      scale, 0xffffff60, 0xffffff18);

		drawText(&game_9, left + 14 * scale, y + 7 * scale, HUD_Z + 6,
			 scale, onMap && !defeated ? CLR_WHITE : 0xffffff80,
			 CENTER_Y, mate ? mate->name : members->names[index]);
		if (onMap) {
			F32 hp = mate->pchar->attrCur.fHitPoints,
			    hpMax = mate->pchar->attrMax.fHitPoints;
			F32 share = hpMax > 0 ? hp / hpMax : 0;
			F32 barWidth = width - 26 * scale;

			if (share < 0)
				share = 0;
			if (share > 1)
				share = 1;
			display_sprite(white_tex_atlas, left + 14 * scale,
				       y + 14 * scale, HUD_Z + 3,
				       barWidth / white_tex_atlas->width,
				       4 * scale / white_tex_atlas->height,
				       0x000000c0);
			display_sprite(
				white_tex_atlas, left + 14 * scale,
				y + 14 * scale, HUD_Z + 4,
				barWidth * share / white_tex_atlas->width,
				4 * scale / white_tex_atlas->height,
				getHealthColor(
					entity_TargetIsInVisionPhase(e, mate),
					share));
		}
	}
	return height;
}

void drawLeagueTeams(Entity *e, int screenWidth, int screenHeight, F32 hudScale)
{
	F32 blockStep =
		(LEAGUE_HEADER_HEIGHT + MAX_TEAM_MEMBERS * LEAGUE_ROW_HEIGHT +
		 6 + LEAGUE_BLOCK_GAP) *
		HUD_TEAM_DEFAULT_SIZE;
	F32 columnStep =
		(LEAGUE_BLOCK_WIDTH + LEAGUE_BLOCK_GAP) * HUD_TEAM_DEFAULT_SIZE;
	int framesUp =
		gamepadConsoleLeagueTeam() && !hudLayout[HUD_PIECE_TEAM].hidden;
	// Down the left where the team frames would be, or beside them when
	// they are kept.
	F32 firstColumn =
		framesUp ? 20 + TEAM_FRAME_WIDTH * HUD_TEAM_DEFAULT_SIZE + 16
			 : 20;
	int own, slot = 0;
	int t;

	if (!e->league || e->league->members.count < 2 ||
	    !e->league->teamRoster)
		return;
	own = leagueOwnTeam(e);

	for (t = 0; t < eaSize(&e->league->teamRoster) && t < MAX_LEAGUE_TEAMS;
	     t++) {
		int piece = HUD_PIECE_LEAGUE_1 + t;
		HudFrame place;
		F32 blockHeight;

		if ((framesUp && t == own) ||
		    !eaiSize(&e->league->teamRoster[t]))
			continue;

		// By default down the left beside your own team, three to a
		// column.
		place = hudPlace(
			piece,
			(firstColumn + (slot / 3) * columnStep) * hudScale,
			(TEAM_FRAME_TOP + (slot % 3) * blockStep) * hudScale,
			hudScale, screenWidth, screenHeight);
		slot++;
		if (hudHidden(piece))
			continue;
		blockHeight = drawMemberBlock(
			e, &e->league->members, e->league->teamRoster[t],
			eaiSize(&e->league->teamRoster[t]), t, place.x, place.y,
			place.scale);
		hudBoxes[piece].height = blockHeight / place.scale;
	}
}

// Drawn down from left, top. Returns the height drawn.
F32 drawTeamFrames(Entity *e, F32 left, F32 top, F32 hudScale)
{
	TeamMembers *members;
	F32 x = left;
	F32 y = top;
	F32 width = TEAM_FRAME_WIDTH * hudScale;
	F32 height = TEAM_FRAME_HEIGHT * hudScale;
	int slot = 0;
	int i;

	if (!e->teamup || e->teamup->members.count < 2)
		return 0;
	members = &e->teamup->members;

	for (i = 0; i < members->count && i < MAX_TEAM_MEMBERS; i++) {
		Entity *mate;
		int targeted, onMap, defeated;
		int buffCount = 0;
		int frameColor;
		F32 barX = x + 34 * hudScale;
		F32 barWidth = width - 44 * hudScale;
		char shortcut[48];
		char level[8];
		CBox box;

		if (members->ids[i] == e->db_id)
			continue;
		slot++;

		mate = entFromDbId(members->ids[i]);
		onMap = mate && mate->pchar && members->onMapserver &&
			members->onMapserver[i];
		defeated = onMap && mate->pchar->attrCur.fHitPoints <= 0;
		targeted = (mate && current_target == mate) ||
			   (!mate && gSelectedDBID == members->ids[i]);
		frameColor = targeted ? FRAME_COLOR_HELD : FRAME_COLOR;

		drawFlatFrame(PIX2, 8, x, y, HUD_Z, width, height, hudScale,
			      frameColor,
			      targeted ? BACK_COLOR_HELD : BACK_COLOR);
		hudAddMember(members, i, x + width / 2, y + height / 2);

		// Clicking a frame targets that teammate; a right click opens
		// their menu, as the Team window did.
		BuildCBox(&box, x, y, width, height);
		if (mouseCollision(&box)) {
			int leftClick = mouseClickHit(&box, MS_LEFT);
			int rightClick = mouseClickHit(&box, MS_RIGHT);

			cursorOverHud = 1;
			if (leftClick || rightClick) {
				team_select(members, i + 1);
				if (rightClick) {
					if (current_target)
						contextMenuForTarget(
							current_target);
					else
						contextMenuForOffMap();
				}
			}
		}

		if (onMap) {
			AtlasTex *icon = GetArchetypeIcon(mate);

			if (icon && icon != white_tex_atlas) {
				F32 iconScale = 26 * hudScale / icon->width;

				display_sprite(icon, x + 6 * hudScale,
					       y + 6 * hudScale, HUD_Z + 4,
					       iconScale, iconScale,
					       defeated ? 0xffffff60
							: CLR_WHITE);
			}
			snprintf(level, sizeof(level), "%d",
				 character_CalcExperienceLevel(mate->pchar) +
					 1);
			drawText(&game_9, x + 19 * hudScale,
				 y + height - 12 * hudScale, HUD_Z + 6,
				 hudScale, 0xffffffc0, CENTER_X | CENTER_Y,
				 level);
		}

		drawText(&game_12, barX, y + 11 * hudScale, HUD_Z + 6, hudScale,
			 onMap && !defeated ? CLR_WHITE : 0xffffff90, CENTER_Y,
			 mate ? mate->name : members->names[i]);

		if (members->leader == members->ids[i]) {
			AtlasTex *star =
				atlasLoadTexture("MissionPicker_icon_star.tga");
			F32 starScale = 14 * hudScale / star->width;

			display_sprite(star, x + width - 20 * hudScale,
				       y + 4 * hudScale, HUD_Z + 6, starScale,
				       starScale, FRAME_COLOR_HELD);
		}

		if (!onMap) {
			drawText(&game_9, barX, y + 30 * hudScale, HUD_Z + 6,
				 hudScale, 0xffffff90, CENTER_Y,
				 textStd(teamMapNames[i]));
		} else if (defeated) {
			drawText(&game_12, barX, y + 30 * hudScale, HUD_Z + 6,
				 hudScale, COLOR_ENEMY, CENTER_Y, "Defeated");
		} else {
			F32 hp = mate->pchar->attrCur.fHitPoints,
			    hpMax = mate->pchar->attrMax.fHitPoints;
			F32 endurance = mate->pchar->attrCur.fEndurance,
			    enduranceMax = mate->pchar->attrMax.fEndurance;

			drawHealthBarStill(
				barX, y + 20 * hudScale, HUD_Z + 4, barWidth,
				hp, hpMax > 0 ? hpMax : 1, hudScale * 0.8f,
				frameColor,
				getHealthColor(
					entity_TargetIsInVisionPhase(e, mate),
					hpMax > 0 ? hp / hpMax : 0),
				HEALTHBAR_HEALTH);
			drawHealthBarStill(barX, y + 32 * hudScale, HUD_Z + 4,
					   barWidth, endurance,
					   enduranceMax > 0 ? enduranceMax : 1,
					   hudScale * 0.8f, frameColor,
					   0x3c7cffff, HEALTHBAR_ENDURANCE);
			buffCount = drawTeamBuffs(mate, barX,
						  y + height + 2 * hudScale,
						  hudScale);
		}

		teamSlotShortcut(slot, shortcut, sizeof(shortcut));
		if (shortcut[0])
			drawText(&game_9,
				 x + width - 8 * hudScale -
					 str_wd_notranslate(&game_9, hudScale,
							    hudScale, shortcut),
				 y + height - 9 * hudScale, HUD_Z + 6, hudScale,
				 0xffffffa0, CENTER_Y, shortcut);

		y += height + TEAM_FRAME_GAP * hudScale +
		     (buffCount ? TEAM_BUFF_SIZE * hudScale : 0);
	}

	// In a league, a line under the team opens the League tab.
	if (e->league_id && e->league) {
		char line[48];
		CBox box;

		snprintf(line, sizeof(line), "League: %d teams",
			 eaSize(&e->league->teamRoster));
		BuildCBox(&box, x, y, width, 24 * hudScale);
		if (mouseClickHit(&box, MS_LEFT)) {
			char open[] = "console_nav_tab league";
			cmdParse(open);
		}
		drawFlatFrame(PIX2, 8, x, y, HUD_Z, width, 24 * hudScale,
			      hudScale, FRAME_COLOR_DIM, BACK_COLOR);
		drawText(&game_12, x + 10 * hudScale, y + 12 * hudScale,
			 HUD_Z + 6, hudScale, 0xffffffc0, CENTER_Y, line);
		y += 24 * hudScale;
	}
	return y - top;
}

static const NavTab navigationTabs[] = {
	{ "Map", "map", WDW_MAP },
	{ "Missions", "missions", WDW_MISSION, 1 },
	{ "Contacts", "contacts", WDW_CONTACT, 1 },
	{ "Clues", "clues", WDW_CLUE, 1 },
	{ "Badges", "badges", WDW_BADGES, 1 },
};

static const NavTab socialTabs[] = {
	{ "Search", "search", WDW_LFG },
	{ "Team", "team", WDW_GROUP },
	{ "Friends", "friends", WDW_FRIENDS, 1 },
	{ "League", "league", WDW_LEAGUE },
	{ "Supergroup", "supergroup", WDW_SUPERGROUP },
};

const NavScreen navScreens[NAV_SCREEN_COUNT] = {
	{ "Navigation", navigationTabs, ARRAY_SIZE(navigationTabs) },
	{ "Social", socialTabs, ARRAY_SIZE(socialTabs) },
};

int navScreen;
int navTab;
// The tab each screen was last left on, so it opens there again.
int navLastTab[NAV_SCREEN_COUNT];
U8 navReopen[NAV_TAB_MAX];
int navFocusFrames;
F32 navMapScale = 1.0f;
F32 navContentWidth, navContentHeight;
